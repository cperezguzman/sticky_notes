#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>

using std::getline;

namespace {
    std::string trim(const std::string& command) {
	auto start = command.find_first_not_of(" \t");
	if (start == std::string::npos) {return "";}
	auto end = command.find_last_not_of(" \t");
	return command.substr(start, (end - start) + 1);
    }
}

std::vector<std::string> parse_command(const std::string& command) {
    std::vector<std::string> fields;

    std::string part;

    std::istringstream iss(trim(command));
    getline(iss, part, ' ');
 
    fields.push_back(part);

    if (fields[0] == "write") {
	// FIXME: Edge cases: empty remainder after "write", leading space after command — trim or treat as empty line if needed.
	getline(iss, part);
	fields.push_back(part);
    } else {
	while (getline(iss, part, ' ')) {
	    fields.push_back(part);
        }
    }

    return fields;
}

// return a vector of different note info
// FIXME: Consider std::optional<std::vector<std::string>> or a struct instead of out-param `ok` for clearer errors.
std::vector<std::string> parse_file_info(std::ifstream& file, bool& ok) {
    std::vector<std::string> fields;

    std::string part;

    while (getline(file, part)) {
	// FIXME: create_note writes lines like `Title: foo` on ONE line — these `part == "Title:"` checks expect a different layout. Align file format with parser (or parse key: value on same line).
	if (part == "Title:") {
	    getline(file, part);
	    fields.push_back(part);
	} else if (part == "ID:") {
	    getline(file, part);
	    fields.push_back(part);
	} else if (part == "Created:") {
	    getline(file, part);
	    fields.push_back(part);
	} else if (part == "Last Edited:") {
	    getline(file, part);
	    fields.push_back(part);
	} else if (part == "Body:") {
	    getline(file, part);
	    fields.push_back(part);
	} else {
	    std::cout << "Error: File containing note is not formatted correctly; cannot parse.\n";
	    ok = false; //tells main.cpp that the file is incorrect and main will then fail the corresponding command
	}
    }

    return fields;
}
