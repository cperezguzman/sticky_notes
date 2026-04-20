#pragma once

#include <fstream>
#include <string>
#include <vector>

std::vector<std::string> parse_command(const std::string& command);

std::vector<std::string> parse_file_info(std::ifstream& file, bool& ok);
