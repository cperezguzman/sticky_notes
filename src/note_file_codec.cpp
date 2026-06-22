#include "note_file_codec.h"

#include <sstream>
#include <string>

namespace {
void strip_trailing_cr(std::string& s) {
    if (!s.empty() && s.back() == '\r') {
	s.pop_back();
    }
}
} // namespace

bool parse_note_file(std::ifstream& file, ParsedNoteFile& out) {
    out = ParsedNoteFile{};

    std::string part;
    std::string line;

    auto read_value_line = [&]() -> bool {
	if (!std::getline(file, line)) {
	    return false;
	}
	strip_trailing_cr(line);
	return true;
    };

    while (std::getline(file, part)) {
	strip_trailing_cr(part);
	if (part.empty()) {
	    continue;
	}

	if (part == "Title:") {
	    if (!read_value_line()) {
		return false;
	    }
	    out.title = line;
	} else if (part == "ID:") {
	    if (!read_value_line()) {
		return false;
	    }
	    out.id = line;
	} else if (part == "Created:") {
	    if (!read_value_line()) {
		return false;
	    }
	    out.created_line = line;
	} else if (part == "Last Edited:") {
	    if (!read_value_line()) {
		return false;
	    }
	    out.last_edited_line = line;
	} else if (part == "Body:") {
	    std::ostringstream body;
	    bool first = true;
	    while (std::getline(file, line)) {
		strip_trailing_cr(line);
		if (!first) {
		    body << '\n';
		}
		body << line;
		first = false;
	    }
	    out.body = body.str();
	    return !out.title.empty() && !out.id.empty();
	} else {
	    return false;
	}
    }

    return !out.title.empty() && !out.id.empty() && !out.body.empty();
}

std::vector<std::string> body_lines_from_parsed(const ParsedNoteFile& parsed) {
    std::vector<std::string> lines;
    if (parsed.body.empty()) {
	return lines;
    }

    std::istringstream body(parsed.body);
    std::string bline;
    while (std::getline(body, bline)) {
	lines.push_back(bline);
    }
    return lines;
}
