#include "textbox_input.h"

namespace {
bool is_printable_ascii(char32_t ch) {
    return ch >= 32 && ch <= 126;
}
} // namespace

void textbox_init_session(EditorSession& session) {
    session = EditorSession{};
    session.note.text.clear();
    session.note.text.push_back("");
    session.current_line = 0;
    session.current_column = 0;
    editor_clear_history(session);
}

bool textbox_apply_key(EditorSession& session, TextboxKeyEvent event) {
    if (session.note.text.empty()) {
	textbox_init_session(session);
    }

    if (session.current_line != 0) {
	session.current_line = 0;
	if (!session.note.text.empty()
	    && session.current_column > session.note.text[0].size()) {
	    session.current_column = session.note.text[0].size();
	}
    }

    switch (event.kind) {
    case TextboxKeyKind::Character:
	if (!is_printable_ascii(event.character)) {
	    return false;
	}
	insert_at_cursor(session, std::string(1, static_cast<char>(event.character)));
	return true;

    case TextboxKeyKind::Backspace:
	erase_char_before(session);
	return true;

    case TextboxKeyKind::Delete:
	delete_at_cursor(session);
	return true;

    case TextboxKeyKind::Left:
	move_left(session);
	return true;

    case TextboxKeyKind::Right:
	move_right(session);
	return true;

    case TextboxKeyKind::Home:
	move_home(session);
	return true;

    case TextboxKeyKind::End:
	move_end(session);
	return true;
    }

    return false;
}

std::string textbox_line_text(const EditorSession& session) {
    if (session.note.text.empty()) {
	return "";
    }
    return session.note.text[0];
}

std::size_t textbox_cursor_column(const EditorSession& session) {
    return session.current_column;
}
