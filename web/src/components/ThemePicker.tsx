/**
 * Sidebar theme badge + upward dropdown — same three presets as the SDL GUI.
 */

import { useEffect, useRef } from "react";

import {
  THEME_IDS,
  THEME_LABELS,
  type ThemeId,
} from "../theme";

interface ThemePickerProps {
  theme: ThemeId;
  onChange: (id: ThemeId) => void;
  open: boolean;
  onOpenChange: (open: boolean) => void;
}

export function ThemePicker({
  theme,
  onChange,
  open,
  onOpenChange,
}: ThemePickerProps) {
  const rootRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!open) {
      return;
    }
    function onDocPointer(ev: MouseEvent) {
      if (rootRef.current && !rootRef.current.contains(ev.target as Node)) {
        onOpenChange(false);
      }
    }
    function onKey(ev: KeyboardEvent) {
      if (ev.key === "Escape") {
        onOpenChange(false);
      }
    }
    document.addEventListener("mousedown", onDocPointer);
    document.addEventListener("keydown", onKey);
    return () => {
      document.removeEventListener("mousedown", onDocPointer);
      document.removeEventListener("keydown", onKey);
    };
  }, [open, onOpenChange]);

  return (
    <div className="theme-picker" ref={rootRef}>
      {open ? (
        <ul className="theme-dropdown" role="listbox" aria-label="Theme">
          {THEME_IDS.map((id) => (
            <li key={id} role="option" aria-selected={id === theme}>
              <button
                type="button"
                className={
                  id === theme ? "theme-option active" : "theme-option"
                }
                onClick={() => {
                  onChange(id);
                  onOpenChange(false);
                }}
              >
                {THEME_LABELS[id]}
              </button>
            </li>
          ))}
        </ul>
      ) : null}
      <button
        type="button"
        className="theme-badge"
        aria-haspopup="listbox"
        aria-expanded={open}
        onClick={() => onOpenChange(!open)}
      >
        Theme · {THEME_LABELS[theme]}
      </button>
    </div>
  );
}
