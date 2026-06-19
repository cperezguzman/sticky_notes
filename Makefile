CXX      ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -O0 -g -Isrc

SRCS     = src/main.cpp src/parser.cpp src/sticky_note.cpp src/note_store.cpp src/note_editor.cpp
HEADERS  = src/sticky_note.h src/parser.h src/note_store.h src/note_editor.h

TEST_SRCS = tests/test_main.cpp src/parser.cpp src/sticky_note.cpp src/note_editor.cpp

.PHONY: all clean test

all: sticky_notes

sticky_notes: $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

test: test_runner
	./test_runner

test_runner: $(TEST_SRCS) src/sticky_note.h src/parser.h src/note_editor.h
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_SRCS)

clean:
	rm -f sticky_notes test_runner
