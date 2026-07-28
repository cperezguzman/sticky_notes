import { DEFAULT_API, DEFAULT_WEB, getSettings } from "./shared.js";
import { ext } from "./browser.js";

const apiInput = document.getElementById("apiBase");
const webInput = document.getElementById("webBase");
const saved = document.getElementById("saved");

getSettings().then((s) => {
  apiInput.value = s.apiBase || DEFAULT_API;
  webInput.value = s.webBase || DEFAULT_WEB;
});

document.getElementById("save").addEventListener("click", async () => {
  const apiBase = apiInput.value.trim().replace(/\/$/, "") || DEFAULT_API;
  const webBase = webInput.value.trim().replace(/\/$/, "") || DEFAULT_WEB;
  await ext.storage.sync.set({ apiBase, webBase });
  try {
    const origin = new URL(apiBase).origin + "/*";
    await ext.permissions.request({ origins: [origin] });
  } catch {
    // optional
  }
  saved.textContent = "Saved.";
});
