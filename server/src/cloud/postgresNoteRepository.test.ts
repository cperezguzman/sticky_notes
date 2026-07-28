import assert from "node:assert/strict";
import { after, before, describe, it } from "node:test";

import { PostgresNoteRepository } from "./postgresNoteRepository.js";
import { UserStore } from "./userStore.js";
import { closePool, getPool, runMigrations } from "../db.js";

const hasDb = Boolean(process.env.DATABASE_URL);

describe("cloud postgres notes (requires DATABASE_URL)", { skip: !hasDb }, () => {
  before(async () => {
    await runMigrations();
  });

  after(async () => {
    await closePool();
  });

  it("scopes notes per user", async () => {
    const pool = getPool();
    const users = new UserStore(pool);
    const notes = new PostgresNoteRepository(pool);

    const emailA = `a-${Date.now()}@example.com`;
    const emailB = `b-${Date.now()}@example.com`;
    const userA = await users.createUser(emailA, "password12");
    const userB = await users.createUser(emailB, "password12");

    // Mark verified for realism (not required for repo)
    await pool.query(`UPDATE users SET email_verified_at = NOW() WHERE id = $1`, [
      userA.id,
    ]);

    const created = await notes.create(userA.id, "Private", "secret");
    const listedA = await notes.list(userA.id);
    assert.ok(listedA.some((n) => n.id === created.id));

    const stolen = await notes.getById(userB.id, created.id);
    assert.equal(stolen, null);

    const deleted = await notes.delete(userB.id, created.id);
    assert.equal(deleted, false);

    assert.equal(await notes.delete(userA.id, created.id), true);
  });

  it("stores and returns sourceUrl for reverse links", async () => {
    const pool = getPool();
    const users = new UserStore(pool);
    const notes = new PostgresNoteRepository(pool);
    const email = `link-${Date.now()}@example.com`;
    const user = await users.createUser(email, "password12");

    const created = await notes.create(
      user.id,
      "Trip plan",
      "flights",
      "https://example.com/booking",
    );
    assert.equal(created.sourceUrl, "https://example.com/booking");

    const listed = await notes.list(user.id, "example.com");
    assert.equal(listed.length, 1);
    assert.equal(listed[0].sourceUrl, "https://example.com/booking");

    const cleared = await notes.update(user.id, created.id, { sourceUrl: "" });
    assert.ok(cleared);
    assert.equal(cleared.sourceUrl, "");

    await notes.delete(user.id, created.id);
  });

  it("searches title and body for the owning user", async () => {
    const pool = getPool();
    const users = new UserStore(pool);
    const notes = new PostgresNoteRepository(pool);
    const email = `s-${Date.now()}@example.com`;
    const user = await users.createUser(email, "password12");
    const other = await users.createUser(`o-${Date.now()}@example.com`, "password12");

    const hit = await notes.create(user.id, "Kitchen", "buy saffron threads");
    await notes.create(user.id, "Garage", "oil change");
    await notes.create(other.id, "Kitchen", "buy saffron threads");

    const byBody = await notes.list(user.id, "saffron");
    assert.equal(byBody.length, 1);
    assert.equal(byBody[0].id, hit.id);

    const byTitle = await notes.list(user.id, "kitchen");
    assert.equal(byTitle.length, 1);

    assert.equal((await notes.list(user.id, "zzzz")).length, 0);

    await notes.delete(user.id, hit.id);
  });

  it("signup verify login token flow", async () => {
    const pool = getPool();
    const users = new UserStore(pool);
    const email = `v-${Date.now()}@example.com`;
    const user = await users.createUser(email, "password12");
    assert.equal(user.emailVerifiedAt, null);
    const token = await users.createVerifyToken(user.id);
    const verified = await users.consumeVerifyToken(token);
    assert.ok(verified);
    assert.ok(verified.emailVerifiedAt);
    const again = await users.findByEmail(email);
    assert.ok(again?.emailVerifiedAt);
  });
});
