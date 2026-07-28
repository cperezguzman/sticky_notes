import { useEffect, useRef } from "react";

import type { NoteIndexEntry } from "../types";
import type { ThemeId } from "../theme";
import { ThemePicker } from "./ThemePicker";

interface NoteListProps {
  notes: NoteIndexEntry[];
  selectedId: string | number | null;
  onSelect: (id: string | number) => void;
  onCreate: () => void;
  onLogout?: () => void;
  user?: string | null;
  busy: boolean;
  theme: ThemeId;
  onThemeChange: (id: ThemeId) => void;
  themeOpen: boolean;
  onThemeOpenChange: (open: boolean) => void;
  searchQuery: string;
  onSearchChange: (value: string) => void;
  searchActive: boolean;
}

export function NoteList({
  notes,
  selectedId,
  onSelect,
  onCreate,
  onLogout,
  user,
  busy,
  theme,
  onThemeChange,
  themeOpen,
  onThemeOpenChange,
  searchQuery,
  onSearchChange,
  searchActive,
}: NoteListProps) {
  const searchRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    function onKey(ev: KeyboardEvent) {
      if (!(ev.ctrlKey || ev.metaKey) || ev.altKey) {
        return;
      }
      if (ev.key !== "k" && ev.key !== "K") {
        return;
      }
      ev.preventDefault();
      searchRef.current?.focus();
      searchRef.current?.select();
    }
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  let emptyHint: string | null = null;
  if (notes.length === 0) {
    emptyHint = searchActive
      ? "No notes match that search."
      : "No notes yet. Create one to begin.";
  }

  return (
    <aside className="sidebar">
      <div className="sidebar-header">
        <h1>Sticky Notes</h1>
        {user ? <p className="signed-in">Signed in as {user}</p> : null}
        <label className="search-field">
          <span className="visually-hidden">Search notes</span>
          <input
            ref={searchRef}
            type="search"
            className="search-input"
            placeholder="Search notes…"
            value={searchQuery}
            onChange={(e) => onSearchChange(e.target.value)}
            disabled={busy}
            spellCheck={false}
            autoComplete="off"
          />
        </label>
        <button
          type="button"
          className="btn primary"
          onClick={onCreate}
          disabled={busy}
        >
          New note
        </button>
        {onLogout ? (
          <button
            type="button"
            className="btn"
            onClick={onLogout}
            disabled={busy}
          >
            Sign out
          </button>
        ) : null}
      </div>
      {emptyHint ? (
        <p className="empty-hint">{emptyHint}</p>
      ) : (
        <ul className="note-list">
          {notes.map((n, index) => (
            <li key={String(n.id)}>
              <button
                type="button"
                className={
                  n.id === selectedId ? "note-item active" : "note-item"
                }
                onClick={() => onSelect(n.id)}
                disabled={busy}
              >
                <span className="note-id">{index + 1}</span>
                <span className="note-title">{n.title || "Untitled"}</span>
                {n.sourceUrl ? (
                  <span className="note-link-flag" title="Has source link">
                    ↗
                  </span>
                ) : null}
              </button>
            </li>
          ))}
        </ul>
      )}
      <ThemePicker
        theme={theme}
        onChange={onThemeChange}
        open={themeOpen}
        onOpenChange={onThemeOpenChange}
      />
    </aside>
  );
}
