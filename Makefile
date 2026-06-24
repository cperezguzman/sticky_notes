CXX      ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -O0 -g -Isrc

SDL3_PREFIX ?= $(CURDIR)/third_party/sdl3-install
SDL3_CFLAGS  = -Isrc -I$(SDL3_PREFIX)/include
SDL3_LIBS    = -L$(SDL3_PREFIX)/lib -Wl,-rpath,$(SDL3_PREFIX)/lib -lSDL3

SRCS     = src/main.cpp src/parser.cpp src/note_file_codec.cpp src/sticky_note.cpp src/note_store.cpp src/note_editor.cpp src/note_editor_cli.cpp
HEADERS  = src/sticky_note.h src/parser.h src/note_file_codec.h src/note_store.h src/note_editor.h src/note_editor_cli.h src/textbox_input.h

TEST_SRCS = tests/test_main.cpp src/parser.cpp src/note_file_codec.cpp src/sticky_note.cpp \
	src/note_editor.cpp src/note_store.cpp src/textbox_input.cpp src/sticky_gui.cpp \
	src/textbox_sdl.cpp src/gui_theme.cpp

HARNESS_SRCS = tests/textbox_harness.cpp src/sticky_gui.cpp src/textbox_input.cpp src/textbox_sdl.cpp \
	src/gui_theme.cpp src/note_editor.cpp src/sticky_note.cpp src/note_store.cpp \
	src/note_file_codec.cpp src/parser.cpp

TEXTBOX_SRCS = sandbox/textbox_main.cpp src/sticky_gui.cpp src/sticky_popup.cpp src/textbox_input.cpp src/textbox_sdl.cpp \
	src/gui_theme.cpp src/note_editor.cpp src/sticky_note.cpp src/note_store.cpp \
	src/note_file_codec.cpp src/parser.cpp

.PHONY: all clean test smoke textbox textbox-smoke sdl3 gui desktop

all: sticky_notes

sticky_notes: $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

test: test_runner
	./test_runner

smoke: sticky_notes
	./tests/manual_smoke.sh

textbox-smoke: textbox_test_harness
	./tests/textbox_smoke.sh

test_runner: $(TEST_SRCS) src/sticky_gui.h src/textbox_sdl.h tests/sdl_event_helpers.h tests/test_notes_dir.h
	$(CXX) $(CXXFLAGS) -Itests $(SDL3_CFLAGS) -o $@ $(TEST_SRCS) $(SDL3_LIBS)

textbox_test_harness: $(HARNESS_SRCS) src/sticky_gui.h src/textbox_sdl.h tests/sdl_event_helpers.h tests/test_notes_dir.h
	$(CXX) $(CXXFLAGS) -Itests $(SDL3_CFLAGS) -o $@ $(HARNESS_SRCS) $(SDL3_LIBS)

# Phase 2: single-line SDL3 textbox sandbox (requires `make sdl3` once).
textbox: textbox_sandbox

textbox_sandbox: $(TEXTBOX_SRCS) src/sticky_gui.h src/textbox_sdl.h
	$(CXX) $(CXXFLAGS) $(SDL3_CFLAGS) -o $@ $(TEXTBOX_SRCS) $(SDL3_LIBS)

sdl3:
	./scripts/build-sdl3.sh

# Alias for the SDL desk + pop-out GUI.
gui: textbox

# Install ~/.local/share/applications/sticky-notes.desktop (search "Sticky Notes" in app menu).
desktop:
	./scripts/install-desktop.sh

clean:
	rm -f sticky_notes test_runner textbox_sandbox textbox_test_harness
