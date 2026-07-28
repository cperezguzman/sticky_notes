/**
 * Auth routes for both storage modes.
 *
 * Cloud (`DATABASE_URL`): signup / login / verify / logout / me
 * Files (`STICKY_STORAGE=files`): single-user login from notes/auth.json
 */

import { Router, type Request, type Response } from "express";

import type { AuthRecord } from "../auth/store.js";
import { isAuthEnabled, verifyPassword } from "../auth/store.js";
import { sendVerifyEmail } from "../cloud/email.js";
import type { UserStore } from "../cloud/userStore.js";

export type AuthMode = "cloud" | "files";

function appOrigin(): string {
  return (process.env.APP_ORIGIN ?? "http://127.0.0.1:5173").replace(/\/$/, "");
}

function isValidEmail(email: string): boolean {
  return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email);
}

export function createAuthRouter(options: {
  mode: AuthMode;
  fileAuth?: AuthRecord;
  users?: UserStore;
}): Router {
  const router = Router();
  const { mode, fileAuth, users } = options;

  router.get("/me", async (req: Request, res: Response) => {
    if (!isAuthEnabled()) {
      res.json({ authRequired: false, user: null, email: null, storage: mode });
      return;
    }
    if (mode === "cloud") {
      if (!req.session?.userId) {
        res.status(401).json({ error: "unauthorized", authRequired: true, storage: mode });
        return;
      }
      res.json({
        authRequired: true,
        user: req.session.email ?? null,
        email: req.session.email ?? null,
        userId: req.session.userId,
        storage: mode,
      });
      return;
    }
    if (!req.session?.user) {
      res.status(401).json({ error: "unauthorized", authRequired: true, storage: mode });
      return;
    }
    res.json({
      authRequired: true,
      user: req.session.user,
      email: null,
      storage: mode,
    });
  });

  router.post("/signup", async (req: Request, res: Response) => {
    if (mode !== "cloud" || !users) {
      res.status(400).json({ error: "signup only available in cloud mode" });
      return;
    }
    const email = typeof req.body?.email === "string" ? req.body.email.trim() : "";
    const password = typeof req.body?.password === "string" ? req.body.password : "";
    if (!isValidEmail(email)) {
      res.status(400).json({ error: "valid email required" });
      return;
    }
    if (password.length < 8) {
      res.status(400).json({ error: "password must be at least 8 characters" });
      return;
    }
    const existing = await users.findByEmail(email);
    if (existing) {
      if (existing.emailVerifiedAt) {
        res.status(409).json({ error: "email already registered" });
        return;
      }
      // Unverified: issue a fresh link instead of blocking the user
      const token = await users.createVerifyToken(existing.id);
      const verifyUrl = `${appOrigin()}/?verify=${encodeURIComponent(token)}`;
      try {
        await sendVerifyEmail(existing.email, verifyUrl);
      } catch (err) {
        console.error(err);
        res.status(502).json({ error: "could not send verification email" });
        return;
      }
      res.status(200).json({
        ok: true,
        message: "account pending verification — we sent a new link",
        email: existing.email,
        resent: true,
      });
      return;
    }
    const user = await users.createUser(email, password);
    const token = await users.createVerifyToken(user.id);
    const verifyUrl = `${appOrigin()}/?verify=${encodeURIComponent(token)}`;
    try {
      await sendVerifyEmail(user.email, verifyUrl);
    } catch (err) {
      console.error(err);
      res.status(502).json({ error: "could not send verification email" });
      return;
    }
    res.status(201).json({
      ok: true,
      message: "check your email to verify your account",
      email: user.email,
    });
  });

  router.post("/resend-verification", async (req: Request, res: Response) => {
    if (mode !== "cloud" || !users) {
      res.status(400).json({ error: "resend only available in cloud mode" });
      return;
    }
    const email = typeof req.body?.email === "string" ? req.body.email.trim() : "";
    if (!isValidEmail(email)) {
      res.status(400).json({ error: "valid email required" });
      return;
    }
    const user = await users.findByEmail(email);
    if (!user) {
      res.status(404).json({ error: "no account with that email" });
      return;
    }
    if (user.emailVerifiedAt) {
      res.status(400).json({ error: "email already verified — sign in" });
      return;
    }
    const token = await users.createVerifyToken(user.id);
    const verifyUrl = `${appOrigin()}/?verify=${encodeURIComponent(token)}`;
    try {
      await sendVerifyEmail(user.email, verifyUrl);
    } catch (err) {
      console.error(err);
      res.status(502).json({ error: "could not send verification email" });
      return;
    }
    res.json({
      ok: true,
      message: "verification link resent — check your email (or the server log)",
      email: user.email,
    });
  });

  router.get("/verify", async (req: Request, res: Response) => {
    if (mode !== "cloud" || !users) {
      res.status(400).json({ error: "verify only available in cloud mode" });
      return;
    }
    const token = typeof req.query.token === "string" ? req.query.token : "";
    if (!token) {
      res.status(400).json({ error: "token required" });
      return;
    }
    const user = await users.consumeVerifyToken(token);
    if (!user) {
      res.status(400).json({ error: "invalid or expired token" });
      return;
    }
    res.json({ ok: true, email: user.email, message: "email verified — you can sign in" });
  });

  router.post("/login", async (req: Request, res: Response) => {
    if (!isAuthEnabled()) {
      res.json({ ok: true, authRequired: false, user: null, storage: mode });
      return;
    }

    if (mode === "cloud" && users) {
      const email = typeof req.body?.email === "string" ? req.body.email.trim() : "";
      const password = typeof req.body?.password === "string" ? req.body.password : "";
      // Also accept username field as email alias for older UI
      const identity =
        email ||
        (typeof req.body?.username === "string" ? req.body.username.trim() : "");
      if (!identity || !password) {
        res.status(400).json({ error: "email and password required" });
        return;
      }
      const user = await users.findByEmail(identity);
      if (!user || !(await users.verifyPassword(user, password))) {
        res.status(401).json({ error: "invalid credentials" });
        return;
      }
      if (!user.emailVerifiedAt) {
        res.status(403).json({
          error: "email not verified — resend the verification link",
          code: "email_unverified",
        });
        return;
      }
      req.session.userId = user.id;
      req.session.email = user.email;
      delete req.session.user;
      res.json({
        ok: true,
        authRequired: true,
        user: user.email,
        email: user.email,
        storage: mode,
      });
      return;
    }

    if (!fileAuth) {
      res.status(500).json({ error: "file auth not configured" });
      return;
    }
    const username =
      typeof req.body?.username === "string"
        ? req.body.username
        : typeof req.body?.email === "string"
          ? req.body.email
          : "";
    const password = typeof req.body?.password === "string" ? req.body.password : "";
    if (!username || !password) {
      res.status(400).json({ error: "username and password required" });
      return;
    }
    const ok = await verifyPassword(fileAuth, username, password);
    if (!ok) {
      res.status(401).json({ error: "invalid credentials" });
      return;
    }
    req.session.user = fileAuth.username;
    delete req.session.userId;
    delete req.session.email;
    res.json({
      ok: true,
      authRequired: true,
      user: fileAuth.username,
      storage: mode,
    });
  });

  router.post("/logout", (req: Request, res: Response) => {
    req.session.destroy((err) => {
      if (err) {
        res.status(500).json({ error: "logout failed" });
        return;
      }
      res.clearCookie("sticky.sid");
      res.status(204).send();
    });
  });

  return router;
}
