import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, it } from "node:test";

import {
  displayTimestamp,
  parseNoteFile,
  parsedToNoteJson,
  serializeNoteFile,
} from "./noteCodec.js";

const here = dirname(fileURLToPath(import.meta.url));
const fixturePath = join(here, "..", "..", "tests", "fixtures", "note_sample.txt");

describe("noteCodec", () => {
  it("parses the C++ sample fixture", () => {
    const text = readFileSync(fixturePath, "utf8");
    const parsed = parseNoteFile(text);
    assert.ok(parsed);
    assert.equal(parsed.title, "Sample Title");
    assert.equal(parsed.id, "42");
    assert.equal(parsed.createdLine, "Created: April 21, 2026 at 10:39 PM");
    assert.equal(parsed.lastEditedLine, "Last Edited: April 21, 2026 at 10:40 PM");
    assert.equal(parsed.body, "line one\nline two\n");
  });

  it("round-trips serialize then parse", () => {
    const raw = serializeNoteFile({
      title: "Round trip",
      id: 7,
      createdLine: "Created: July 22, 2026 at 02:00 PM",
      lastEditedLine: "Last Edited: July 22, 2026 at 02:05 PM",
      body: "hello\nworld",
    });
    const parsed = parseNoteFile(raw);
    assert.ok(parsed);
    assert.equal(parsed.title, "Round trip");
    assert.equal(parsed.id, "7");
    assert.equal(parsed.body, "hello\nworld\n");
  });

  it("maps to NoteJson with stripped timestamps", () => {
    const parsed = parseNoteFile(readFileSync(fixturePath, "utf8"));
    assert.ok(parsed);
    const note = parsedToNoteJson(parsed, "notes/note_42.txt");
    assert.ok(note);
    assert.equal(note.id, 42);
    assert.equal(note.created, "April 21, 2026 at 10:39 PM");
    assert.equal(note.lastEdited, "April 21, 2026 at 10:40 PM");
    assert.equal(displayTimestamp("Created: x"), "x");
  });

  it("rejects unknown labels", () => {
    assert.equal(parseNoteFile("Nope:\nvalue\n"), null);
  });
});
