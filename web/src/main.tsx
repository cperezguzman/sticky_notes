/**
 * React entry point — mounts `<App />` into `#root` from `index.html`.
 *
 * StrictMode double-invokes effects in development to surface unsafe side
 * effects; `App.tsx` uses a `cancelled` flag in async effects for that reason.
 */

import { StrictMode } from "react";
import { createRoot } from "react-dom/client";

import App from "./App";
import { applyThemeToDocument, loadTheme } from "./theme";
import "./styles.css";

applyThemeToDocument(loadTheme());

const root = document.getElementById("root");
if (!root) {
  throw new Error("Missing #root element");
}

createRoot(root).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
