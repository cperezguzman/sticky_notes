/**
 * Badge + tab listener — forward resurfacing for the active page URL.
 */

import { apiFetch, isHttpUrl } from "./shared.js";
import { ext } from "./browser.js";

async function setBadge(count, ok) {
  if (!ok) {
    await ext.action.setBadgeBackgroundColor({ color: "#8b3a2f" });
    await ext.action.setBadgeText({ text: "!" });
    return;
  }
  await ext.action.setBadgeBackgroundColor({ color: "#2f5d50" });
  await ext.action.setBadgeText({
    text: count > 0 ? String(Math.min(count, 99)) : "",
  });
}

async function refreshForTab(tabId, url) {
  if (!isHttpUrl(url)) {
    await ext.action.setBadgeText({ text: "" });
    return;
  }
  try {
    const res = await apiFetch(`/notes/context?url=${encodeURIComponent(url)}`);
    if (res.status === 401) {
      await setBadge(0, false);
      return;
    }
    if (!res.ok) {
      await setBadge(0, false);
      return;
    }
    const data = await res.json();
    const count = Array.isArray(data.notes) ? data.notes.length : 0;
    await setBadge(count, true);
  } catch {
    await setBadge(0, false);
  }
}

ext.tabs.onActivated.addListener(async ({ tabId }) => {
  try {
    const tab = await ext.tabs.get(tabId);
    await refreshForTab(tabId, tab.url || "");
  } catch {
    // ignore
  }
});

ext.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
  if (changeInfo.status === "complete" || changeInfo.url) {
    await refreshForTab(tabId, tab.url || changeInfo.url || "");
  }
});

ext.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (message?.type === "refresh-active") {
    ext.tabs.query({ active: true, currentWindow: true }).then(async (tabs) => {
      const tab = tabs[0];
      if (tab?.id != null) {
        await refreshForTab(tab.id, tab.url || "");
      }
      sendResponse({ ok: true });
    });
    return true;
  }
  return false;
});
