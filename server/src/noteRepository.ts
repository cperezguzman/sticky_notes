/**
 * File-backed note storage shared with the C++ CLI / SDL GUI.
 *
 * Reads and writes sectioned plain-text files:
 *   notes/next_note_id.txt   — next numeric id to assign
 *   notes/note_<id>.txt      — Title / ID / Created / Last Edited / Body sections
 *
 * Parsing and serialization are delegated to `noteCodec.ts` so the on-disk
 * format stays byte-compatible with `src/note_file_codec.cpp`.
 *
 * ## Concurrency
 *
 * There is no locking. If the CLI, GUI, and API edit the same file at once,
 * **last write wins**. That matches the learning-project scope (local single user).
 */

import { mkdir, readdir, readFile, unlink, writeFile } from "node:fs/promises";
import { join } from "node:path";

import {
  formatTimestampLine,
  parseNoteFile,
  parsedToNoteJson,
  serializeNoteFile,
  type NoteJson,
} from "./noteCodec.js";
import { noteMatchesQuery } from "./noteSearch.js";
import { rankContextHits, type ContextNoteHit } from "./sourceUrl.js";

/** Lightweight row for the sidebar list (no body text). */
export interface NoteIndexEntry {
  id: number;
  title: string;
  /** Absolute path to the note file on disk. */
  path: string;
  /** External context URL if set. */
  sourceUrl: string;
}

export class NoteRepository {
  constructor(private readonly notesDir: string) {}

  private counterPath(): string {
    return join(this.notesDir, "next_note_id.txt");
  }

  private notePath(id: number): string {
    return join(this.notesDir, `note_${id}.txt`);
  }

  /**
   * Create the notes directory and seed `next_note_id.txt` with `0` if missing.
   * Safe to call repeatedly (idempotent).
   */
  async ensureDataDir(): Promise<void> {
    await mkdir(this.notesDir, { recursive: true });
    try {
      await readFile(this.counterPath(), "utf8");
    } catch {
      await writeFile(this.counterPath(), "0", "utf8");
    }
  }

  /**
   * Scan `note_*.txt` files, parse metadata, return sorted by id.
   * Skips unreadable / malformed files instead of failing the whole list.
   * When `query` is non-blank, keeps notes whose title or body contains it
   * (case-insensitive substring).
   */
  async list(query?: string): Promise<NoteIndexEntry[]> {
    await this.ensureDataDir();
    const entries = await readdir(this.notesDir, { withFileTypes: true });
    const index: NoteIndexEntry[] = [];

    for (const entry of entries) {
      if (!entry.isFile() || !entry.name.startsWith("note_") || !entry.name.endsWith(".txt")) {
        continue;
      }
      const path = join(this.notesDir, entry.name);
      const text = await readFile(path, "utf8");
      const parsed = parseNoteFile(text);
      if (!parsed || parsed.id === "") {
        continue;
      }
      const id = Number.parseInt(parsed.id, 10);
      if (Number.isNaN(id)) {
        continue;
      }
      if (!noteMatchesQuery(parsed.title, parsed.body, query ?? "", parsed.sourceUrl)) {
        continue;
      }
      index.push({
        id,
        title: parsed.title,
        path,
        sourceUrl: parsed.sourceUrl,
      });
    }

    index.sort((a, b) => a.id - b.id);
    return index;
  }

  /** Notes whose source URL matches this page (exact) or its domain. */
  async listByContext(pageUrl: string): Promise<ContextNoteHit[]> {
    const all = await this.list();
    return rankContextHits(all, pageUrl);
  }

  /** Load a single note by numeric id, or null if the file is missing / invalid. */
  async getById(id: number): Promise<NoteJson | null> {
    await this.ensureDataDir();
    const path = this.notePath(id);
    let text: string;
    try {
      text = await readFile(path, "utf8");
    } catch {
      return null;
    }
    const parsed = parseNoteFile(text);
    if (!parsed) {
      return null;
    }
    return parsedToNoteJson(parsed, path);
  }

  /**
   * Read `next_note_id.txt`, return that id, then write id+1.
   * Ids are never reused after delete (gaps are fine).
   */
  private async allocateId(): Promise<number> {
    await this.ensureDataDir();
    let raw: string;
    try {
      raw = (await readFile(this.counterPath(), "utf8")).trim();
    } catch {
      raw = "0";
    }
    const id = Number.parseInt(raw || "0", 10);
    if (Number.isNaN(id) || id < 0) {
      throw new Error(`Invalid next_note_id.txt value: ${raw}`);
    }
    await writeFile(this.counterPath(), String(id + 1), "utf8");
    return id;
  }

  /**
   * Create a new note file with fresh Created / Last Edited timestamps.
   * Empty title becomes `"Untitled"`. Body is normalized to end with `\n`.
   */
  async create(title: string, body = "", sourceUrl = ""): Promise<NoteJson> {
    const id = await this.allocateId();
    const nowCreated = formatTimestampLine("created");
    const nowEdited = formatTimestampLine("lastEdited");
    const normalizedTitle = title.trim() === "" ? "Untitled" : title;
    const bodyText = body.endsWith("\n") || body === "" ? body : body + "\n";
    const path = this.notePath(id);
    const fileText = serializeNoteFile({
      title: normalizedTitle,
      id,
      createdLine: nowCreated,
      lastEditedLine: nowEdited,
      body: bodyText,
      sourceUrl,
    });
    await writeFile(path, fileText, "utf8");
    const note = await this.getById(id);
    if (!note) {
      throw new Error(`Failed to read back created note ${id}`);
    }
    return note;
  }

  /**
   * Patch title and/or body. Preserves the original Created line from disk;
   * always refreshes Last Edited to “now”.
   */
  async update(
    id: number,
    patch: { title?: string; body?: string; sourceUrl?: string },
  ): Promise<NoteJson | null> {
    const existing = await this.getById(id);
    if (!existing) {
      return null;
    }

    // Re-read raw file so we keep the exact Created value line
    const path = this.notePath(id);
    const raw = await readFile(path, "utf8");
    const parsed = parseNoteFile(raw);
    if (!parsed) {
      return null;
    }

    const title =
      patch.title !== undefined
        ? patch.title.trim() === ""
          ? "Untitled"
          : patch.title
        : existing.title;
    let body = patch.body !== undefined ? patch.body : existing.body;
    if (body !== "" && !body.endsWith("\n")) {
      body += "\n";
    }
    const sourceUrl =
      patch.sourceUrl !== undefined ? patch.sourceUrl : existing.sourceUrl;

    const fileText = serializeNoteFile({
      title,
      id,
      createdLine: parsed.createdLine || formatTimestampLine("created"),
      lastEditedLine: formatTimestampLine("lastEdited"),
      body,
      sourceUrl,
    });
    await writeFile(path, fileText, "utf8");
    return this.getById(id);
  }

  /** Delete `note_<id>.txt`. Returns false if the file was already gone. */
  async delete(id: number): Promise<boolean> {
    await this.ensureDataDir();
    try {
      await unlink(this.notePath(id));
      return true;
    } catch {
      return false;
    }
  }
}
