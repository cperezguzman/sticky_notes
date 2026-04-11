#include "sticky_note.h"

#include <filesystem>
#include <iostream>
#include <fstream>

using std::string;
using std::getline;
using std::cout;
using std::cin;
using std::vector;

void print_usage() {
  std::cout << "This is the command list the user can use:\n";
       	    << "write <text user wants to insert>		the input of the user will be added to the body of text\n";
	    << "erase <char/word> <n>				erase the last n characters or words of the most recent input line (default is word and 1)\n";
	    << "save						saves the current note\n";
	    << "delete						deletes the current note\n";
	    << "create						creates new note\n";
	    << "list						lists the notes made\n";
	    << "open <note_name>				opens a note\n";
            << "view <note_name>				prints out the contents of a note\n";
            << "quit						saves the current note and quits the program";
	    << "help						prints out this command menu\n";
}

void set_noteid(sticky_note& sn, string& count) {
    string note_path = "notes/note_" + count + ".txt";
    sn.note_path = note_path;

    std::ofstream out(note_path);
    out.close();

    sn.id = std::stoi(count);

    std::ofstream out("notes/next_note_id.txt");
    out << (sn.id + 1);
    out.close();
}

sticky_note create_note(bool first_time) {
    sticky_note sn;


    if (first_time) {
	cout << "Welcome to Sticky_Note.V1 [Terminal Draft]! This is my starter sticky note project.\n";
	     << "Since it appears to be your first time using this program, I'll automatically create a note for you.\n";
    }

    else {
	cout << "A note will be created.\n";
    }


    cout  << "Please enter its title: ";
    string title;
    getline(cin, title);

    set_title(sn, title);

    std::ifstream in("notes/next_note_id.txt");
    string count;
    getline(in, count);

    set_noteid(sn, count);

    return sn;
}

void write_into_note(sticky_note& sn, vector& fields) {
    sn.text.push_back(fields[1]);
}

void erase_from_note(sticky_note& sn, vector& fields) {
    if (sn.text.empty()) {
	cout << "Error: Nothing to erase yet. Please enter another command.\n";
	continue;
    }

    if (fields.size() == 1) {
	while (sn.text.back().back() != ' ') {
	    sn.text.back().pop_back();
	}
	cout << "The most recent word has been deleted.\n";
    }

    else if (fields[1] == "char") {
	if (fields.size() == 2) {
	    sn.text.back().pop_back();
	    cout << "The most recent character has been deleted.\n";
	}

	else if (fields.size() == 3) {
	    sn.text.back().resize(sn.text.back().size() - fields[2]);
	    cout << "The last " << fields[2] << "characters have been deleted.\n";
	}

	else {
	    cout << "Error: Too many arguments passed for the 'erase' command. Please try again.\n";
	    continue;
	}
    }

    else if (fields[1] == "word") {
	if (fields.size() == 2) {
	    while (sn.text.back().back() != ' ') {
	        sn.text.back().pop_back();
	    }

	    cout << "The most recent word has been deleted.\n";
	}

	else if (fields.size() == 3) {
	    for (int i = 0; i < fields[2]; i++) {
		while (sn.text.back().back() != ' ') {
		    sn.text.back().pop_back();
		}
		sn.text.back().pop_back();
	    }

	    cout << "The last " << fields[2] << "characters have been deleted.\n";
	}

	else {
	    cout << "Error: Too many arguments passed for the 'erase' command. Please try again.\n";
	    continue;
	}
    }
}

void open_note() {
    cout << "Please enter the note title 
}

int main() {
    std::ifstream in("notes/next_note_id.txt");

    if (!in) {
	std::cerr << "Error: Could not find file that contains the next viable id number.\n";
	return 1;
    }

    string note_num;
    std::ifstream in("notes/next_note_id.txt");
    getline(in, note_num);

    bool first_time;
    if (note_num == "0") {first_time = true;}

    string choice;
    if (first_time) {sticky_note sn = create_note(first_time);}
    else {
	cout << "Welcome back. Would you like to open a note or create a note. (open/create): \n";
        cin >> choice;
    }

    if (choice == "open") {
	open_note();
    }

    print_usage();

    while(true) {
        string command;

        cout << "Please enter a command: ";
        getline(std::cin, command);

        if (command.empty()) {
	    cout << "Error: no command entered. Please try again.\n";
	    continue;
        }

        std::vector<std::string> fields = parse_command(command);

	if (fields[0] == "write " {write_to_note(sn

	else if (fields[0] == "erase") {
	    
	}

	else if (fields[0] == "save") {
	    std::ofstream out(sn.note_path);

	    for (const auto& t : text) {
		out << t << "\n";
	    }

	    out.close();
	}

	else if (fields[0] == "delete") {
	    string choice;

	    cout << "This will permanently delete this note from memory.\n";
	         << "Are you sure? (y/n): ";
	    cin >> choice;

	    while ((choice != "y") && (choice != "n")) {
	        cout << "Error: Input is not a choice. Please input the command and try again.\n";

	        cout << "Are you sure you want to exit the delete this note? (y/n): \n";
	        cin >> choice;
	    }

	    if (choice == "y") {
		std::filesystem::remove(path);
		while (!sn.text.empty()) {
		    sn.text.pop_back();
		}

		return 0;
	    }

	    else if (choice == "n") {
		cout << "Returning to command menu\n";
		print_usage();
		continue;
	    }
	}

	else if (fields[0] == "create") {
	    sticky_note create_note();
        }


	else if (fields[0] == "quit") {
	    string choice;

	    cout << "Are you sure you want to exit the program? (y/n): \n";
	    cin >> choice;

	    while ((choice != "y") && (choice != "n")) {
	        cout << "Error: Input is not a choice. Please input the command and try again.\n";

	        cout << "Are you sure you want to exit the program? (y/n): \n";
	        cin >> choice;
	    }

	    if (choice == "y") {
	        cout << "Saved current note. Thank you for using my lil project.\n";
	        return 0;
	    }

	    else if (choice == "n") {
	        cout << "Going back to command menu.\n";
	        print_usage();
	        continue;
	    }
	}

	else if (fields[0] == "help") {print_usage();}
