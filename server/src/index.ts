/**
 * Sticky Notes HTTP API — process entry point.
 *
 * Storage modes:
 * - **cloud** (default when `DATABASE_URL` is set): Postgres users + notes
 * - **files** (`STICKY_STORAGE=files` or no DATABASE_URL): local notes/ + auth.json
 */

import { existsSync } from "node:fs";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";

import connectPgSimple from "connect-pg-simple";
import express from "express";
import session from "express-session";

import {
  isAuthEnabled,
  loadOrCreateAuth,
  loadOrCreateSessionSecret,
} from "./auth/store.js";
import { requireAuth } from "./auth/middleware.js";
import { PostgresNoteRepository } from "./cloud/postgresNoteRepository.js";
import { UserStore } from "./cloud/userStore.js";
import { getPool, resolveStorageMode, runMigrations } from "./db.js";
import { NoteRepository } from "./noteRepository.js";
import { createAuthRouter } from "./routes/auth.js";
import { createCloudNotesRouter, createNotesRouter } from "./routes/notes.js";

const HOST = process.env.HOST ?? "127.0.0.1";
const PORT = Number.parseInt(process.env.PORT ?? "8787", 10);
const NODE_ENV = process.env.NODE_ENV ?? "development";

function defaultNotesDir(): string {
  const serverRoot = fileURLToPath(new URL("..", import.meta.url));
  return resolve(serverRoot, "..", "notes");
}

function webDistPath(): string {
  const serverRoot = fileURLToPath(new URL("..", import.meta.url));
  return resolve(serverRoot, "..", "web", "dist");
}

const notesDir = resolve(process.env.NOTES_DIR ?? defaultNotesDir());

async function main(): Promise<void> {
  const storage = resolveStorageMode();
  if (storage === "cloud" && !process.env.DATABASE_URL) {
    throw new Error("Cloud mode requires DATABASE_URL");
  }

  const authEnabled = isAuthEnabled();
  const app = express();
  app.use(express.json({ limit: "1mb" }));

  const corsOrigins = new Set([
    "http://127.0.0.1:5173",
    "http://localhost:5173",
  ]);
  if (process.env.APP_ORIGIN) {
    corsOrigins.add(process.env.APP_ORIGIN.replace(/\/$/, ""));
  }
  app.use((req, res, next) => {
    const origin = req.headers.origin;
    if (origin && corsOrigins.has(origin)) {
      res.setHeader("Access-Control-Allow-Origin", origin);
      res.setHeader("Access-Control-Allow-Credentials", "true");
      res.setHeader(
        "Access-Control-Allow-Methods",
        "GET,POST,PUT,DELETE,OPTIONS",
      );
      res.setHeader("Access-Control-Allow-Headers", "Content-Type");
    }
    if (req.method === "OPTIONS") {
      res.status(204).end();
      return;
    }
    next();
  });

  let sessionSecret: string;
  if (storage === "cloud") {
    await runMigrations();
    sessionSecret =
      process.env.STICKY_SESSION_SECRET ??
      (await loadOrCreateSessionSecret(notesDir));
    const PgSession = connectPgSimple(session);
    app.use(
      session({
        name: "sticky.sid",
        store: new PgSession({
          pool: getPool(),
          tableName: "session",
          createTableIfMissing: false,
        }),
        secret: sessionSecret,
        resave: false,
        saveUninitialized: false,
        cookie: {
          httpOnly: true,
          sameSite: "lax",
          secure: NODE_ENV === "production",
          maxAge: 7 * 24 * 60 * 60 * 1000,
        },
      }),
    );

    const users = new UserStore(getPool());
    const notes = new PostgresNoteRepository(getPool());
    app.use("/auth", createAuthRouter({ mode: "cloud", users }));
    app.use("/notes", requireAuth, createCloudNotesRouter(notes));
  } else {
    const repo = new NoteRepository(notesDir);
    await repo.ensureDataDir();
    const authRecord = await loadOrCreateAuth(notesDir);
    sessionSecret = await loadOrCreateSessionSecret(notesDir);
    app.use(
      session({
        name: "sticky.sid",
        secret: sessionSecret,
        resave: false,
        saveUninitialized: false,
        cookie: {
          httpOnly: true,
          sameSite: "lax",
          secure: false,
          maxAge: 7 * 24 * 60 * 60 * 1000,
        },
      }),
    );
    app.use("/auth", createAuthRouter({ mode: "files", fileAuth: authRecord }));
    app.use("/notes", requireAuth, createNotesRouter(repo));
  }

  app.get("/health", (_req, res) => {
    res.json({
      ok: true,
      authRequired: authEnabled,
      storage,
    });
  });

  // Production: serve the React build from the same origin (simple cookies).
  const dist = webDistPath();
  if (existsSync(dist)) {
    app.use(express.static(dist));
    app.get("*", (req, res, next) => {
      if (
        req.path.startsWith("/auth") ||
        req.path.startsWith("/notes") ||
        req.path.startsWith("/health")
      ) {
        next();
        return;
      }
      res.sendFile(resolve(dist, "index.html"), (err) => {
        if (err) {
          next();
        }
      });
    });
  }

  app.listen(PORT, HOST, () => {
    console.log(`sticky-notes API listening on http://${HOST}:${PORT}`);
    console.log(`STORAGE=${storage}`);
    console.log(`AUTH=${authEnabled ? "on" : "off"}`);
    if (storage === "files") {
      console.log(`NOTES_DIR=${notesDir}`);
    }
    if (existsSync(dist)) {
      console.log(`STATIC=${dist}`);
    }
  });
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
