import type { NextFunction, Request, Response } from "express";

import { isAuthEnabled } from "./store.js";

declare module "express-session" {
  interface SessionData {
    /** File-mode username (legacy single-user). */
    user?: string;
    /** Cloud-mode user UUID. */
    userId?: string;
    /** Cloud-mode email (display). */
    email?: string;
  }
}

export function requireAuth(req: Request, res: Response, next: NextFunction): void {
  if (!isAuthEnabled()) {
    next();
    return;
  }
  if (req.session?.userId || req.session?.user) {
    next();
    return;
  }
  res.status(401).json({ error: "unauthorized" });
}
