# Sticky Notes API (TypeScript)

Express HTTP API for Sticky Notes.

## Storage modes

| Mode | When | Data |
|------|------|------|
| **cloud** | `DATABASE_URL` set (default) | PostgreSQL users + notes (UUID) |
| **files** | `STICKY_STORAGE=files` or no `DATABASE_URL` | Local `notes/*.txt` + `auth.json` |

Cloud mode is the product path (signup → verify email → notes in Postgres).  
Files mode keeps compatibility with the C++ CLI/GUI disk layout.

Production deploy: **[DEPLOY.md](DEPLOY.md)**. Desktop sync later: **ADR-003**.

## Setup (cloud)

```bash
cd server
npm install
export DATABASE_URL=postgres://postgres:sticky@127.0.0.1:5432/sticky_notes
export APP_ORIGIN=http://127.0.0.1:5173
npm run migrate
npm start
```

Without `RESEND_API_KEY`, verification links are printed to the console.

## Setup (files)

```bash
cd server
STICKY_STORAGE=files npm start
```

Default file login: `admin` / `sticky-notes` (created in `notes/auth.json` on first run).

## Env

| Variable | Default | Meaning |
|----------|---------|---------|
| `DATABASE_URL` | — | Enables cloud mode |
| `STICKY_STORAGE` | auto | `cloud` or `files` |
| `STICKY_AUTH` | `on` | `off` disables session gate |
| `STICKY_SESSION_SECRET` | auto / file | Cookie signing |
| `APP_ORIGIN` | `http://127.0.0.1:5173` | Base URL for verify links |
| `RESEND_API_KEY` / `EMAIL_FROM` | unset / Resend default | Email delivery |
| `HOST` / `PORT` | `127.0.0.1` / `8787` | Listen (use `HOST=0.0.0.0` in prod) |

## Auth endpoints

| Method | Path | Body | Result |
|--------|------|------|--------|
| `POST` | `/auth/signup` | `{ email, password }` | Create user + send verify (cloud) |
| `GET` | `/auth/verify?token=` | — | Mark email verified |
| `POST` | `/auth/login` | `{ email, password }` | Session cookie |
| `POST` | `/auth/logout` | — | Clear session |
| `GET` | `/auth/me` | — | Current user |

## Notes endpoints (auth required when enabled)

Same paths as before. Cloud note `id` is a **UUID** string. Files mode uses integer ids.

| Method | Path | Notes |
|--------|------|--------|
| `GET` | `/notes` | List index entries |
| `GET` | `/notes?q=` | Case-insensitive substring search over **title + body + sourceUrl** |
| `GET` | `/notes/:id` | Full note (includes `sourceUrl`) |
| `POST` | `/notes` | Create (`title`, `body`, optional `sourceUrl`) |
| `PUT` | `/notes/:id` | Update title/body/`sourceUrl` |
| `DELETE` | `/notes/:id` | Delete |

`sourceUrl` is an optional http(s) URL for **reverse context links** (Open source). Empty string clears it. Cloud mode also stores `source_domain` for a future browser-extension resurfacing feature.

## Tests

```bash
npm test                          # file tests; cloud skipped without DATABASE_URL
DATABASE_URL=... npm test         # includes tenant isolation + verify token
```

## Layout

```text
server/
  migrations/001_init.sql
  src/
    index.ts
    db.ts / migrate.ts
    auth/                 — file-mode auth.json helpers + requireAuth
    cloud/                — UserStore, PostgresNoteRepository, email
    routes/auth.ts
    routes/notes.ts
  DEPLOY.md
```
