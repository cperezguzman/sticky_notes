# Sticky Notes (terminal)

A small **C++20** command-line note editor. Notes are **plain text files** under `notes/`, with a monotonic ID counter so each new note gets a stable filename.

## Requirements

- **Compiler:** GCC or Clang with **C++20** (`std::format`, `std::filesystem`, etc.)
- **OS:** Linux is assumed (paths use `/`; run the binary from the project root so `notes/` resolves correctly)

## Build

```bash
make
```

Produces `sticky_notes` in the project directory. Alternatively:

```bash
g++ -std=c++20 -Wall -Wextra -o sticky_notes \
  src/main.cpp src/parser.cpp src/sticky_note.cpp src/note_store.cpp src/note_editor.cpp
```

## Run

From the repository root (so `notes/next_note_id.txt` and `notes/note_*.txt` are found):

```bash
./sticky_notes
```

**First run:** the program creates `notes/` and `notes/next_note_id.txt` if they are missing. When the counter is `0`, it walks you through creating your first note.

**Data:** `notes/` is gitignored — your note files stay local and are not committed.

**Later runs:** you can **open** an existing note (by **exact title** or **numeric id** from `list`) or **create** a new one.

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
| `delete` | Delete the **note file** from disk (with confirmation); clears in-memory state. |
| `quit` | Save the current note (if it has a path) and exit (with confirmation). |
| `help` | Show the built-in command summary. |

Unknown commands print a short error; use `help` for the full list.

## On-disk layout

- `notes/next_note_id.txt` — next numeric id to assign (one integer per line).
- `notes/note_<id>.txt` — note file; format is sectioned text:

  `Title:`, `ID:`, `Created:`, `Last Edited:`, `Body:` (each label on its own line, value on the following line(s); body is the rest of the file).

Opening a note **reloads** title, id, body, and parses **Created** / **Last Edited** lines back into `std::chrono` (English month names; uses `en_US.UTF-8` locale when available). If a line is missing or invalid, that field falls back to the current time, and `last_edited` is clamped so it is not before `created`.

## Project layout

```
src/
  main.cpp           — CLI loop and command dispatch
  note_editor.cpp/.h — line and column cursor; insert, move, erase at cursor
  note_store.cpp/.h  — load/save, list, open/view by title or id
  parser.cpp/.h      — command parsing; note file parsing
  sticky_note.cpp/.h — `sticky_note` struct; timestamps
tests/
  test_main.cpp      — parser and timestamp tests
  fixtures/          — sample note files for tests
notes/               — local data (gitignored; auto-created on first run)
Makefile
```

## Tests

From the project root:

```bash
make test
```

Runs parser and timestamp parsing checks against `tests/fixtures/`.

## Manual test checklist

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

## Limitations

- One **active** note at a time; switching uses `open` / `create`.
- **Titles** for `open` / `view` must match **exactly** when not using a numeric id.
- Opening by id when the argument is all digits; titles that are only digits will be treated as ids.
- No GUI or forward-delete beyond `del`—by design for this version.
