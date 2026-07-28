/**
 * Auth credential + session-secret persistence for the local Sticky Notes API.
 *
 * ## How auth works (big picture)
 *
 * 1. On first start, this module writes `notes/auth.json` with a username and a
 *    **bcrypt hash** of the password (never the plain password).
 * 2. Login (`POST /auth/login`) compares the submitted password to that hash.
 * 3. On success, Express stores `session.user` and sends an HTTP-only cookie
 *    named `sticky.sid`. Later `/notes` requests include that cookie.
 * 4. `requireAuth` (middleware) rejects requests that lack a valid session.
 *
 * ## Files on disk (under NOTES_DIR, usually `../notes`)
 *
 * - `auth.json` — `{ username, passwordHash }`
 * - `.session_secret` — random string used to sign session cookies
 *
 * ## Environment overrides
 *
 * | Variable               | Role |
 * |------------------------|------|
 * | `STICKY_AUTH`          | `off` / `0` / `false` / `no` disables auth entirely |
 * | `STICKY_USER`          | Username baked into a *new* auth.json (default `admin`) |
 * | `STICKY_PASSWORD`      | Password baked into a *new* auth.json (default `sticky-notes`) |
 * | `STICKY_SESSION_SECRET`| If set, used instead of reading/writing `.session_secret` |
 *
 * Changing `STICKY_USER` / `STICKY_PASSWORD` after `auth.json` already exists
 * does **nothing** — edit or delete `auth.json` to reset credentials.
 */

import { randomBytes } from "node:crypto";
import { readFile, writeFile } from "node:fs/promises";
import { join } from "node:path";

import bcrypt from "bcryptjs";

/** Shape of `notes/auth.json` after loadOrCreateAuth succeeds. */
export interface AuthRecord {
  username: string;
  /** bcrypt hash of the password — never store the plain password here. */
  passwordHash: string;
}

/** Default username for a brand-new auth.json (overridable via STICKY_USER). */
function defaultUser(): string {
  return process.env.STICKY_USER ?? "admin";
}

/** Default password for a brand-new auth.json (overridable via STICKY_PASSWORD). */
function defaultPassword(): string {
  return process.env.STICKY_PASSWORD ?? "sticky-notes";
}

/** Absolute path to the credentials file inside the notes directory. */
export function authFilePath(notesDir: string): string {
  return join(notesDir, "auth.json");
}

/** Absolute path to the cookie-signing secret file. */
export function sessionSecretPath(notesDir: string): string {
  return join(notesDir, ".session_secret");
}

/**
 * Load existing credentials, or create them on first run.
 *
 * - If `auth.json` exists and has a non-empty username + passwordHash → return it.
 * - Otherwise hash `STICKY_PASSWORD` (or the default), write `auth.json`, warn
 *   on the console so the operator knows the default login, and return the record.
 */
export async function loadOrCreateAuth(notesDir: string): Promise<AuthRecord> {
  const path = authFilePath(notesDir);
  try {
    const raw = await readFile(path, "utf8");
    const parsed = JSON.parse(raw) as Partial<AuthRecord>;
    if (
      typeof parsed.username === "string" &&
      parsed.username !== "" &&
      typeof parsed.passwordHash === "string" &&
      parsed.passwordHash !== ""
    ) {
      return { username: parsed.username, passwordHash: parsed.passwordHash };
    }
  } catch {
    // Missing file or invalid JSON — fall through and create defaults.
  }

  const username = defaultUser();
  const password = defaultPassword();
  // cost factor 10 is a reasonable default for a local single-user tool
  const passwordHash = await bcrypt.hash(password, 10);
  const record: AuthRecord = {
    username,
    passwordHash,
  };
  await writeFile(path, JSON.stringify(record, null, 2) + "\n", "utf8");
  console.warn(
    `[auth] Created ${path} — default login: ${username} / ${password}`,
  );
  console.warn(
    "[auth] Change STICKY_USER / STICKY_PASSWORD before first start, or edit auth.json.",
  );
  return record;
}

/**
 * Return a stable secret for signing express-session cookies.
 *
 * Priority:
 * 1. `STICKY_SESSION_SECRET` env var (useful in CI / containers)
 * 2. Existing `notes/.session_secret` on disk (persists across restarts)
 * 3. Generate 32 random bytes, write them to disk, and return them
 *
 * Without a stable secret, every server restart would invalidate all sessions.
 */
export async function loadOrCreateSessionSecret(notesDir: string): Promise<string> {
  if (process.env.STICKY_SESSION_SECRET) {
    return process.env.STICKY_SESSION_SECRET;
  }
  const path = sessionSecretPath(notesDir);
  try {
    const existing = (await readFile(path, "utf8")).trim();
    // Guard against truncated / empty files
    if (existing.length >= 16) {
      return existing;
    }
  } catch {
    // create below
  }
  const secret = randomBytes(32).toString("hex");
  await writeFile(path, secret + "\n", "utf8");
  return secret;
}

/**
 * Check whether the given username/password match the stored AuthRecord.
 *
 * Username must match exactly (case-sensitive). Password is verified with
 * bcrypt.compare so we never compare hashes manually.
 */
export async function verifyPassword(
  record: AuthRecord,
  username: string,
  password: string,
): Promise<boolean> {
  if (username !== record.username) {
    return false;
  }
  return bcrypt.compare(password, record.passwordHash);
}

/**
 * Whether the API should require login for `/notes`.
 *
 * Auth is **on** by default. Set `STICKY_AUTH` to `off`, `0`, `false`, or `no`
 * to disable (handy for local curl tests without cookies).
 */
export function isAuthEnabled(): boolean {
  const v = (process.env.STICKY_AUTH ?? "on").toLowerCase();
  return v !== "off" && v !== "0" && v !== "false" && v !== "no";
}
