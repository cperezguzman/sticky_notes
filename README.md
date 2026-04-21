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
g++ -std=c++20 -Wall -Wextra -o sticky_notes src/main.cpp src/parser.cpp src/sticky_note.cpp
```

## Run

From the repository root (so `notes/next_note_id.txt` and `notes/note_*.txt` are found):

```bash
./sticky_notes
```

**First run:** if `notes/next_note_id.txt` contains `0`, the program creates your first note and walks you through a title.

**Later runs:** you can **open** an existing note (by **exact title** as shown in `list`) or **create** a new one.

## Commands

| Command | Description |
|--------|-------------|
| `write <text>` | Append **one line** to the body (everything after `write ` on the same line). |
| `erase` | Erase the **last word** on the current line (same as `erase word` with one word). |
| `erase char` | Delete the last character on the current line. |
| `erase char <n>` | Delete up to `n` characters from the end of the line. |
| `erase word` | Delete the last **word** (non-space suffix) on the current line. |
| `erase word <n>` | Delete up to `n` trailing words on the current line. |
| `save` | Write the current note to its file. |
| `create` | Create a new note; the **current** note is saved first if it has a path. |
| `list` | Print **id : title** for each `notes/note_*.txt` file that parses correctly. |
| `open <title>` | Load a note by **exact title** (same string as in `list`). Title can contain spaces. |
| `view <title>` | Print that note’s body without switching the active note. |
| `delete` | Delete the current note’s file from disk (with confirmation); clears in-memory state. |
| `quit` | Save the current note (if it has a path) and exit (with confirmation). |
| `help` | Show the built-in command summary. |

Unknown commands print a short error; use `help` for the full list.

## On-disk layout

- `notes/next_note_id.txt` — next numeric id to assign (one integer per line).
- `notes/note_<id>.txt` — note file; format is sectioned text:

  `Title:`, `ID:`, `Created:`, `Last Edited:`, `Body:` (each label on its own line, value on the following line(s); body is the rest of the file).

Opening a note **reloads** title, id, and body lines. Timestamps in memory are set to the load time unless you extend the code to parse the saved date strings.

## Project layout

```
src/
  main.cpp        — CLI loop, note I/O, erase/save/list/open/view
  parser.cpp/.h   — command parsing; note file parsing
  sticky_note.cpp/.h — `sticky_note` struct; created / last-edited formatting
notes/            — data directory (tracked or gitignored per your preference)
Makefile
```

## Limitations

- One **active** note at a time; switching uses `open` / `create`.
- **Titles** for `open` / `view` must match **exactly** (whitespace matters after trimming user input in some prompts).
- No GUI, undo stack, or rich text—by design for this version.


