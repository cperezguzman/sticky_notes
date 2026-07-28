/**
 * Source URL helpers for context-linked notes.
 * Reverse: note → page (“Open source”).
 * Forward: page → notes (extension resurfacing by exact URL / domain).
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
    return normalizeDomain(new URL(url).hostname);
  } catch {
    return "";
  }
}

/** Lowercase host; strip a leading `www.` so domain resurfacing is forgiving. */
export function normalizeDomain(host: string): string {
  const h = host.trim().toLowerCase();
  return h.startsWith("www.") ? h.slice(4) : h;
}

/** Exact page match ignoring URL hash. */
export function urlsMatchExact(a: string, b: string): boolean {
  try {
    const ua = new URL(a);
    const ub = new URL(b);
    ua.hash = "";
    ub.hash = "";
    return ua.href === ub.href;
  } catch {
    return false;
  }
}

export type ContextMatchKind = "exact" | "domain";

export interface ContextNoteHit {
  id: string | number;
  title: string;
  sourceUrl: string;
  match: ContextMatchKind;
}

export function rankContextHits(
  notes: { id: string | number; title: string; sourceUrl: string }[],
  pageUrl: string,
): ContextNoteHit[] {
  const domain = sourceDomainFromUrl(pageUrl);
  if (!domain) {
    return [];
  }
  const exact: ContextNoteHit[] = [];
  const byDomain: ContextNoteHit[] = [];
  for (const note of notes) {
    if (!note.sourceUrl) {
      continue;
    }
    if (urlsMatchExact(note.sourceUrl, pageUrl)) {
      exact.push({ ...note, match: "exact" });
    } else if (sourceDomainFromUrl(note.sourceUrl) === domain) {
      byDomain.push({ ...note, match: "domain" });
    }
  }
  return [...exact, ...byDomain];
}
