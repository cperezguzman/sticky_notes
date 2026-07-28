/**
 * Vite config for the Sticky Notes React UI.
 *
 * Dev server: http://127.0.0.1:5173
 *
 * The `proxy` block forwards API paths to the Node server on port 8787 so the
 * browser can use relative URLs (`/auth/login`, `/notes`, …) and keep the
 * session cookie same-origin. Start the API first (`cd server && npm start`).
 */

import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],
  server: {
    host: "127.0.0.1",
    port: 5173,
    proxy: {
      "/notes": "http://127.0.0.1:8787",
      "/health": "http://127.0.0.1:8787",
      "/auth": "http://127.0.0.1:8787",
    },
  },
});
