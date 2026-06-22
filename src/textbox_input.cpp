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

    switch (event.kind) {
    case TextboxKeyKind::Character:
	if (!is_printable_ascii(event.character)) {
	    return false;
	}
	insert_at_cursor(session, std::string(1, static_cast<char>(event.character)));
	return true;

    case TextboxKeyKind::Backspace:
	if (session.current_column == 0 && session.current_line > 0) {
	    return join_with_previous_line(session) == EditStatus::Ok;
	}
	return erase_char_before(session) == EditStatus::Ok;

    case TextboxKeyKind::Delete:
	return delete_at_cursor(session) == EditStatus::Ok;

    case TextboxKeyKind::Left:
	move_left(session);
	return true;

    case TextboxKeyKind::Right:
	move_right(session);
	return true;

    case TextboxKeyKind::Up:
	move_up(session);
	return true;

    case TextboxKeyKind::Down:
	move_down(session);
	return true;

    case TextboxKeyKind::Home:
	return move_home(session) == EditStatus::Ok;

    case TextboxKeyKind::End:
	return move_end(session) == EditStatus::Ok;

    case TextboxKeyKind::Newline:
	insert_newline_at_cursor(session);
	return true;
    }

    return false;
}

std::size_t textbox_line_count(const EditorSession& session) {
    return session.note.text.size();
}

std::string textbox_line_at(const EditorSession& session, std::size_t line_index) {
    if (line_index >= session.note.text.size()) {
	return "";
    }
    return session.note.text[line_index];
}

std::size_t textbox_cursor_line(const EditorSession& session) {
    return session.current_line;
}

std::size_t textbox_cursor_column(const EditorSession& session) {
    return session.current_column;
}

std::string textbox_line_text(const EditorSession& session) {
    return textbox_line_at(session, textbox_cursor_line(session));
}

void textbox_scroll_to_cursor(TextboxViewport& viewport, const EditorSession& session,
			      std::size_t visible_line_count) {
    if (visible_line_count == 0) {
	visible_line_count = 1;
    }

    const std::size_t cursor_line = textbox_cursor_line(session);
    if (cursor_line < viewport.first_visible_line) {
	viewport.first_visible_line = cursor_line;
	return;
    }

    const std::size_t last_visible = viewport.first_visible_line + visible_line_count - 1;
    if (cursor_line > last_visible) {
	viewport.first_visible_line = cursor_line - visible_line_count + 1;
    }
}
