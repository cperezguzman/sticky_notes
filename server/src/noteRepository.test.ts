import assert from "node:assert/strict";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { after, before, describe, it } from "node:test";

import { NoteRepository } from "./noteRepository.js";

describe("NoteRepository", () => {
  let dir: string;
  let repo: NoteRepository;

  before(async () => {
    dir = await mkdtemp(join(tmpdir(), "sticky-notes-api-"));
    repo = new NoteRepository(dir);
  });

  after(async () => {
    await rm(dir, { recursive: true, force: true });
  });

  it("creates notes with monotonic ids", async () => {
    const a = await repo.create("First", "alpha");
    const b = await repo.create("Second", "beta\n");
    assert.equal(a.id, 0);
    assert.equal(b.id, 1);
    assert.equal(a.title, "First");
    assert.equal(a.body, "alpha\n");
    const counter = (await readFile(join(dir, "next_note_id.txt"), "utf8")).trim();
    assert.equal(counter, "2");
  });

  it("lists, gets, updates, and deletes", async () => {
    const listed = await repo.list();
    assert.equal(listed.length, 2);
    assert.equal(listed[0].title, "First");

    const got = await repo.getById(0);
    assert.ok(got);
    assert.equal(got.title, "First");

    const updated = await repo.update(0, { title: "Renamed", body: "new body" });
    assert.ok(updated);
    assert.equal(updated.title, "Renamed");
    assert.equal(updated.body, "new body\n");

    assert.equal(await repo.delete(1), true);
    assert.equal(await repo.getById(1), null);
    assert.equal(await repo.delete(99), false);
  });
});
