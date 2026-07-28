# Deploy Sticky Notes (cloud Phase 1)

Production shape: **one Express service** serves the API and the built React app (`web/dist`) so cookies stay same-origin. **PostgreSQL** holds users + notes. **Resend** sends verification emails.

## 1. PostgreSQL

Create a Postgres instance (Railway Postgres plugin, Neon, Supabase, or local Docker):

```bash
docker run --name sticky-pg -e POSTGRES_PASSWORD=sticky -e POSTGRES_DB=sticky_notes \
  -p 5432:5432 -d postgres:16
```

Connection string example:

```text
DATABASE_URL=postgres://postgres:sticky@localhost:5432/sticky_notes
```

## 2. Environment

| Variable | Required | Meaning |
|----------|----------|---------|
| `DATABASE_URL` | yes (cloud) | Postgres connection string |
| `STICKY_SESSION_SECRET` | recommended | Cookie signing secret (long random string) |
| `STICKY_STORAGE` | no | `cloud` (default if DATABASE_URL set) or `files` |
| `STICKY_AUTH` | no | `on` (default) or `off` |
| `APP_ORIGIN` | yes (prod) | Public site origin, e.g. `https://your-app.up.railway.app` |
| `RESEND_API_KEY` | yes (prod email) | Resend API key |
| `EMAIL_FROM` | recommended | e.g. `Sticky Notes <notes@yourdomain.com>` |
| `HOST` / `PORT` | no | Listen address (Railway sets `PORT`) |
| `NODE_ENV` | prod | `production` enables `secure` cookies |

Without `RESEND_API_KEY`, verify links are **printed to the server log** (fine for local).

## 3. Build & migrate

```bash
# API deps + schema
cd server
npm install
npm run migrate

# UI
cd ../web
npm install
npm run build

# Run API (serves ../web/dist when present)
cd ../server
DATABASE_URL=... STICKY_SESSION_SECRET=... APP_ORIGIN=https://... npm start
```

On Railway: set root or start command to build web then start server, e.g.:

```bash
cd web && npm ci && npm run build && cd ../server && npm ci && npm run migrate && npm start
```

Listen on `0.0.0.0` in production:

```bash
HOST=0.0.0.0 npm start
```

## 4. Resend

1. Create a Resend account and API key.
2. Verify your sending domain (or use Resend’s onboarding sender for tests).
3. Set `EMAIL_FROM` and `RESEND_API_KEY`.
4. Set `APP_ORIGIN` to the public HTTPS URL so verify links work.

## 5. Backups

Enable your host’s Postgres backups (Railway snapshots, provider PITR). Notes and accounts live only in the database in cloud mode — treat backups as required.

## 6. Local file mode (optional)

For curl/CLI against disk without Postgres:

```bash
STICKY_STORAGE=files npm start
```

Uses `notes/` + `notes/auth.json` (single local user). Does not affect cloud users.

## 7. Desktop sync (Phase 2 — not implemented)

See ADR-003: optional C++ app login + pull/push to this API, with a user toggle for local `notes/` mirror.
