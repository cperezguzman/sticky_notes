#include "parser.h"

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
} // namespace

static bool takes_rest_of_line(const std::string& verb) {
    return verb == "write" || verb == "append" || verb == "insert" || verb == "rename"
	   || verb == "find" || verb == "view" || verb == "open";
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
