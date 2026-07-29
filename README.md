# Sticky Notes

A **C++20** sticky-notes app with two front ends over the same local store:

- **Terminal editor** — command-driven editing (line/cursor ops, undo/redo, find, yank/paste, list/open/export).
- **SDL3 desk GUI** — full-window note layout (refits on resize), themes, sidebar, and always-on-top pop-out windows.

Notes are **plain text files** under `notes/`, with a monotonic ID counter so each new note gets a stable `note_<id>.txt` filename. Both UIs share the same load/save codec and edit core; the project doubles as a learning path toward a custom GUI textbox (platform-neutral input seam → SDL adapter → multi-panel desk).

Optional **TypeScript / Node HTTP API** (`server/`) supports two storage modes:

- **Cloud** (default when `DATABASE_URL` is set): multi-user signup, email verification, notes in PostgreSQL.
- **Files** (`STICKY_STORAGE=files`): single-user session over local `notes/` (same as CLI/GUI).

See [server/README.md](server/README.md) and [server/DEPLOY.md](server/DEPLOY.md).

Optional **React web UI** (`web/`): create an account, verify email, then list/edit notes in the browser. Themes match the SDL GUI (Minimal / Retro / Cyberpunk). See [web/README.md](web/README.md). Optional **browser extension** (`extension/`) resurfaces notes when you visit a linked site (Firefox or Chromium) — see [extension/README.md](extension/README.md).

## Requirements

- **Compiler:** GCC or Clang with **C++20** (`std::format`, `std::filesystem`, etc.)
- **OS:** Linux is assumed (paths use `/`; run the binary from the project root so `notes/` resolves correctly)
- **GUI (optional):** `cmake` and `curl` — SDL3 is built locally on first GUI setup (`third_party/`, gitignored)

### Install build tools (if needed)

```bash
# Debian / Ubuntu
sudo apt update && sudo apt install build-essential cmake curl

# Fedora
sudo dnf install gcc-c++ make cmake curl
```

## Quick start (fresh clone)

From the repository root:

```bash
git clone https://github.com/cperezguzman/sticky_notes.git
cd sticky_notes
./scripts/setup.sh
```

That checks for `g++`, `make`, `cmake`, and `curl`, builds the CLI, builds SDL3 + GUI (first run can take a few minutes), and prints how to launch.

**App menu + dock icon (Linux):**

```bash
./scripts/setup.sh --desktop
```

**Terminal only (skip SDL/GUI):**

```bash
./scripts/setup.sh --cli-only
```

Equivalent: `make setup` (no `--desktop`; run `make desktop` separately if you want the launcher).

Then:

```bash
./sticky_notes                        # terminal editor
./scripts/run-sticky-notes-gui.sh     # SDL desk GUI
```

## Quick start (Docker — one command)

If you have [Docker](https://docs.docker.com/engine/install/) installed and don’t want to install build tools on the host:

```bash
git clone https://github.com/cperezguzman/sticky_notes.git
cd sticky_notes
./scripts/docker.sh          # GUI (Linux desktop + X11/Wayland)
./scripts/docker.sh cli      # terminal editor only
```

First run **builds the image** (SDL3 + app inside Docker; may take several minutes). Your notes are stored in `./notes` on the host.

| Command | What it does |
|---------|----------------|
| `./scripts/docker.sh` or `make docker` | Build image if needed, run GUI |
| `./scripts/docker.sh cli` or `make docker-cli` | Build image if needed, run CLI |
| `./scripts/docker.sh build` or `make docker-build` | Build image only |

**GUI in Docker:** Linux with `DISPLAY` set (GNOME/KDE/X11). The script mounts `/tmp/.X11-unix` so the window appears on your desktop. macOS/Windows GUI in Docker is not supported here (use native `./scripts/setup.sh` or CLI via Docker).

**No compiler on host:** Docker path only needs Docker; `cmake`/`g++` run inside the image.

## Quick start (Web UI — React + cloud)

Needs **Node.js 18+** and **PostgreSQL** (`DATABASE_URL`).

```bash
# Postgres (example)
docker run --name sticky-pg -e POSTGRES_PASSWORD=sticky -e POSTGRES_DB=sticky_notes \
  -p 5432:5432 -d postgres:16

# Terminal 1 — API (cloud)
cd server
npm install
export DATABASE_URL=postgres://postgres:sticky@127.0.0.1:5432/sticky_notes
export APP_ORIGIN=http://127.0.0.1:5173
npm run migrate
npm start

# Terminal 2 — UI
cd web && npm install && npm run dev
```

Open http://127.0.0.1:5173 — **Create account**, check the server log for the verify link (no Resend key locally), then sign in.

Deploy checklist: [server/DEPLOY.md](server/DEPLOY.md).

File-only API (no Postgres): `STICKY_STORAGE=files npm start` in `server/`.

### Firefox extension note

The browser extension works on Firefox, but the final auth path is **not** "just reuse the web cookie":

- Firefox MV3 currently needs `background.scripts` alongside `background.service_worker`.
- The extension popup originally failed CORS because `moz-extension://...` was not in the API allowlist.
- After CORS was fixed, Firefox still did not reliably keep/send the local API session cookie from the popup.
- Final design: the extension logs in through `/auth/login`, receives a `sessionToken`, stores it in extension local storage, and sends it back as `Authorization: Bearer ...`.

See [extension/README.md](extension/README.md) and [server/README.md](server/README.md) for the full debugging story and final architecture.

## Build

```bash
make
```

Produces `sticky_notes` in the project directory. Alternatively:

```bash
g++ -std=c++20 -Wall -Wextra -o sticky_notes \
  src/main.cpp src/parser.cpp src/sticky_note.cpp src/note_store.cpp \
  src/note_editor.cpp src/note_editor_cli.cpp
```

## Run

From the repository root (so `notes/next_note_id.txt` and `notes/note_*.txt` are found):

```bash
./sticky_notes
```

**First run:** the program creates `notes/` and `notes/next_note_id.txt` if they are missing. When the counter is `0`, it walks you through creating your first note.

**Data:** `notes/` is gitignored — your note files stay local and are not committed.

**Later runs:** you can **open** an existing note (by **exact title** or **numeric id** from `list`) or **create** a new one.

## Project status

This project is currently in a good **portfolio-pause** state: the CLI is complete, the SDL desk GUI is usable, pop-out windows work, and the app has both automated tests and a desktop launcher path on Linux.

If you come back later, start with:

1. `README.md` for run/build commands and current shortcuts
2. `docs/portfolio/sticky-notes/dev-log.md` for the latest session snapshot
3. `docs/portfolio/sticky-notes/future-features.md` for backlog / resume ideas

## Commands

| Command | Description |
|--------|-------------|
| `write <text>` | Replace the **current line** with `text`. |
| `append <text>` | Add `text` to the **end** of the current line. |
| `insert <text>` | Insert `text` at the **cursor** (column position). |
| `erase` | Delete **one character before** the cursor (backspace). |
| `del` | Delete the character **at** the cursor (forward delete). |
| `undo` / `redo` | Undo or redo the last **body** edit (up to 100 steps). |
| `find <text>` | Search forward from the cursor; wraps around the note. |
| `findnext` | Jump to the next match of the last `find` query. |
| `yank` | Copy the current line to an internal clipboard. |
| `paste` | Insert the yanked line **after** the current line. |
| `erase char` | Same as `erase`. |
| `erase char <n>` | Delete up to `n` characters before the cursor. |
| `erase word` | Delete the word immediately before the cursor. |
| `erase word <n>` | Delete up to `n` words before the cursor. |
| `left` / `right` | Move the cursor one column; crosses line boundaries at edges. |
| `home` / `end` | Move to start / end of the current line. |
| `pos` | Print the current line and column. |
| `goto <line> [col]` | Jump to line (1-based); optional column (1-based, default 1). |
| `newline` | Split the line at the cursor, or insert a blank line if at end. |
| `delete line` | Remove the line at the cursor (does not delete the note file). |
| `show` | Print the body; `>` marks the current line and `\|` marks the column. |
| `save` | Write the current note to its file. |
| `rename <title>` | Change the current note's title and save. |
| `create` | Create a new note; the **current** note is saved first if it has a path. |
| `list` | Print **id : title** for each `notes/note_*.txt` file that parses correctly. |
| `open <title\|id>` | Load a note by **exact title** or **numeric id** (as shown in `list`). |
| `view <title\|id>` | Print that note’s body without switching the active note. |
| `export markdown [path]` | Write the current note as Markdown with YAML frontmatter. Default: `exports/note_<id>.md` (gitignored). |
| `delete` | Delete the **note file** from disk (with confirmation); clears in-memory state. |
| `quit` | Save the current note (if it has a path) and exit (with confirmation). |
| `help` | Show the built-in command summary. |

Unknown commands print a short error; use `help` for the full list.

## On-disk layout

- `notes/next_note_id.txt` — next numeric id to assign (one integer per line).
- `notes/gui_theme.txt` — last GUI theme (`minimal`, `retro`, or `cyberpunk`); optional (defaults to Minimal).
- `notes/desk_state.txt` — sidebar open flag + desk note path; optional (defaults to most recently edited note).
- `notes/note_<id>.txt` — note file; format is sectioned text:

  `Title:`, `ID:`, `Created:`, `Last Edited:`, optional `Source:` (external http(s) URL), optional `Font:` / `FontSize:` (GUI body typography; default `debug` / `8`), optional `Styles:` (character style runs: `start length bius`), `Body:` (each label on its own line, value on the following line(s); body is the rest of the file).

Opening a note **reloads** title, id, body, optional source URL, and parses **Created** / **Last Edited** lines back into `std::chrono` (English month names; uses `en_US.UTF-8` locale when available). If a line is missing or invalid, that field falls back to the current time, and `last_edited` is clamped so it is not before `created`.

## Project layout

```
src/
  main.cpp              — CLI loop and command dispatch
  note_editor.cpp/.h    — silent edit core (`EditStatus`, typed erase API)
  note_editor_cli.cpp/.h — terminal presentation adapter
  note_store.cpp/.h     — load/save, list, open/view by title or id
  parser.cpp/.h      — CLI command parsing
  note_file_codec.*  — on-disk note file format (`ParsedNoteFile`)
  text_font.*        — note body typography (Debug default; Sans/Serif/Mono/Times/Papyrus/Art Deco)
  text_style.*       — character style runs (bold/italic/underline/strike)
  text_font_render.* — SDL draw path for body fonts (GUI only)
  sticky_note.cpp/.h — `sticky_note` struct; timestamps
  textbox_input.*    — platform-neutral multiline key seam (Phase 2–3)
  textbox_sdl.*      — SDL3 adapter + themed panel render
  gui_theme.*        — desk/panel color themes (Minimal, Retro, Cyberpunk); persists to notes/gui_theme.txt
  sticky_gui.*       — Phase 4: desk layout (fills window), sidebar, load/save
  sticky_popup.*     — Phase 4+: pop-out notes as always-on-top SDL windows
sandbox/
  textbox_main.cpp   — multi-window SDL loop (desk + floating pop-outs)
server/
  package.json / src/ — optional TypeScript notes HTTP API (same notes/ store)
web/
  package.json / src/ — optional React UI (Vite; proxies to server/)
extension/
  manifest.json / … — optional browser extension (Firefox or Chromium; forward context resurfacing)
scripts/
  build-sdl3.sh           — vendored SDL3 install (local, gitignored)
  setup.sh                — fresh-clone setup (CLI + GUI; optional --desktop)
  docker.sh               — Docker one-click (CLI or GUI; no host toolchain)
  run-sticky-notes-gui.sh — launch GUI from repo root (builds if needed)
  install-desktop.sh      — install ~/.local/share/applications entry
Dockerfile / docker-compose.yml — container build and run services
assets/
  sticky-notes.png        — launcher icon
tests/
  test_main.cpp         — unit tests (CLI, editor, codec, GUI logic)
  textbox_harness.cpp   — Phase 4 integration harness (96 headless checks)
  textbox_smoke.sh      — runs harness; mirrors manual_smoke.sh for GUI
  sdl_event_helpers.h   — synthetic SDL key/mouse events for tests
  test_notes_dir.h      — isolated temp notes/ per test scenario
  fixtures/             — sample note files for parser/codec tests
notes/               — local data (gitignored; auto-created on first run)
Makefile
```

Build targets: `sticky_notes` (CLI), `test_runner`, `textbox_test_harness`, `textbox_sandbox` (GUI), `gui` (alias), `setup` (fresh clone), `desktop` (Linux launcher), `docker` / `docker-cli` / `docker-build` (container).

## Tests

All test commands run from the project root. **GUI tests do not open a window** — they simulate SDL events in process.

**One-time:** SDL3 must be built for GUI-linked tests (`make test`, `make textbox-smoke`):

```bash
./scripts/build-sdl3.sh
```

### Unit tests — `make test`

Builds `test_runner` and runs **50 unit test functions** covering:

| Area | What is checked |
|------|-----------------|
| **Parser / codec** | `parse_command`, `parse_note_file` (fixture + malformed + optional Font/FontSize) |
| **Timestamps** | `parse_saved_timestamp_line` round-trip and rejection |
| **Editor core** | insert, erase, movement, undo, find wrap, yank/paste, lines |
| **textbox_input** | init, typing, backspace, multiline Enter, line merge, **wrap/reflow** (columns + pixel width), word-boundary wrap, storage-line collapse, soft-wrap file repair, viewport reset, click-to-cursor, wheel scroll, scrollbar jump |
| **text_font / text_style** | Debug default; font catalog cycle; size presets; style runs; selection toggle/delete |
| **note_store** | `delete_note_file` |
| **gui_theme** | theme save/load roundtrip (`notes/gui_theme.txt`) |
| **sticky_gui** | focus z-order, delete confirm (no re-save after Y), hit targets, F2 title edit, help/delete modals block body input |
| **textbox_sdl** | text input, arrows, Enter, Delete, Esc quit, **key repeat**, panel chrome sizes |

Uses `tests/test_notes_dir.h` so each GUI-related test gets its own temporary `notes/` directory.

### CLI smoke — `make smoke`

Runs `tests/manual_smoke.sh` — **30 automated checks** that pipe commands into `./sticky_notes` and assert stdout + disk state. Covers the full **Manual test checklist (CLI)** below. Backs up and restores your real `notes/` after the run.

### GUI smoke — `make textbox-smoke`

Builds `textbox_test_harness` and runs `tests/textbox_smoke.sh` — **96 headless checks** that drive `sticky_gui_handle_event()` with synthetic SDL keyboard and mouse events. Covers the **Manual test checklist (GUI)** below (except visual polish and pop-out windows, which need a display). Each scenario uses an isolated temp `notes/` directory.

| Scenario group | Examples |
|----------------|----------|
| **Sidebar** | Ctrl+B toggle; click row opens on desk; drag row to pop out |
| **Startup** | One most-recent note on desk; rest in sidebar; state in `notes/desk_state.txt` |
| **Focus** | Click panel raises z-order; **hover** brings panel to front |
| **Body edit** | Type, Enter multiline, Ctrl+S save; **wrap at panel width**; **reflow on widen** |
| **Close** | Ctrl+W saves then removes panel; title-bar **x** same |
| **Delete file** | Ctrl+Shift+W → Y removes file (no resurrection on re-init); N/Esc cancel |
| **Title** | F2 rename + persist; Esc cancel; whitespace → `Untitled` |
| **Modals** | H/F1 help blocks typing; delete confirm blocks stray keys |
| **Open / find / undo** | Ctrl+O picker; Ctrl+F find; Ctrl+K sidebar search; Ctrl+Z/Y undo/redo; F3 find next |
| **Themes** | Ctrl+T cycle; Ctrl+1/2/3 presets; **persisted in `notes/gui_theme.txt`** |
| **Create** | Ctrl+N + save creates `notes/note_<id>.txt` |
| **Mouse** | Desk: click body / title (dbl-click pops out); pop-out: drag title, resize grip |
| **Quit path** | `sticky_gui_save_all`, Esc sets quit (desk only) |
| **SDL seam** | `textbox_handle_sdl_event` Enter, Delete, backspace merge |

**Regression:** includes automated coverage for **BUG-005** (delete-from-disk must not re-save via `close_panel_at`).

### Interactive GUI — manual only

```bash
make textbox
./textbox_sandbox
```

Requires a display. Not run by `make smoke` or `make textbox-smoke`.

### Desktop shortcut (Linux)

Install a launcher in your app menu (no `sudo`):

```bash
make desktop
```

Then search for **Sticky Notes** in your desktop environment’s application menu. The launcher runs `scripts/run-sticky-notes-gui.sh`, which `cd`s to the project root (so `notes/` resolves correctly) and builds the GUI on first launch if needed.

You can also pin the installed entry to your dock/taskbar, or run `./scripts/run-sticky-notes-gui.sh` directly.

If you move the repo later, rerun `make desktop` so the launcher paths point at the new location.

The launcher, SDL app id (`sticky-notes`), and `StartupWMClass` in the `.desktop` file must agree so GNOME/KDE show the notepad icon on the dock for running windows — not a generic fallback icon. After updating launcher scripts, run `make desktop` (refreshes the icon theme cache), rebuild with `make textbox`, quit any running copy, and launch again from the app menu.

## Phase 2 — SDL3 textbox sandbox

Single-line GUI textbox using **SDL3** and the same `EditorSession` / editor core as the CLI — no toolkit text widget.

## Phase 3 — Multiline sticky-note panel

Multiline textbox in one **sticky-note panel** (title bar + scrollable body). Same `EditorSession` / `note.text` lines as the terminal.

| Input | Action |
|-------|--------|
| Type | Insert at cursor |
| **Enter** | Split line at cursor (`newline`) |
| **Backspace** at line start | Merge with previous line |
| **Backspace** / **Delete** | Erase before / at cursor |
| **Arrow keys** | Move cursor (including across lines) |
| **Home** / **End** | Start / end of current line |

Rendering uses the note’s body typography (default **Debug 8×8**; optional TTF families via stb_truetype). Character styles (bold/italic/underline/strike) decorate glyphs. Body lines outside the panel clip via scroll (`TextboxViewport`); a **scrollbar** appears when content overflows. Input: `textbox_apply_key` → silent `note_editor` core.

## Phase 4 — Multi-note GUI desk

`./textbox_sandbox` is a small sticky-notes desk with optional **floating pop-out windows**. The desk window is **borderless** (no OS title bar): drag the top chrome to move; use **− / + / x** for minimize, maximize, and close; resize from the window edges.

| Input | Action |
|-------|--------|
| **Click panel** | Focus (brings to front) |
| **Click body** | Place text cursor under the pointer |
| **Mouse wheel** | Scroll long notes in the focused panel / pop-out |
| **Body scrollbar** | Appears when the note is taller than the panel; click track or drag thumb |
| **Hold key** | Repeat typing, Backspace, Delete, and arrow movement |
| **Hover panel** | Focus without clicking (when not dragging) |
| **Click x** on title bar | Close panel (saves first) |
| **Double-click title bar** | **Pop out** focused note to its own always-on-top window |
| **Ctrl+Shift+P** | Pop out focused note (same as double-click title) |
| **F2** | Rename note title |
| **Ctrl+O** | Open note picker — numbered list (`1. Title`); loads from `notes/` or focuses open panel |
| **Ctrl+S** | Save focused note (shows **Saved** toast) |
| **Ctrl+Z** / **Ctrl+Y** | Undo / redo body edit |
| **Ctrl+F** | Find bar — type needle, **Enter** to search **within** the focused note |
| **F3** | Find next match |
| **Ctrl+K** | Sidebar search — filter notes by title/body keyword; **Enter** keeps filter, **Esc** clears |
| **Ctrl+N** | New note (saved to `notes/`) |
| **Ctrl+W** | Close focused panel on desk (saves first) |
| **Ctrl+Shift+W** | Delete note file from disk (Y/N confirm) |
| **Ctrl+T** | Cycle theme: Minimal → Retro → Cyberpunk |
| **Ctrl+1** / **Ctrl+2** / **Ctrl+3** | Jump to Minimal / Retro / Cyberpunk |
| **Ctrl+Shift+F** | Cycle body font (Debug → Sans → Serif → Mono → Times → Papyrus → Art Deco); toast shows current |
| **Ctrl+=** / **Ctrl+-** | Larger / smaller TTF size presets (no-op on Debug) |
| **Format bar** | Bottom badges: Font / Size dropdowns + **B I U S** toggles |
| **Shift+arrows** / click-drag | Select text; Backspace/Delete removes selection |
| **Ctrl+Shift+B** / **Ctrl+I** / **Ctrl+U** / **Ctrl+Shift+S** | Bold / italic / underline / strikethrough (selection or typing style) |
| **Click Theme badge** | Open theme dropdown (Minimal / Retro / Cyberpunk) |
| **Ctrl+B** | Toggle notes sidebar |
| **Sidebar click** | Show that note on the desk (replaces current desk note) |
| **Sidebar drag** | Pop note out to a floating window |
| **Sidebar scroll** | Wheel over the list, or drag the scrollbar thumb when the list overflows |
| **Esc** | Quit desk app (saves all notes with paths); cancels title edit / confirm on desk |

**Pop-out window** (after double-click title or Ctrl+Shift+P) — frameless (no OS title bar; only the themed panel chrome). Title drag uses global screen coordinates; rendering is paused during drag to avoid compositor ghosting (documented as BUG-006 / BUG-007 in `docs/portfolio/sticky-notes/bug-log.md`).

| Input | Action |
|-------|--------|
| **Drag title bar** | Move the OS window |
| **Resize grip** | Resize window (body reflows to new width) |
| **Format bar** | Font / Size / **B I U S** (same as desk) |
| **Theme badge** | Switch Minimal / Retro / Cyberpunk (syncs with desk) |
| **Ctrl+T** / **Ctrl+1/2/3** | Cycle / jump theme |
| **v** button | **Dock** back to main desk |
| **Double-click title** | Dock back to desk |
| **x** / window close | Save and close pop-out only (desk keeps running) |
| **Esc** | Does **not** quit the app (desk still open) |

On startup, opens **one note** on the desk (last saved desk note, or most recently edited on disk). All other notes appear in the **sidebar** (sorted by last edited). **Desk state** persists in `notes/desk_state.txt`. Uses the same on-disk format as the CLI. **Theme choice** persists in `notes/gui_theme.txt` across restarts. The desk UI (sidebar, note, format bar, chrome) **refits the window** on resize/maximize; the desk note fills the content area and is **not** user-draggable or resizable (pop-outs keep drag + resize grip). Body text **soft-wraps** to the panel’s glyph width (real advances for TTF); changing window size reflows soft-wrapped lines (manual **Enter** still creates hard line breaks).

### Notes sidebar

- **Ctrl+B** or the **<** / **>** tab on the left edge toggles the sidebar.
- Lists every saved note (most recently edited first). Highlight = on desk.
- **Ctrl+K** (or click the search row) filters by keyword in title or body; **Esc** clears.
- **Click** a row to show that note on the desk.
- **Drag** a row outward to pop it out automatically.
- When the list is longer than the sidebar, a **scrollbar** appears; mouse wheel over the list also scrolls.

### Themes

| Theme | Look |
|-------|------|
| **Minimal** (default) | Dark charcoal desk, warm gold focus — current portfolio aesthetic |
| **Retro** | Win95-style grey desktop, navy title bars, white body, beveled edges |
| **Cyberpunk** | Black desk, matrix-green text, magenta caret, neon borders |

Switch with **Ctrl+T** or **Ctrl+1/2/3**. Theme badge sits at the **bottom of the sidebar** and hides when the sidebar is collapsed. Last choice is saved to `notes/gui_theme.txt` and restored on next launch.

**One-time:** build SDL3 into `third_party/` (gitignored):

```bash
./scripts/build-sdl3.sh
```

**Run the sandbox:**

```bash
make textbox
./textbox_sandbox
```

## Manual test checklist (CLI)

Automated by `make smoke` (30 checks). Backs up and restores your `notes/` directory.

- [ ] First run with `next_note_id.txt` = `0` creates a note and prompts for title.
- [ ] `write hello` then `show` — line 1 shows `hello|` (cursor at end).
- [ ] `goto 1 3` then `insert XX` — yields `heXXllo` on line 1.
- [ ] `left` / `right` move across characters and line boundaries.
- [ ] `undo` restores the previous body after `write` or `insert`.
- [ ] `find world` jumps to a match; `findnext` finds the next one.
- [ ] `yank` then `paste` duplicates the current line below.
- [ ] `newline` on `hello|` splits into `hello` and `` (blank second line).
- [ ] `append world` after `write hello` on the same line yields `helloworld`.
- [ ] `newline` inserts a blank line after the cursor; `delete line` removes the current line.
- [ ] `delete` (no args) still deletes the note **file**; `delete line` only removes one body line.
- [ ] `list` shows `id : title`; `open 0` and `open <exact title>` both load the same note.
- [ ] `view <id>` prints body without changing the active note.
- [ ] `save` then reopen — body and timestamps persist.
- [ ] `create` saves the previous note before creating a new one.
- [ ] `delete` + `y` removes the file; `quit` + `y` saves and exits.

## Manual test checklist (GUI)

Automated by `make textbox-smoke` (96 headless checks). Run `./textbox_sandbox` on your display for visual verification, sidebar drag, and pop-out windows.

- [ ] Startup shows **one** note on desk; others listed in sidebar.
- [ ] **Ctrl+B** toggles sidebar; **<** / **>** tab works.
- [ ] Theme badge at the **bottom of the sidebar** opens/closes the dropdown; click a row to apply theme.
- [ ] **Click** sidebar row swaps desk note; highlight follows.
- [ ] **Drag** sidebar row pops note out to floating window.
- [ ] Many notes: sidebar **scrollbar** appears; wheel / thumb drag scrolls the list.
- [ ] **Hover** over a panel focuses it without clicking.
- [ ] Click panel focuses it; **click in the body** places the cursor; body **wraps** at panel edge; desk note has **no** resize grip.
- [ ] **Hold** a letter, Backspace, Delete, or arrow key — action repeats while held.
- [ ] **Mouse wheel** scrolls when the note is taller than the panel; body **scrollbar** appears and thumb/track scroll too.
- [ ] Maximize / resize the desk window — sidebar, note, and format bar **refit**; text **reflows** to the new width.
- [ ] **Ctrl+O** opens note picker with **numbered titles**; selecting open note focuses without duplicate.
- [ ] **Ctrl+S** saves and shows toast; **Ctrl+Z/Y** undo/redo in body.
- [ ] **Ctrl+F** + **Enter** finds text; **F3** find next.
- [ ] **Ctrl+K** filters sidebar by title/body; **Esc** clears search.
- [ ] **Ctrl+T** and **Ctrl+1/2/3** switch themes; restart app — **theme persists**.
- [ ] **Ctrl+Shift+F** cycles fonts; format bar Font/Size dropdowns work; **Ctrl+=/-** change size; save/reload keeps Font fields.
- [ ] Select with Shift+arrows or click-drag; **B/I/U/S** (bar or shortcuts) style the selection; Styles: persist on save.
- [ ] Desk note fills the content area; popped-out note uses compact floating size with resize grip.
- [ ] **Double-click title** or **Ctrl+Shift+P** pops note to always-on-top window.
- [ ] Pop-out: drag title moves window; **format bar** Font/Size/B I U S and **Theme** work; **v** or double-click title **docks** back; **x** closes pop-out only.
- [ ] **Ctrl+W** / title-bar **x** closes desk panel (saves first).
- [ ] **Ctrl+Shift+W** → Y deletes file from disk; N/Esc cancels.
- [ ] **H** / **F1** help overlay; **Esc** on desk quits and saves all notes with paths.

## License

**All rights reserved** — see [LICENSE](LICENSE). This is not an open-source license; viewing or forking the public GitHub repo does not grant permission to reuse the code in other products.

## Limitations

- **CLI:** one active note at a time; switching uses `open` / `create`.
- **Titles** for `open` / `view` must match **exactly** when not using a numeric id.
- Opening by id when the argument is all digits; titles that are only digits will be treated as ids.
- **GUI:** up to 8 panels on desk; unlimited **pop-out** windows; body default is Debug 8×8; extra fonts + B/I/U/S; TTF glyphs use proportional advances and **pixel-width** soft wrap (Debug/tests still use column wrap via average cell width).
- **Open picker** shows numbered list position, not storage id (CLI `list` / `open <id>` still use file ids).
- **GUI testing:** `make textbox-smoke` covers desk logic headlessly; pop-out windows and visual polish need `./textbox_sandbox` on a display.
- **CI:** interactive sandbox not run in automated targets; use `make test`, `make smoke`, and `make textbox-smoke`.
