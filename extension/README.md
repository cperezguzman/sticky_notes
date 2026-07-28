# Sticky Notes Context (browser extension)

Manifest V3 extension for **forward resurfacing**: when you visit a page, notes whose **Source URL** matches that page (exact) or its **domain** show up in the toolbar popup. The toolbar badge shows the match count.

## Requirements

- Sticky Notes API running (`server/`, usually `http://127.0.0.1:8787`)
- A note with a **Source URL** set (web editor or extension “New note for this page”)
- Firefox 109+ or a Chromium-based browser (Chrome, Edge, Brave, …)

## Install (unpacked)

### Firefox (Ubuntu and others)

1. Open `about:debugging#/runtime/this-firefox`
2. Click **Load Temporary Add-on…**
3. Select `extension/manifest.json` in this repo (Firefox uses `background.scripts`; Chromium uses `service_worker` — both are in the same manifest)
4. Open the toolbar popup → **Sign in** with the same account you use in the web UI  
   (the extension stores a session token locally — logging in only on Vite `:5173` is not enough)

Temporary add-ons are removed when Firefox quits. Reload the same way after a restart. For a permanent install you need a signed package from Mozilla (AMO) or local signing with `web-ext`.

### Chromium (Chrome, Edge, Brave, …)

1. Open `chrome://extensions`
2. Enable **Developer mode**
3. **Load unpacked** → select this `extension/` folder
4. Sign in via the popup (same as Firefox step 4)

## Options

Right-click the icon → **Options** (or the Options button in the popup):

| Setting | Default | Meaning |
|---------|---------|---------|
| API base URL | `http://127.0.0.1:8787` | Where `/notes/context` and `/auth` live |
| Web app URL | `http://127.0.0.1:5173` | Used to open a note (`/?note=<id>`) |

If you deploy the API elsewhere, set both URLs and grant the optional host permission when prompted.

## Firefox auth notes

The extension originally tried to reuse the API's normal `express-session` cookie flow:

1. popup `POST /auth/login`
2. server sets `sticky.sid`
3. popup `GET /auth/me`
4. popup `GET /notes/context?...`

That worked in the web UI, but not reliably from Firefox's `moz-extension://...` popup context on local HTTP development servers.

### What failed

- **First failure:** the API CORS allowlist only included the web UI origins (`http://127.0.0.1:5173` / `http://localhost:5173`), so Firefox blocked extension requests before the popup could read the response.
- **Second failure:** after CORS was fixed, valid credentials still led to `Could not establish a session: unauthorized`.
- **Root cause:** Firefox would accept the login request itself, but the popup's follow-up request did not reliably carry the session cookie back to the API in this local extension context.

We tried the usual cookie adjustments (`SameSite=None; Secure` for localhost), but the extension path was still brittle enough that it was not a good base for the feature.

### Final fix

The extension now uses a **local bearer session token** instead of relying on browser cookies:

- The popup sends `X-Sticky-Client: extension` with auth requests.
- `POST /auth/login` still creates a normal server session, but when the request came from the extension the API also returns:

  ```json
  { "sessionToken": "<express-session id>" }
  ```

- The extension stores that `sessionToken` in `ext.storage.local`.
- All later extension requests send:

  ```http
  Authorization: Bearer <sessionToken>
  X-Sticky-Client: extension
  ```

- Server middleware (`server/src/auth/extensionSession.ts`) looks up the session by id in the session store and hydrates `req.session` before `requireAuth` runs.

### Why keep server sessions at all?

This preserves the existing auth model:

- the web UI still uses normal cookies
- the extension still depends on a real server-side session
- notes routes and auth checks stay mostly unchanged

The only Firefox-specific behavior is how the extension **transports** the session identifier.

### Troubleshooting

- If the popup says `invalid credentials`, the request reached the API and the login was rejected normally.
- If the popup says `Login succeeded but no extension session token was returned`, the extension code is newer than the running API. Restart `server/`.
- If login seems stuck after code changes, remove and reload the temporary add-on in `about:debugging`, then try again.

## How matching works

API: `GET /notes/context?url=<current-page>`

1. **Exact** — same URL as the note’s `sourceUrl` (hash ignored)
2. **Domain** — same host as `source_domain` (`www.` stripped)

Exact matches are listed first.

## Popup actions

- Click a note → opens the web app focused on that note
- **New note for this page** → creates a note with `sourceUrl` = current tab
- **Sign out** → clears the extension's stored session token (and also calls API logout)

## Layout

```text
extension/
  manifest.json
  browser.js        — Firefox `browser` / Chromium `chrome` shim
  background.js     — tab listener + badge
  popup.*           — login + matches
  options.*         — API / web URLs
  shared.js         — fetch helpers + stored extension session token
  icons/
```
