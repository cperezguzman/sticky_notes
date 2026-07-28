import {
  apiFetch,
  getSettings,
  isHttpUrl,
  noteOpenUrl,
  readError,
  saveSessionToken,
} from "./shared.js";
import { ext } from "./browser.js";

const pageUrlEl = document.getElementById("page-url");
const authPanel = document.getElementById("auth-panel");
const mainPanel = document.getElementById("main-panel");
const noteList = document.getElementById("note-list");
const statusEl = document.getElementById("status");
const authError = document.getElementById("auth-error");
const actionError = document.getElementById("action-error");

let activeTabUrl = "";
let webBase = "http://127.0.0.1:5173";
let loginAttempted = false;

function show(el, visible) {
  el.classList.toggle("hidden", !visible);
}

async function currentTab() {
  const tabs = await ext.tabs.query({ active: true, currentWindow: true });
  return tabs[0] || null;
}

async function loadContext() {
  actionError.textContent = "";
  authError.textContent = "";
  const settings = await getSettings();
  webBase = settings.webBase;
  const tab = await currentTab();
  activeTabUrl = tab?.url || "";
  pageUrlEl.textContent = isHttpUrl(activeTabUrl)
    ? activeTabUrl
    : "Open an http(s) page to resurface notes.";

  const meRes = await apiFetch("/auth/me");
  if (!meRes.ok && meRes.status !== 401) {
    show(authPanel, true);
    show(mainPanel, false);
    authError.textContent = await readError(meRes);
    return;
  }
  const meData =
    meRes.status === 401
      ? { authRequired: true, user: null }
      : await meRes.json();

  if (meData.authRequired && !meData.user) {
    if (loginAttempted) {
      // Most common case: login request succeeded, but Firefox didn't keep/send the session cookie.
      // We surface a hint instead of leaving the user on a blank sign-in screen.
      authError.textContent = `Could not establish a session: ${await readError(meRes)}.`;
    }
    loginAttempted = false;
    show(authPanel, true);
    show(mainPanel, false);
    return;
  }

  show(authPanel, false);
  show(mainPanel, true);
  loginAttempted = false;

  if (!isHttpUrl(activeTabUrl)) {
    statusEl.textContent = "No page context on this tab.";
    noteList.innerHTML = "";
    return;
  }

  const res = await apiFetch(
    `/notes/context?url=${encodeURIComponent(activeTabUrl)}`,
  );
  if (res.status === 401) {
    show(authPanel, true);
    show(mainPanel, false);
    return;
  }
  if (!res.ok) {
    statusEl.textContent = await readError(res);
    noteList.innerHTML = "";
    return;
  }

  const data = await res.json();
  const notes = data.notes || [];
  if (notes.length === 0) {
    statusEl.textContent = `No notes linked to ${data.domain || "this page"}.`;
    noteList.innerHTML = "";
  } else {
    statusEl.textContent = `${notes.length} note${notes.length === 1 ? "" : "s"} for ${data.domain}`;
    noteList.innerHTML = "";
    for (const note of notes) {
      const li = document.createElement("li");
      const a = document.createElement("a");
      a.href = noteOpenUrl(webBase, note.id);
      a.target = "_blank";
      a.rel = "noopener noreferrer";
      const title = document.createElement("div");
      title.className = "title";
      title.textContent = note.title || "Untitled";
      const meta = document.createElement("div");
      meta.className = "meta";
      if (note.match === "exact") {
        const exact = document.createElement("span");
        exact.className = "exact";
        exact.textContent = "exact page";
        meta.appendChild(exact);
      } else {
        meta.textContent = `same site · ${note.sourceUrl || ""}`;
      }
      a.appendChild(title);
      a.appendChild(meta);
      li.appendChild(a);
      noteList.appendChild(li);
    }
  }

  ext.runtime.sendMessage({ type: "refresh-active" });
}

document.getElementById("login-btn").addEventListener("click", async () => {
  authError.textContent = "";
  try {
    loginAttempted = true;
    const email = document.getElementById("email").value.trim();
    const password = document.getElementById("password").value;
    const res = await apiFetch("/auth/login", {
      method: "POST",
      body: JSON.stringify({ email, password }),
    });
    if (!res.ok) {
      authError.textContent = await readError(res);
      return;
    }
    const data = await res.json();
    if (!data.sessionToken) {
      authError.textContent =
        "Login succeeded but no extension session token was returned. Restart the API server.";
      return;
    }
    await saveSessionToken(data.sessionToken);
    await loadContext();
  } catch (err) {
    authError.textContent = err instanceof Error ? err.message : String(err);
  }
});

document.getElementById("logout-btn").addEventListener("click", async () => {
  await apiFetch("/auth/logout", { method: "POST", body: "{}" });
  await saveSessionToken("");
  await loadContext();
});

document.getElementById("refresh-btn").addEventListener("click", () => {
  loadContext();
});

document.getElementById("options-btn").addEventListener("click", () => {
  ext.runtime.openOptionsPage();
});

document.getElementById("save-page-btn").addEventListener("click", async () => {
  actionError.textContent = "";
  if (!isHttpUrl(activeTabUrl)) {
    actionError.textContent = "Current tab is not an http(s) page.";
    return;
  }
  let title = "Untitled";
  try {
    title = new URL(activeTabUrl).hostname.replace(/^www\./, "");
  } catch {
    // keep Untitled
  }
  const res = await apiFetch("/notes", {
    method: "POST",
    body: JSON.stringify({
      title,
      body: "",
      sourceUrl: activeTabUrl,
    }),
  });
  if (!res.ok) {
    actionError.textContent = await readError(res);
    return;
  }
  const created = await res.json();
  await loadContext();
  ext.tabs.create({ url: noteOpenUrl(webBase, created.id) });
});

loadContext().catch((err) => {
  show(authPanel, true);
  show(mainPanel, false);
  authError.textContent = err instanceof Error ? err.message : String(err);
});
