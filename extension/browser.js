/**
 * Firefox uses `browser.*`; Chromium uses `chrome.*` (MV3).
 */
export const ext = globalThis.browser ?? globalThis.chrome;
