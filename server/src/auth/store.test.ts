/**
 * Unit tests for the auth store (credentials file + bcrypt verify + feature flag).
 *
 * Runs with Node’s built-in test runner (`node --test`). Uses a temp directory
 * so we never touch the real `notes/auth.json` in the repo.
 *
 * What we cover:
 * - Creating `auth.json` from env defaults and verifying good/bad passwords
 * - Reloading the same file returns the same hash (idempotent create)
 * - `isAuthEnabled()` respects `STICKY_AUTH=off` vs `on`
 */

import assert from "node:assert/strict";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { after, before, describe, it } from "node:test";

import {
  isAuthEnabled,
  loadOrCreateAuth,
  verifyPassword,
} from "./store.js";

describe("auth store", () => {
  let dir: string;
  // Save / restore env so these tests don’t leak into other suites
  const prevAuth = process.env.STICKY_AUTH;
  const prevUser = process.env.STICKY_USER;
  const prevPass = process.env.STICKY_PASSWORD;

  before(async () => {
    dir = await mkdtemp(join(tmpdir(), "sticky-auth-"));
    process.env.STICKY_USER = "tester";
    process.env.STICKY_PASSWORD = "s3cret";
  });

  after(async () => {
    if (prevAuth === undefined) {
      delete process.env.STICKY_AUTH;
    } else {
      process.env.STICKY_AUTH = prevAuth;
    }
    if (prevUser === undefined) {
      delete process.env.STICKY_USER;
    } else {
      process.env.STICKY_USER = prevUser;
    }
    if (prevPass === undefined) {
      delete process.env.STICKY_PASSWORD;
    } else {
      process.env.STICKY_PASSWORD = prevPass;
    }
    await rm(dir, { recursive: true, force: true });
  });

  it("creates auth.json and verifies password", async () => {
    const record = await loadOrCreateAuth(dir);
    assert.equal(record.username, "tester");
    const raw = await readFile(join(dir, "auth.json"), "utf8");
    // File must store a hash, never the plaintext password
    assert.match(raw, /passwordHash/);
    assert.equal(await verifyPassword(record, "tester", "s3cret"), true);
    assert.equal(await verifyPassword(record, "tester", "wrong"), false);
    assert.equal(await verifyPassword(record, "other", "s3cret"), false);

    // Second call should reuse the existing file, not re-hash
    const again = await loadOrCreateAuth(dir);
    assert.equal(again.passwordHash, record.passwordHash);
  });

  it("isAuthEnabled respects STICKY_AUTH", () => {
    process.env.STICKY_AUTH = "off";
    assert.equal(isAuthEnabled(), false);
    process.env.STICKY_AUTH = "on";
    assert.equal(isAuthEnabled(), true);
  });
});
