#include "sticky_note.h"
#include "parser.h"

#include <iostream>


void print_usage() {
  std::cout << "This is the command list the user can use:\n";
       	    << "write				the input of the user will be added to the body of text\n";
	    << "erase <char/word> <n>		erase the last n characters or words (default is word and 1)\n";
	    << "save				saves the current note\n";
	    << "delete				deletes the current note\n";
	    << "list				lists the notes made\n";
	    << "open <note_name>		opens a note\n";
            << "view <note_name>		prints out the contents of a note\n";
            << "quit				saves the current note and quits the program";
	    << "help				prints out this command menu\n";
}

int main() {
    sticky_note sn;

    std::cout << "A note is automatically created when starting this program.\n" << "Enter the title: ";
    std::string title;
    std::getline(std::cin, title);
    sn.change_title(title);

    print_usage();

    while(true) {
        std::string command;

        std::cout << "Please enter a command: ";
        std::getline(std::cin, command);

        if (command.empty()) {
	    std::cout << "Error: no command entered. Please try again.\n";
	    continue;
        }

        std::vector<std::string> fields = parse_command(command);

	if (fields[0] == "write") {
	    sn.text.push_back(fields[1]);
	}

	else if (fields[0] == "erase") {
	    if (sn.text.empty()) {
		std::cout << "Nothing to erase yet. Please enter another command.\n";
		continue;
	    }

	    if (fields.size() == 1) {
		sn.text.pop_back();
		std::cout << "The most recent word has been deleted.\n";

	    }
