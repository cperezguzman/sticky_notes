import { mkdir, readdir, readFile, unlink, writeFile } from "node:fs/promises";
import { join } from "node:path";

import {
  formatTimestampLine,
  parseNoteFile,
  parsedToNoteJson,
  serializeNoteFile,
  type NoteJson,
} from "./noteCodec.js";

export interface NoteIndexEntry {
  id: number;
  title: string;
  path: string;
}

export class NoteRepository {
  constructor(private readonly notesDir: string) {}

  private counterPath(): string {
    return join(this.notesDir, "next_note_id.txt");
  }

  private notePath(id: number): string {
    return join(this.notesDir, `note_${id}.txt`);
  }

  async ensureDataDir(): Promise<void> {
    await mkdir(this.notesDir, { recursive: true });
    try {
      await readFile(this.counterPath(), "utf8");
    } catch {
      await writeFile(this.counterPath(), "0", "utf8");
    }
  }

  async list(): Promise<NoteIndexEntry[]> {
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
      index.push({ id, title: parsed.title, path });
    }

    index.sort((a, b) => a.id - b.id);
    return index;
  }

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

  async create(title: string, body = ""): Promise<NoteJson> {
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
    });
    await writeFile(path, fileText, "utf8");
    const note = await this.getById(id);
    if (!note) {
      throw new Error(`Failed to read back created note ${id}`);
    }
    return note;
  }

  async update(
    id: number,
    patch: { title?: string; body?: string },
  ): Promise<NoteJson | null> {
    const existing = await this.getById(id);
    if (!existing) {
      return null;
    }

    // Preserve original Created value line from disk
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

    const fileText = serializeNoteFile({
      title,
      id,
      createdLine: parsed.createdLine || formatTimestampLine("created"),
      lastEditedLine: formatTimestampLine("lastEdited"),
      body,
    });
    await writeFile(path, fileText, "utf8");
    return this.getById(id);
  }

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
