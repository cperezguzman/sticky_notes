/**
 * Source URL helpers for context-linked notes (reverse link: note → page).
 * Only http(s) URLs are accepted so “Open source” is safe to hand to a browser.
 */

export function normalizeSourceUrl(
  raw: unknown,
): { ok: true; url: string } | { ok: false; error: string } {
  if (raw === undefined || raw === null) {
    return { ok: true, url: "" };
  }
  if (typeof raw !== "string") {
    return { ok: false, error: "sourceUrl must be a string" };
  }
  const trimmed = raw.trim();
  if (trimmed === "") {
    return { ok: true, url: "" };
  }
  let parsed: URL;
  try {
    parsed = new URL(trimmed);
  } catch {
    return { ok: false, error: "sourceUrl must be a valid URL" };
  }
  if (parsed.protocol !== "http:" && parsed.protocol !== "https:") {
    return { ok: false, error: "sourceUrl must be http or https" };
  }
  return { ok: true, url: parsed.href };
}

export function sourceDomainFromUrl(url: string): string {
  if (!url) {
    return "";
  }
  try {
    return new URL(url).hostname.toLowerCase();
  } catch {
    return "";
  }
}
