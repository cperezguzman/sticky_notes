CXX      ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -O0 -g

.PHONY: all clean

all: sticky_notes

sticky_notes: src/main.cpp src/parser.cpp src/sticky_note.cpp src/sticky_note.h src/parser.h
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp src/parser.cpp src/sticky_note.cpp

clean:
	rm -f sticky_notes
