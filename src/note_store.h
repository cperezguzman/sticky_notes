#pragma once

#include "sticky_note.h"

#include <map>
#include <string>
#include <utility>

using NoteIndex = std::map<int, std::pair<std::string, std::string>>;

NoteIndex build_note_index();

void list_notes();

const std::string* find_path_by_title(const NoteIndex& idx, const std::string& title);

const std::string* find_path_by_id(const NoteIndex& idx, int id);

bool load_note_from_path(sticky_note& sn, const std::string& path);

void save_note(const sticky_note& sn);

bool open_note_by_title(sticky_note& sn, const std::string& title);

bool open_note_by_id(sticky_note& sn, int id);

void print_note_by_title(const std::string& title);

void print_note_by_id(int id);

sticky_note create_note(bool first_time);
