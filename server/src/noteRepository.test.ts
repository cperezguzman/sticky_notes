/**
 * Integration-style tests for NoteRepository against a temporary notes dir.
 * Covers create → list → get → update → delete without touching the real notes/.
 */

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

  it("filters list by title or body query", async () => {
    await repo.create("Recipes", "pasta carbonara\n");
    await repo.create("Travel", "packing list for japan\n");

    const byTitle = await repo.list("recipe");
    assert.equal(byTitle.length, 1);
    assert.equal(byTitle[0].title, "Recipes");

    const byBody = await repo.list("japan");
    assert.equal(byBody.length, 1);
    assert.equal(byBody[0].title, "Travel");

    assert.equal((await repo.list("zzzz-missing")).length, 0);
    assert.ok((await repo.list("")).length >= 2);
  });

  it("persists optional sourceUrl on create/update", async () => {
    const created = await repo.create(
      "Linked",
      "body\n",
      "https://example.com/page",
    );
    assert.equal(created.sourceUrl, "https://example.com/page");
    const listed = await repo.list("example.com");
    assert.ok(listed.some((n) => n.id === created.id));
    const cleared = await repo.update(created.id, { sourceUrl: "" });
    assert.ok(cleared);
    assert.equal(cleared.sourceUrl, "");
  });
});
