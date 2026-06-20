#!/usr/bin/env bash
# README manual checklist — run from repo root: ./tests/manual_smoke.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BIN="./sticky_notes"
FAIL=0
PASS=0

pass() { echo "PASS: $1"; PASS=$((PASS + 1)); }
fail() { echo "FAIL: $1"; FAIL=$((FAIL + 1)); }

assert_contains() {
    local label="$1" haystack="$2" needle="$3"
    if [[ "$haystack" == *"$needle"* ]]; then
	pass "$label"
    else
	fail "$label (expected substring: $needle)"
    fi
}

assert_not_contains() {
    local label="$1" haystack="$2" needle="$3"
    if [[ "$haystack" != *"$needle"* ]]; then
	pass "$label"
    else
	fail "$label (unexpected substring: $needle)"
    fi
}

run_app() {
    "$BIN" 2>&1
}

write_note_file() {
    local id="$1" title="$2" body="$3"
    cat > "notes/note_${id}.txt" <<EOF
Title:
${title}
ID:
${id}
Created:
Created: June 20, 2026 at 12:00 PM
Last Edited:
Last Edited: June 20, 2026 at 12:00 PM
Body:
${body}
EOF
}

reset_note0() {
    write_note_file 0 "Editor Test" ""
}

BACKUP="$(mktemp -d)"
cleanup() {
    if [[ -d "$BACKUP/notes" ]]; then
	rm -rf notes
	cp -a "$BACKUP/notes" notes
    fi
    rm -rf "$BACKUP"
}
trap cleanup EXIT

cp -a notes "$BACKUP/"

if [[ ! -x "$BIN" ]]; then
    make sticky_notes
fi

# --- 1. First run with counter 0 ---
rm -f notes/note_*.txt
echo 0 > notes/next_note_id.txt
OUT=$(printf 'First Run Title\nwrite hello\nshow\nquit\ny\n' | run_app)
assert_contains "first run prompts for title" "$OUT" "first time"
assert_contains "first run write hello show" "$OUT" "> 1: hello|"
[[ -f notes/note_0.txt ]] && pass "first run creates note_0.txt" || fail "first run creates note_0.txt"
[[ "$(cat notes/next_note_id.txt)" == "1" ]] && pass "first run increments counter" || fail "first run increments counter"

echo 2 > notes/next_note_id.txt

# --- 2. write + show ---
reset_note0
OUT=$(printf 'open\n0\nwrite hello\nshow\nquit\ny\n' | run_app)
assert_contains "write then show cursor at end" "$OUT" "> 1: hello|"

# --- 3. goto + insert ---
reset_note0
OUT=$(printf 'open\n0\nwrite hello\ngoto 1 3\ninsert XX\nshow\nquit\ny\n' | run_app)
assert_contains "goto insert XX" "$OUT" "heXX|llo"

# --- 4. left / right across lines ---
reset_note0
OUT=$(printf 'open\n0\nwrite ab\nnewline\nwrite cd\nleft\nleft\nleft\nshow\nright\nright\nright\nshow\nquit\ny\n' | run_app)
assert_contains "left crosses to line 1" "$OUT" "> 1: ab|"
assert_contains "right crosses to line 2" "$OUT" "> 2: cd|"

# --- 5. undo ---
reset_note0
OUT=$(printf 'open\n0\nwrite first\nwrite second\nundo\nshow\nquit\ny\n' | run_app)
assert_contains "undo restores previous body" "$OUT" "> 1: first|"

# --- 6. find / findnext ---
reset_note0
OUT=$(printf 'open\n0\nwrite foo world world\nfind world\nfindnext\npos\nquit\ny\n' | run_app)
assert_contains "find locates match" "$OUT" "Match found"
assert_contains "findnext locates second match" "$OUT" "Cursor on line 1, column 11"

# --- 7. yank / paste ---
reset_note0
OUT=$(printf 'open\n0\nwrite copyme\nyank\npaste\nshow\nquit\ny\n' | run_app)
assert_contains "yank paste keeps original line" "$OUT" "1: copyme"
assert_contains "yank paste adds duplicate below" "$OUT" "> 2: copyme|"

# --- 8. newline split ---
reset_note0
OUT=$(printf 'open\n0\nwrite hello\ngoto 1 6\nnewline\nshow\nquit\ny\n' | run_app)
assert_contains "newline split keeps hello on line 1" "$OUT" "1: hello"
assert_contains "newline split adds blank line 2" "$OUT" "> 2: |"

# --- 9. append ---
write_note_file 1 "Append Test" ""
OUT=$(printf 'open\ncreate\nAppend Test\nwrite hello\nappend world\nshow\nquit\ny\n' | run_app)
assert_contains "append concatenates" "$OUT" "helloworld|"

# --- 10. newline blank + delete line ---
write_note_file 1 "Append Test" "x"
OUT=$(printf 'open\nAppend Test\ngoto 1 2\nnewline\ndelete line\nshow\nquit\ny\n' | run_app)
assert_contains "delete line keeps first line content" "$OUT" "> 1:"
assert_contains "delete line keeps x on line 1" "$OUT" "|x"
assert_not_contains "delete line removes second line" "$OUT" "> 2:"

# --- 11. delete line vs delete file ---
write_note_file 99 "Delete Me" "line"
OUT=$(printf 'open\n99\nwrite a\nnewline\nwrite b\ndelete line\nshow\nquit\ny\n' | run_app)
assert_contains "delete line keeps content" "$OUT" "> 1: a|"
[[ -f notes/note_99.txt ]] && pass "delete line does not remove file" || fail "delete line does not remove file"

# --- 12. list / open by id and title ---
write_note_file 0 "Editor Test" "body"
write_note_file 1 "List Test" "x"
OUT=$(printf 'open\n0\nlist\nopen 1\nshow\nopen\nEditor Test\nshow\nquit\ny\n' | run_app)
assert_contains "list shows id : title" "$OUT" "0 : Editor Test"
assert_contains "open by id loads note" "$OUT" "Opened note \"List Test\""
assert_contains "open by exact title loads note" "$OUT" "Opened note \"Editor Test\""

# --- 13. view without switching ---
reset_note0
write_note_file 1 "Other Note" "other body"
OUT=$(printf 'open\n0\nwrite ACTIVE\nview Other Note\nshow\nquit\ny\n' | run_app)
assert_contains "view prints other note body" "$OUT" "--- Other Note ---"
assert_contains "view does not switch active note" "$OUT" "> 1: ACTIVE|"

# --- 14. save and reopen ---
reset_note0
printf 'open\n0\nwrite persisted\ngoto 1 5\ninsert XX\nsave\nquit\ny\n' | run_app >/dev/null
OUT=$(printf 'open\n0\nshow\nquit\ny\n' | run_app)
assert_contains "save persists body" "$OUT" "persXXisted"

# --- 15. create saves previous ---
reset_note0
printf 'open\n0\nwrite before create\ncreate\nSaved By Create\nwrite new body\nquit\ny\n' | run_app >/dev/null
BODY=$(awk '/^Body:$/{flag=1;next} flag{print}' notes/note_0.txt | head -1)
[[ "$BODY" == "before create" ]] && pass "create saves previous note" || fail "create saves previous note (got: '$BODY')"

# --- 16. delete file + quit ---
write_note_file 88 "Trash" "bye"
OUT=$(printf 'open\n88\ndelete\ny\nquit\ny\n' | run_app)
[[ ! -f notes/note_88.txt ]] && pass "delete y removes file" || fail "delete y removes file"
assert_contains "quit y saves and exits" "$OUT" "Thank you for using"

echo ""
echo "Results: $PASS passed, $FAIL failed"
[[ "$FAIL" -eq 0 ]]
