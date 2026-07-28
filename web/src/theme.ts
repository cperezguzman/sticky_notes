/**
 * Web theme ids mirror the SDL GUI (`gui_theme.h`): Minimal, Retro, Cyberpunk.
 * Choice persists in localStorage (browser-local; not shared with notes/gui_theme.txt).
 */

export type ThemeId = "minimal" | "retro" | "cyberpunk";

export const THEME_IDS: ThemeId[] = ["minimal", "retro", "cyberpunk"];

export const THEME_LABELS: Record<ThemeId, string> = {
  minimal: "Minimal",
  retro: "Retro",
  cyberpunk: "Cyberpunk",
};

const STORAGE_KEY = "sticky.web.theme";

export function isThemeId(value: string): value is ThemeId {
  return THEME_IDS.includes(value as ThemeId);
}

export function loadTheme(): ThemeId {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw && isThemeId(raw)) {
      return raw;
    }
  } catch {
    // private mode / blocked storage
  }
  return "minimal";
}

export function saveTheme(id: ThemeId): void {
  try {
    localStorage.setItem(STORAGE_KEY, id);
  } catch {
    // ignore
  }
}

export function cycleTheme(current: ThemeId): ThemeId {
  const i = THEME_IDS.indexOf(current);
  return THEME_IDS[(i + 1) % THEME_IDS.length]!;
}

export function applyThemeToDocument(id: ThemeId): void {
  document.documentElement.dataset.theme = id;
}
