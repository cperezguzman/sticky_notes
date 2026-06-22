CXX      ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -O0 -g -Isrc

SDL3_PREFIX ?= $(CURDIR)/third_party/sdl3-install
SDL3_CFLAGS  = -Isrc -I$(SDL3_PREFIX)/include
SDL3_LIBS    = -L$(SDL3_PREFIX)/lib -Wl,-rpath,$(SDL3_PREFIX)/lib -lSDL3

SRCS     = src/main.cpp src/parser.cpp src/sticky_note.cpp src/note_store.cpp src/note_editor.cpp src/note_editor_cli.cpp
HEADERS  = src/sticky_note.h src/parser.h src/note_store.h src/note_editor.h src/note_editor_cli.h src/textbox_input.h

TEST_SRCS = tests/test_main.cpp src/parser.cpp src/sticky_note.cpp src/note_editor.cpp src/note_store.cpp src/textbox_input.cpp

TEXTBOX_SRCS = sandbox/textbox_main.cpp src/textbox_input.cpp src/textbox_sdl.cpp src/note_editor.cpp src/sticky_note.cpp

.PHONY: all clean test smoke textbox sdl3

all: sticky_notes

sticky_notes: $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

test: test_runner
	./test_runner

smoke: sticky_notes
	./tests/manual_smoke.sh

test_runner: $(TEST_SRCS) src/sticky_note.h src/parser.h src/note_editor.h src/note_store.h src/textbox_input.h
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_SRCS)

# Phase 2: single-line SDL3 textbox sandbox (requires `make sdl3` once).
textbox: textbox_sandbox

textbox_sandbox: $(TEXTBOX_SRCS) src/textbox_sdl.h
	$(CXX) $(CXXFLAGS) $(SDL3_CFLAGS) -o $@ $(TEXTBOX_SRCS) $(SDL3_LIBS)

sdl3:
	./scripts/build-sdl3.sh

clean:
	rm -f sticky_notes test_runner textbox_sandbox
