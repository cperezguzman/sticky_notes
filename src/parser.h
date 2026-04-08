#include <string>
#include <vector>
#include <sstream>

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
    std::getline(iss, part, ' ');
 
    fields.push_back(part);

    if (fields[0] == "write") {
	fields.push_back(std::getline(iss, part));

    else {
	while (std::get_line(iss, part, ' ')) {
	    fields.push_back(part);
        }
    }

    return fields;
}
