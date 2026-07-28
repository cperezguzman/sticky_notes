/**
 * Extension auth via Bearer session id — avoids unreliable cookies from
 * moz-extension:// / chrome-extension:// popups to localhost.
 */

import type { NextFunction, Request, Response } from "express";
import type { SessionData, Store } from "express-session";

export function isExtensionClient(req: Request): boolean {
  return req.get("X-Sticky-Client") === "extension";
}

export function getBearerSessionId(req: Request): string | null {
  const auth = req.get("authorization");
  if (!auth?.startsWith("Bearer ")) {
    return null;
  }
  const sid = auth.slice("Bearer ".length).trim();
  return sid || null;
}

function applySessionData(req: Request, sid: string, data: SessionData): void {
  if (data.userId) {
    req.session.userId = data.userId;
  }
  if (data.email) {
    req.session.email = data.email;
  }
  if (data.user) {
    req.session.user = data.user;
  }
  req.sessionID = sid;
}

export function hydrateExtensionSession(
  req: Request,
  res: Response,
  next: NextFunction,
): void {
  if (req.session?.userId || req.session?.user) {
    next();
    return;
  }
  const sid = getBearerSessionId(req);
  const store = req.sessionStore as Store | undefined;
  if (!sid || !store) {
    next();
    return;
  }
  store.get(sid, (err, sess) => {
    if (!err && sess && typeof sess === "object") {
      applySessionData(req, sid, sess as SessionData);
    }
    next();
  });
}

export function extensionSessionFields(
  req: Request,
): { sessionToken: string } | Record<string, never> {
  if (!isExtensionClient(req) || !req.sessionID) {
    return {};
  }
  return { sessionToken: req.sessionID };
}

export function respondWithOptionalExtensionSession(
  req: Request,
  res: Response,
  body: Record<string, unknown>,
  status = 200,
): void {
  const send = () => {
    res.status(status).json({ ...body, ...extensionSessionFields(req) });
  };
  if (!isExtensionClient(req)) {
    send();
    return;
  }
  req.session.save((err) => {
    if (err) {
      res.status(500).json({ error: "session save failed" });
      return;
    }
    send();
  });
}
