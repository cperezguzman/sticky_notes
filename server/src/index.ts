import { resolve } from "node:path";
import { fileURLToPath } from "node:url";

import express from "express";

import { NoteRepository } from "./noteRepository.js";
import { createNotesRouter } from "./routes/notes.js";

const HOST = process.env.HOST ?? "127.0.0.1";
const PORT = Number.parseInt(process.env.PORT ?? "8787", 10);

function defaultNotesDir(): string {
  // server/ → repo root → notes/
  const serverRoot = fileURLToPath(new URL("..", import.meta.url));
  return resolve(serverRoot, "..", "notes");
}

const notesDir = resolve(process.env.NOTES_DIR ?? defaultNotesDir());

async function main(): Promise<void> {
  const repo = new NoteRepository(notesDir);
  await repo.ensureDataDir();

  const app = express();
  app.use(express.json({ limit: "1mb" }));

  app.get("/health", (_req, res) => {
    res.json({ ok: true });
  });

  app.use("/notes", createNotesRouter(repo));

  app.listen(PORT, HOST, () => {
    console.log(`sticky-notes API listening on http://${HOST}:${PORT}`);
    console.log(`NOTES_DIR=${notesDir}`);
  });
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
