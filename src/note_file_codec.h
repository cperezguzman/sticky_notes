#pragma once

#include <fstream>
#include <string>
#include <vector>

// On-disk note file format (notes/note_<id>.txt):
// Title:, ID:, Created:, Last Edited:, Body: — each label on its own line, value follows.
struct ParsedNoteFile {
    std::string title;
    std::string id;
    std::string created_line;
    std::string last_edited_line;
    std::string body;
};

// Returns false on malformed or incomplete files. Does not print to stdout.
bool parse_note_file(std::ifstream& file, ParsedNoteFile& out);

// Split body text into note lines (one vector entry per line).
std::vector<std::string> body_lines_from_parsed(const ParsedNoteFile& parsed);
