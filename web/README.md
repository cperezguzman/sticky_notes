# Sticky Notes Web UI

React + TypeScript client for the notes API.

## Cloud product flow

1. **Create account** (email + password, min 8 chars).
2. Open the **verification link** (emailed via Resend, or printed in the API log locally).
3. **Sign in** and create/edit notes — stored in PostgreSQL for your user.
4. Later (Phase 2): optional desktop app + local mirror — not in this UI yet.

## Setup

```bash
# Terminal 1 — API with Postgres (see server/DEPLOY.md)
cd server
export DATABASE_URL=postgres://postgres:sticky@127.0.0.1:5432/sticky_notes
export APP_ORIGIN=http://127.0.0.1:5173
npm run migrate && npm start

# Terminal 2
cd web
npm install
npm run dev
```

Open **http://127.0.0.1:5173**. Vite proxies `/auth`, `/notes`, `/health`.

## Themes

Same three presets as the SDL GUI: **Minimal**, **Retro**, **Cyberpunk**.

- **Theme** badge at the bottom of the sidebar opens a dropdown
- **Ctrl+T** cycles themes; **Ctrl+1/2/3** jump to Minimal / Retro / Cyberpunk
- Choice is stored in `localStorage` (`sticky.web.theme`) — browser-local, not `notes/gui_theme.txt`

## Search

Sidebar **Search notes…** filters the list by keyword in **title, body, or source URL** (API `GET /notes?q=`). Debounced ~200ms. **Ctrl+K** focuses the search box.

## Context links

- **Reverse:** optional **Source URL** on each note — **Open source** opens it.
- **Forward:** Chromium extension under [`extension/`](../extension/README.md) resurfaces notes when you visit a matching page/domain. Sidebar shows ↗ when a note has a link.

## Scripts

| Command | Meaning |
|---------|---------|
| `npm run dev` | Vite + API proxy |
| `npm run build` | Production `dist/` (served by Express in prod) |
| `npm run typecheck` | `tsc --noEmit` |
