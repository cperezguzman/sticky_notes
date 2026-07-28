/**
 * Shared helpers for the Sticky Notes browser extension.
 */

import { ext } from "./browser.js";

export const DEFAULT_API = "http://127.0.0.1:8787";
export const DEFAULT_WEB = "http://127.0.0.1:5173";
const SESSION_KEY = "sessionToken";

export async function getSettings() {
  const stored = await ext.storage.sync.get({
    apiBase: DEFAULT_API,
    webBase: DEFAULT_WEB,
  });
  return {
    apiBase: String(stored.apiBase || DEFAULT_API).replace(/\/$/, ""),
    webBase: String(stored.webBase || DEFAULT_WEB).replace(/\/$/, ""),
  };
}

export async function getSessionToken() {
  const stored = await ext.storage.local.get({ [SESSION_KEY]: "" });
  return String(stored[SESSION_KEY] || "");
}

export async function saveSessionToken(token) {
  if (token) {
    await ext.storage.local.set({ [SESSION_KEY]: token });
  } else {
    await ext.storage.local.remove(SESSION_KEY);
  }
}

export async function apiFetch(path, init = {}) {
  const { apiBase } = await getSettings();
  const sessionToken = await getSessionToken();
  const headers = {
    "Content-Type": "application/json",
    "X-Sticky-Client": "extension",
    ...(init.headers || {}),
  };
  if (sessionToken) {
    headers.Authorization = `Bearer ${sessionToken}`;
  }
  const res = await fetch(`${apiBase}${path}`, {
    ...init,
    credentials: "include",
    headers,
  });
  return res;
}

export async function readError(res) {
  try {
    const data = await res.json();
    if (data && data.error) {
      return String(data.error);
    }
  } catch {
    // ignore
  }
  return `HTTP ${res.status}`;
}

export function noteOpenUrl(webBase, noteId) {
  return `${webBase}/?note=${encodeURIComponent(String(noteId))}`;
}

export function isHttpUrl(url) {
  return typeof url === "string" && /^https?:\/\//i.test(url);
}
