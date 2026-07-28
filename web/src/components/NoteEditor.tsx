/**
 * Main editor pane: title + source URL + body + Save / Delete / Open source.
 *
 * Controlled inputs — drafts live in `App.tsx` so dirty-checking can compare
 * against the last saved `note` object.
 */

import type { Note } from "../types";

interface NoteEditorProps {
  /** Last saved note from the API, or null if none open. */
  note: Note | null;
  /** 1-based position in the sidebar list (display only). */
  displayNumber: number | null;
  /** Draft title (may differ from `note.title` when dirty). */
  title: string;
  /** Draft body (may differ from `note.body` when dirty). */
  body: string;
  /** Draft external context URL. */
  sourceUrl: string;
  /** True when draft ≠ last saved note — enables Save. */
  dirty: boolean;
  busy: boolean;
  onTitleChange: (value: string) => void;
  onBodyChange: (value: string) => void;
  onSourceUrlChange: (value: string) => void;
  onSave: () => void;
  onDelete: () => void;
}

export function NoteEditor({
  note,
  displayNumber,
  title,
  body,
  sourceUrl,
  dirty,
  busy,
  onTitleChange,
  onBodyChange,
  onSourceUrlChange,
  onSave,
  onDelete,
}: NoteEditorProps) {
  if (!note) {
    return (
      <section className="editor empty">
        <p>Select a note from the list, or create a new one.</p>
      </section>
    );
  }

  const openable = sourceUrl.trim().startsWith("http");

  return (
    <section className="editor">
      <div className="editor-toolbar">
        <div className="meta">
          <span>note {displayNumber ?? "—"}</span>
          <span>created {note.created || "—"}</span>
          <span>edited {note.lastEdited || "—"}</span>
        </div>
        <div className="actions">
          {openable ? (
            <a
              className="btn"
              href={sourceUrl.trim()}
              target="_blank"
              rel="noopener noreferrer"
            >
              Open source
            </a>
          ) : null}
          <button
            type="button"
            className="btn primary"
            onClick={onSave}
            disabled={busy || !dirty}
          >
            Save
          </button>
          <button
            type="button"
            className="btn danger"
            onClick={onDelete}
            disabled={busy}
          >
            Delete
          </button>
        </div>
      </div>
      <label className="field">
        <span>Title</span>
        <input
          type="text"
          value={title}
          onChange={(e) => onTitleChange(e.target.value)}
          disabled={busy}
          spellCheck={false}
        />
      </label>
      <label className="field">
        <span>Source URL</span>
        <input
          type="url"
          value={sourceUrl}
          onChange={(e) => onSourceUrlChange(e.target.value)}
          disabled={busy}
          spellCheck={false}
          placeholder="https://… (optional — page that prompted this note)"
        />
      </label>
      <label className="field body-field">
        <span>Body</span>
        <textarea
          value={body}
          onChange={(e) => onBodyChange(e.target.value)}
          disabled={busy}
          spellCheck={false}
        />
      </label>
    </section>
  );
}
