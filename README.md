# Sticky Notes

A **C++20** sticky-notes app with two front ends over the same local store:

- **Terminal editor** — command-driven editing (line/cursor ops, undo/redo, find, yank/paste, list/open/export).
- **SDL3 desk GUI** — draggable/resizable note panels, themes, sidebar, and always-on-top pop-out windows.

Notes are **plain text files** under `notes/`, with a monotonic ID counter so each new note gets a stable `note_<id>.txt` filename. Both UIs share the same load/save codec and edit core; the project doubles as a learning path toward a custom GUI textbox (platform-neutral input seam → SDL adapter → multi-panel desk).

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

  `Title:`, `ID:`, `Created:`, `Last Edited:`, `Body:` (each label on its own line, value on the following line(s); body is the rest of the file).

Opening a note **reloads** title, id, body, and parses **Created** / **Last Edited** lines back into `std::chrono` (English month names; uses `en_US.UTF-8` locale when available). If a line is missing or invalid, that field falls back to the current time, and `last_edited` is clamped so it is not before `created`.

## Project layout

```
src/
  main.cpp              — CLI loop and command dispatch
  note_editor.cpp/.h    — silent edit core (`EditStatus`, typed erase API)
  note_editor_cli.cpp/.h — terminal presentation adapter
  note_store.cpp/.h     — load/save, list, open/view by title or id
  parser.cpp/.h      — CLI command parsing
  note_file_codec.*  — on-disk note file format (`ParsedNoteFile`)
  sticky_note.cpp/.h — `sticky_note` struct; timestamps
  textbox_input.*    — platform-neutral multiline key seam (Phase 2–3)
  textbox_sdl.*      — SDL3 adapter + themed panel render
  gui_theme.*        — desk/panel color themes (Minimal, Retro, Cyberpunk); persists to notes/gui_theme.txt
  sticky_gui.*       — Phase 4: multiple panels, drag, resize, load/save
  sticky_popup.*     — Phase 4+: pop-out notes as always-on-top SDL windows
sandbox/
  textbox_main.cpp   — multi-window SDL loop (desk + floating pop-outs)
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
  textbox_harness.cpp   — Phase 4 integration harness (83 headless checks)
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

Builds `test_runner` and runs **35 unit test functions** covering:

| Area | What is checked |
|------|-----------------|
| **Parser / codec** | `parse_command`, `parse_note_file` (fixture + malformed) |
| **Timestamps** | `parse_saved_timestamp_line` round-trip and rejection |
| **Editor core** | insert, erase, movement, undo, find wrap, yank/paste, lines |
| **textbox_input** | init, typing, backspace, multiline Enter, line merge, **wrap/reflow** |
| **note_store** | `delete_note_file` |
| **gui_theme** | theme save/load roundtrip (`notes/gui_theme.txt`) |
| **sticky_gui** | focus z-order, delete confirm (no re-save after Y), hit targets, F2 title edit, help/delete modals block body input |
| **textbox_sdl** | text input, arrows, Enter, Delete, Esc quit, panel chrome sizes |

Uses `tests/test_notes_dir.h` so each GUI-related test gets its own temporary `notes/` directory.

### CLI smoke — `make smoke`

Runs `tests/manual_smoke.sh` — **30 automated checks** that pipe commands into `./sticky_notes` and assert stdout + disk state. Covers the full **Manual test checklist (CLI)** below. Backs up and restores your real `notes/` after the run.

### GUI smoke — `make textbox-smoke`

Builds `textbox_test_harness` and runs `tests/textbox_smoke.sh` — **83 headless checks** that drive `sticky_gui_handle_event()` with synthetic SDL keyboard and mouse events. Covers the **Manual test checklist (GUI)** below (except visual polish and pop-out windows, which need a display). Each scenario uses an isolated temp `notes/` directory.

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
| **Open / find / undo** | Ctrl+O picker (numbered list); Ctrl+F find; Ctrl+Z/Y undo/redo; F3 find next |
| **Themes** | Ctrl+T cycle; Ctrl+1/2/3 presets; **persisted in `notes/gui_theme.txt`** |
| **Create** | Ctrl+N + save creates `notes/note_<id>.txt` |
| **Mouse** | Drag title bar, resize grip |
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

Rendering uses SDL’s debug bitmap font (`SDL_RenderDebugText`). Body lines outside the panel clip via scroll (`TextboxViewport`). Input: `textbox_apply_key` → silent `note_editor` core.

## Phase 4 — Multi-note GUI desk

`./textbox_sandbox` is a small sticky-notes desk with optional **floating pop-out windows**:

| Input | Action |
|-------|--------|
| **Click panel** | Focus (brings to front) |
| **Hover panel** | Focus without clicking (when not dragging) |
| **Click x** on title bar | Close panel (saves first) |
| **Drag title bar** | Move panel on the desk |
| **Double-click title bar** | **Pop out** focused note to its own always-on-top window |
| **Ctrl+Shift+P** | Pop out focused note (same as double-click title) |
| **Drag bottom-right grip** | Resize focused panel |
| **F2** | Rename note title |
| **Ctrl+O** | Open note picker — numbered list (`1. Title`); loads from `notes/` or focuses open panel |
| **Ctrl+S** | Save focused note (shows **Saved** toast) |
| **Ctrl+Z** / **Ctrl+Y** | Undo / redo body edit |
| **Ctrl+F** | Find bar — type needle, **Enter** to search |
| **F3** | Find next match |
| **Ctrl+N** | New note (saved to `notes/`) |
| **Ctrl+W** | Close focused panel on desk (saves first) |
| **Ctrl+Shift+W** | Delete note file from disk (Y/N confirm) |
| **Ctrl+T** | Cycle theme: Minimal → Retro → Cyberpunk |
| **Ctrl+1** / **Ctrl+2** / **Ctrl+3** | Jump to Minimal / Retro / Cyberpunk |
| **Click Theme badge** | Open theme dropdown (Minimal / Retro / Cyberpunk) |
| **Ctrl+B** | Toggle notes sidebar |
| **Sidebar click** | Show that note on the desk (replaces current desk note) |
| **Sidebar drag** | Pop note out to a floating window |
| **Esc** | Quit desk app (saves all notes with paths); cancels title edit / confirm on desk |

**Pop-out window** (after double-click title or Ctrl+Shift+P) — frameless (no OS title bar; only the themed panel chrome). Title drag uses global screen coordinates; rendering is paused during drag to avoid compositor ghosting (documented as BUG-006 / BUG-007 in `docs/portfolio/sticky-notes/bug-log.md`).

| Input | Action |
|-------|--------|
| **Drag title bar** | Move the OS window |
| **Resize grip** | Resize window (body reflows to new width) |
| **v** button | **Dock** back to main desk |
| **Double-click title** | Dock back to desk |
| **x** / window close | Save and close pop-out only (desk keeps running) |
| **Esc** | Does **not** quit the app (desk still open) |

On startup, opens **one note** on the desk (last saved desk note, or most recently edited on disk). All other notes appear in the **sidebar** (sorted by last edited). **Desk state** persists in `notes/desk_state.txt`. Uses the same on-disk format as the CLI. **Theme choice** persists in `notes/gui_theme.txt` across restarts. The desk note expands to fill most of the desk area; pop-out windows use a smaller floating size. Body text **wraps** to panel width; narrowing then widening a panel reflows soft-wrapped lines (manual **Enter** still creates hard line breaks).

### Notes sidebar

- **Ctrl+B** or the **<** / **>** tab on the left edge toggles the sidebar.
- Lists every saved note (most recently edited first). Highlight = on desk.
- **Click** a row to show that note on the desk.
- **Drag** a row outward to pop it out automatically.

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

Automated by `make textbox-smoke` (83 headless checks). Run `./textbox_sandbox` on your display for visual verification, sidebar drag, and pop-out windows.

- [ ] Startup shows **one** note on desk; others listed in sidebar.
- [ ] **Ctrl+B** toggles sidebar; **<** / **>** tab works.
- [ ] Theme badge at the **bottom of the sidebar** opens/closes the dropdown; click a row to apply theme.
- [ ] **Click** sidebar row swaps desk note; highlight follows.
- [ ] **Drag** sidebar row pops note out to floating window.
- [ ] **Hover** over a panel focuses it without clicking.
- [ ] Click panel focuses it; drag title bar moves; grip resizes; body **wraps** at panel edge.
- [ ] Narrow then widen a panel — text **reflows** to use full width (Enter breaks stay separate).
- [ ] **Ctrl+O** opens note picker with **numbered titles**; selecting open note focuses without duplicate.
- [ ] **Ctrl+S** saves and shows toast; **Ctrl+Z/Y** undo/redo in body.
- [ ] **Ctrl+F** + **Enter** finds text; **F3** find next.
- [ ] **Ctrl+T** and **Ctrl+1/2/3** switch themes; restart app — **theme persists**.
- [ ] Desk note uses most of the main window; popped-out note reopens in compact floating size.
- [ ] **Double-click title** or **Ctrl+Shift+P** pops note to always-on-top window.
- [ ] Pop-out: drag title moves window; **v** or double-click title **docks** back; **x** closes pop-out only.
- [ ] **Ctrl+W** / title-bar **x** closes desk panel (saves first).
- [ ] **Ctrl+Shift+W** → Y deletes file from disk; N/Esc cancels.
- [ ] **H** / **F1** help overlay; **Esc** on desk quits and saves all notes with paths.

## Limitations

- **CLI:** one active note at a time; switching uses `open` / `create`.
- **Titles** for `open` / `view` must match **exactly** when not using a numeric id.
- Opening by id when the argument is all digits; titles that are only digits will be treated as ids.
- **GUI:** up to 8 panels on desk; unlimited **pop-out** windows; `SDL_RenderDebugText` is ASCII 8×8 only; no rich text or system font.
- **Open picker** shows numbered list position, not storage id (CLI `list` / `open <id>` still use file ids).
- **GUI testing:** `make textbox-smoke` covers desk logic headlessly; pop-out windows and visual polish need `./textbox_sandbox` on a display.
- **CI:** interactive sandbox not run in automated targets; use `make test`, `make smoke`, and `make textbox-smoke`.
