#include "parser.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using std::getline;

namespace {
std::string trim(const std::string& command) {
    auto start = command.find_first_not_of(" \t");
    if (start == std::string::npos) {
	return "";
    }
    auto end = command.find_last_not_of(" \t");
    return command.substr(start, (end - start) + 1);
}

void strip_trailing_cr(std::string& s) {
    if (!s.empty() && s.back() == '\r') {
	s.pop_back();
    }
}
} // namespace

static bool takes_rest_of_line(const std::string& verb) {
    return verb == "write" || verb == "view" || verb == "open";
}

std::vector<std::string> parse_command(const std::string& command) {
    std::vector<std::string> fields;

    std::string part;

    std::istringstream iss(trim(command));
    getline(iss, part, ' ');

    fields.push_back(part);

    if (takes_rest_of_line(fields[0])) {
	getline(iss, part);
	const std::string body = trim(part);
	if (!body.empty()) {
	    fields.push_back(body);
	}
    } else {
	while (getline(iss, part, ' ')) {
	    if (!part.empty()) {
		fields.push_back(part);
	    }
	}
    }

    return fields;
}

// Parses a note file written by create/save: stream order must be section labels
// Title:, ID:, Created:, Last Edited:, Body: (each followed by its value line; Body
// consumes the remainder of the file). Blank lines before a label are ignored.
// Returns: [0] title, [1] id, [2] created line, [3] last edited line, [4] body.
std::vector<std::string> parse_file_info(std::ifstream& file, bool& ok) {
    ok = true;
    std::vector<std::string> fields;
    fields.resize(5);

    std::string part;
    std::string line;

    auto read_value_line = [&]() -> bool {
	if (!getline(file, line)) {
	    ok = false;
	    return false;
	}
	strip_trailing_cr(line);
	return true;
    };

    while (getline(file, part)) {
	strip_trailing_cr(part);
	if (part.empty()) {
	    continue;
	}

	if (part == "Title:") {
	    if (!read_value_line()) {
		return fields;
	    }
	    fields[0] = line;
	} else if (part == "ID:") {
	    if (!read_value_line()) {
		return fields;
	    }
	    fields[1] = line;
	} else if (part == "Created:") {
	    if (!read_value_line()) {
		return fields;
	    }
	    fields[2] = line;
	} else if (part == "Last Edited:") {
	    if (!read_value_line()) {
		return fields;
	    }
	    fields[3] = line;
	} else if (part == "Body:") {
	    std::ostringstream body;
	    bool first = true;
	    while (getline(file, line)) {
		strip_trailing_cr(line);
		if (!first) {
		    body << '\n';
		}
		body << line;
		first = false;
	    }
	    fields[4] = body.str();
	    break;
	} else {
	    std::cout << "Error: File containing note is not formatted correctly; cannot parse.\n";
	    ok = false;
	    return fields;
	}
    }

    return fields;
}
