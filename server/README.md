# Sticky Notes API (TypeScript)

Local **Node.js / TypeScript** HTTP adapter over the same `notes/` directory used by the C++ CLI and SDL GUI. Local files remain the source of truth; this process only reads and writes sectioned `note_<id>.txt` files (same format as `note_file_codec`).

Binds **`127.0.0.1` only**. No auth. Concurrent CLI/GUI/API editors: **last write wins** per file.

## Setup

```bash
cd server
npm install
```

## Run

From `server/` (default notes dir is `../notes` at the repo root):

```bash
npm start
```

Optional env:

| Variable | Default | Meaning |
|----------|---------|---------|
| `NOTES_DIR` | `../notes` (resolved from `server/`) | Path to the notes directory |
| `HOST` | `127.0.0.1` | Bind address |
| `PORT` | `8787` | Listen port |

```bash
NOTES_DIR=/tmp/my-notes PORT=9000 npm start
```

## Endpoints

| Method | Path | Body | Result |
|--------|------|------|--------|
| `GET` | `/health` | — | `{ "ok": true }` |
| `GET` | `/notes` | — | `[{ id, title, path }, ...]` |
| `GET` | `/notes/:id` | — | Full note JSON |
| `POST` | `/notes` | `{ "title"?, "body"? }` | Create note (201) |
| `PUT` | `/notes/:id` | `{ "title"?, "body"? }` | Update note |
| `DELETE` | `/notes/:id` | — | Remove file (204) |

Note JSON shape:

```json
{
  "id": 3,
  "title": "Interview prep",
  "created": "June 20, 2026 at 10:39 PM",
  "lastEdited": "July 22, 2026 at 02:05 PM",
  "body": "line one\nline two\n",
  "path": "/…/notes/note_3.txt"
}
```

## Example curls

```bash
curl -s http://127.0.0.1:8787/health

curl -s http://127.0.0.1:8787/notes

curl -s -X POST http://127.0.0.1:8787/notes \
  -H 'Content-Type: application/json' \
  -d '{"title":"From API","body":"hello from TypeScript\n"}'

curl -s http://127.0.0.1:8787/notes/0

curl -s -X PUT http://127.0.0.1:8787/notes/0 \
  -H 'Content-Type: application/json' \
  -d '{"body":"updated body\n"}'

curl -s -o /dev/null -w '%{http_code}\n' -X DELETE http://127.0.0.1:8787/notes/0
```

## Tests

```bash
cd server
npm test
```

Uses the repo fixture `tests/fixtures/note_sample.txt` plus a temp `notes/` dir for repository CRUD.

## Layout

```text
server/
  src/
    index.ts           — listen on 127.0.0.1
    noteCodec.ts       — parse/serialize (port of note_file_codec)
    noteRepository.ts  — list/get/create/update/delete + next_note_id
    routes/notes.ts    — REST handlers
```
