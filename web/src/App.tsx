import { useCallback, useEffect, useState } from "react";

import {
  ApiError,
  checkHealth,
  createNote,
  deleteNote,
  getAuthMe,
  getNote,
  listNotes,
  login,
  logout,
  signup,
  resendVerification,
  updateNote,
  verifyEmail,
} from "./api";
import { AuthForms } from "./components/AuthForms";
import { NoteEditor } from "./components/NoteEditor";
import { NoteList } from "./components/NoteList";
import {
  applyThemeToDocument,
  cycleTheme,
  loadTheme,
  saveTheme,
  type ThemeId,
} from "./theme";
import type { Note, NoteIndexEntry } from "./types";

type AuthState =
  | { status: "loading" }
  | { status: "need_login" }
  | { status: "ready"; user: string | null; authRequired: boolean };

type FormMode = "login" | "signup" | "check_email";

function readNoteDeepLink(): string | number | null {
  const raw = new URLSearchParams(window.location.search).get("note");
  if (!raw) {
    return null;
  }
  if (/^\d+$/.test(raw)) {
    return Number.parseInt(raw, 10);
  }
  return raw;
}

export default function App() {
  const [auth, setAuth] = useState<AuthState>({ status: "loading" });
  const [formMode, setFormMode] = useState<FormMode>("login");
  const [storage, setStorage] = useState<"cloud" | "files" | null>(null);
  const [loginError, setLoginError] = useState<string | null>(null);
  const [info, setInfo] = useState<string | null>(null);
  const [checkEmailAddress, setCheckEmailAddress] = useState<string | null>(
    null,
  );
  const [notes, setNotes] = useState<NoteIndexEntry[]>([]);
  const [selectedId, setSelectedId] = useState<string | number | null>(null);
  const [note, setNote] = useState<Note | null>(null);
  const [title, setTitle] = useState("");
  const [body, setBody] = useState("");
  const [sourceUrl, setSourceUrl] = useState("");
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState("Loading…");
  const [apiOk, setApiOk] = useState<boolean | null>(null);
  const [theme, setTheme] = useState<ThemeId>(() => loadTheme());
  const [themeOpen, setThemeOpen] = useState(false);
  const [searchQuery, setSearchQuery] = useState("");
  const [debouncedSearch, setDebouncedSearch] = useState("");

  const dirty =
    note !== null &&
    (title !== note.title ||
      body !== note.body ||
      sourceUrl !== (note.sourceUrl ?? ""));

  useEffect(() => {
    applyThemeToDocument(theme);
    saveTheme(theme);
  }, [theme]);

  useEffect(() => {
    const handle = window.setTimeout(() => {
      setDebouncedSearch(searchQuery.trim());
    }, 200);
    return () => window.clearTimeout(handle);
  }, [searchQuery]);

  useEffect(() => {
    function onKey(ev: KeyboardEvent) {
      if (!(ev.ctrlKey || ev.metaKey) || ev.altKey) {
        return;
      }
      const tag = (ev.target as HTMLElement | null)?.tagName;
      if (tag === "INPUT" || tag === "TEXTAREA") {
        return;
      }
      if (ev.key === "t" || ev.key === "T") {
        ev.preventDefault();
        setTheme((prev) => cycleTheme(prev));
        setThemeOpen(false);
        return;
      }
      if (ev.key === "1") {
        ev.preventDefault();
        setTheme("minimal");
        setThemeOpen(false);
      } else if (ev.key === "2") {
        ev.preventDefault();
        setTheme("retro");
        setThemeOpen(false);
      } else if (ev.key === "3") {
        ev.preventDefault();
        setTheme("cyberpunk");
        setThemeOpen(false);
      }
    }
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  const selectedDisplayNumber =
    selectedId === null
      ? null
      : notes.findIndex((n) => n.id === selectedId) + 1 || null;

  const refreshList = useCallback(
    async (preferId?: string | number | null, query?: string) => {
      const q = query ?? debouncedSearch;
      const entries = await listNotes(q);
      setNotes(entries);
      if (preferId !== undefined && preferId !== null) {
        const stillThere = entries.some((e) => e.id === preferId);
        setSelectedId(stillThere ? preferId : entries[0]?.id ?? null);
        return;
      }
      setSelectedId((prev) => {
        if (prev !== null && entries.some((e) => e.id === prev)) {
          return prev;
        }
        return entries[0]?.id ?? null;
      });
    },
    [debouncedSearch],
  );

  useEffect(() => {
    if (auth.status !== "ready") {
      return;
    }
    let cancelled = false;
    (async () => {
      try {
        await refreshList(undefined, debouncedSearch);
      } catch (err) {
        if (!cancelled) {
          if (err instanceof ApiError && err.status === 401) {
            setAuth({ status: "need_login" });
            setStatus("Session expired — sign in again");
          } else {
            setStatus(
              err instanceof Error ? err.message : "Search failed",
            );
          }
        }
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [debouncedSearch, auth.status, refreshList]);

  const enterApp = useCallback(
    async (user: string | null, authRequired: boolean) => {
      setAuth({ status: "ready", user, authRequired });
      try {
        const prefer = readNoteDeepLink();
        await refreshList(prefer);
        if (prefer !== null) {
          window.history.replaceState({}, "", window.location.pathname);
        }
        setStatus(user ? `Signed in as ${user}` : "Ready (auth off)");
      } catch (err) {
        if (err instanceof ApiError && err.status === 401) {
          setAuth({ status: "need_login" });
          setStatus("Session expired — sign in again");
          return;
        }
        setStatus(err instanceof Error ? err.message : "Failed to load notes");
      }
    },
    [refreshList],
  );

  useEffect(() => {
    let cancelled = false;
    (async () => {
      const params = new URLSearchParams(window.location.search);
      const verifyToken = params.get("verify");
      if (verifyToken) {
        try {
          const result = await verifyEmail(verifyToken);
          if (!cancelled) {
            setInfo(result.message);
            setFormMode("login");
            window.history.replaceState({}, "", window.location.pathname);
          }
        } catch (err) {
          if (!cancelled) {
            setLoginError(
              err instanceof Error ? err.message : "Verification failed",
            );
            setFormMode("login");
            window.history.replaceState({}, "", window.location.pathname);
          }
        }
      }

      const health = await checkHealth();
      if (cancelled) {
        return;
      }
      setApiOk(health.ok);
      setStorage(health.storage ?? null);
      if (!health.ok) {
        setStatus("API unreachable — start the server: cd server && npm start");
        setAuth({ status: "need_login" });
        return;
      }
      try {
        const me = await getAuthMe();
        if (cancelled) {
          return;
        }
        if (me.storage) {
          setStorage(me.storage);
        }
        if (me.authRequired && !me.user) {
          setAuth({ status: "need_login" });
          setStatus("Sign in to continue");
          return;
        }
        await enterApp(me.user, me.authRequired);
      } catch (err) {
        if (!cancelled) {
          setAuth({ status: "need_login" });
          setStatus(err instanceof Error ? err.message : "Auth check failed");
        }
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [enterApp]);

  useEffect(() => {
    if (auth.status !== "ready" || selectedId === null) {
      if (selectedId === null) {
        setNote(null);
        setTitle("");
        setBody("");
        setSourceUrl("");
      }
      return;
    }
    let cancelled = false;
    (async () => {
      setBusy(true);
      try {
        const loaded = await getNote(selectedId);
        if (cancelled) {
          return;
        }
        setNote(loaded);
        setTitle(loaded.title);
        setBody(loaded.body);
        setSourceUrl(loaded.sourceUrl ?? "");
        setStatus(`Opened note`);
      } catch (err) {
        if (!cancelled) {
          if (err instanceof ApiError && err.status === 401) {
            setAuth({ status: "need_login" });
            setStatus("Session expired — sign in again");
          } else {
            setStatus(err instanceof Error ? err.message : "Failed to open note");
            setNote(null);
          }
        }
      } finally {
        if (!cancelled) {
          setBusy(false);
        }
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [selectedId, auth.status]);

  async function handleLogin(email: string, password: string) {
    setBusy(true);
    setLoginError(null);
    try {
      const me = await login(email, password);
      if (me.storage) {
        setStorage(me.storage);
      }
      await enterApp(me.user, me.authRequired);
    } catch (err) {
      setLoginError(err instanceof Error ? err.message : "Login failed");
    } finally {
      setBusy(false);
    }
  }

  async function handleSignup(email: string, password: string) {
    setBusy(true);
    setLoginError(null);
    setInfo(null);
    try {
      const result = await signup(email, password);
      setCheckEmailAddress(result.email);
      setFormMode("check_email");
      setInfo(result.message);
    } catch (err) {
      setLoginError(err instanceof Error ? err.message : "Signup failed");
    } finally {
      setBusy(false);
    }
  }

  async function handleResend(email: string) {
    const trimmed = email.trim();
    if (!trimmed) {
      setLoginError("enter your email first");
      return;
    }
    setBusy(true);
    setLoginError(null);
    setInfo(null);
    try {
      const result = await resendVerification(trimmed);
      setCheckEmailAddress(result.email);
      setFormMode("check_email");
      setInfo(result.message);
    } catch (err) {
      setLoginError(err instanceof Error ? err.message : "Resend failed");
    } finally {
      setBusy(false);
    }
  }

  async function handleLogout() {
    setBusy(true);
    try {
      await logout();
      setNotes([]);
      setSelectedId(null);
      setNote(null);
      setTitle("");
      setBody("");
      setSourceUrl("");
      setSearchQuery("");
      setDebouncedSearch("");
      setAuth({ status: "need_login" });
      setFormMode("login");
      setStatus("Signed out");
    } catch (err) {
      setStatus(err instanceof Error ? err.message : "Logout failed");
    } finally {
      setBusy(false);
    }
  }

  async function handleCreate() {
    setBusy(true);
    try {
      const created = await createNote("Untitled", "");
      setSearchQuery("");
      setDebouncedSearch("");
      await refreshList(created.id, "");
      setStatus(`Created note`);
    } catch (err) {
      if (err instanceof ApiError && err.status === 401) {
        setAuth({ status: "need_login" });
      }
      setStatus(err instanceof Error ? err.message : "Create failed");
    } finally {
      setBusy(false);
    }
  }

  async function handleSave() {
    if (note === null || !dirty) {
      return;
    }
    setBusy(true);
    try {
      const saved = await updateNote(note.id, { title, body, sourceUrl });
      setNote(saved);
      setTitle(saved.title);
      setBody(saved.body);
      setSourceUrl(saved.sourceUrl ?? "");
      await refreshList(saved.id);
      setStatus(`Saved`);
    } catch (err) {
      if (err instanceof ApiError && err.status === 401) {
        setAuth({ status: "need_login" });
      }
      setStatus(err instanceof Error ? err.message : "Save failed");
    } finally {
      setBusy(false);
    }
  }

  async function handleDelete() {
    if (note === null) {
      return;
    }
    const ok = window.confirm(
      `Delete note "${note.title}" from your account?`,
    );
    if (!ok) {
      return;
    }
    setBusy(true);
    try {
      const id = note.id;
      await deleteNote(id);
      setNote(null);
      setTitle("");
      setBody("");
      setSourceUrl("");
      await refreshList(null);
      setStatus(`Deleted`);
    } catch (err) {
      if (err instanceof ApiError && err.status === 401) {
        setAuth({ status: "need_login" });
      }
      setStatus(err instanceof Error ? err.message : "Delete failed");
    } finally {
      setBusy(false);
    }
  }

  if (auth.status === "loading") {
    return (
      <div className="login-shell">
        <p className="login-lead">Loading…</p>
      </div>
    );
  }

  if (auth.status === "need_login") {
    return (
      <AuthForms
        mode={formMode}
        storage={storage}
        busy={busy}
        error={loginError}
        info={info}
        checkEmailAddress={checkEmailAddress}
        onLogin={handleLogin}
        onSignup={handleSignup}
        onResend={handleResend}
        onShowLogin={() => {
          setFormMode("login");
          setLoginError(null);
        }}
        onShowSignup={() => {
          setFormMode("signup");
          setLoginError(null);
        }}
      />
    );
  }

  return (
    <div className="app-shell">
      <NoteList
        notes={notes}
        selectedId={selectedId}
        onSelect={setSelectedId}
        onCreate={handleCreate}
        onLogout={auth.authRequired ? handleLogout : undefined}
        user={auth.user}
        busy={busy}
        theme={theme}
        onThemeChange={setTheme}
        themeOpen={themeOpen}
        onThemeOpenChange={setThemeOpen}
        searchQuery={searchQuery}
        onSearchChange={setSearchQuery}
        searchActive={debouncedSearch.length > 0}
      />
      <main className="main-pane">
        <NoteEditor
          note={note}
          displayNumber={selectedDisplayNumber}
          title={title}
          body={body}
          sourceUrl={sourceUrl}
          dirty={dirty}
          busy={busy}
          onTitleChange={setTitle}
          onBodyChange={setBody}
          onSourceUrlChange={setSourceUrl}
          onSave={handleSave}
          onDelete={handleDelete}
        />
        <footer className="status-bar">
          <span
            className={
              apiOk === false ? "dot bad" : apiOk === true ? "dot ok" : "dot"
            }
            aria-hidden
          />
          <span>{status}</span>
          {storage ? (
            <span className="storage-flag">{storage}</span>
          ) : null}
          {dirty ? <span className="dirty-flag">unsaved</span> : null}
        </footer>
      </main>
    </div>
  );
}
