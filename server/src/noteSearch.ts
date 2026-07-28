/**
 * Case-insensitive substring match over title + body (shared by file + cloud search).
 */

export function noteMatchesQuery(
  title: string,
  body: string,
  query: string,
  sourceUrl = "",
): boolean {
  const q = query.trim().toLowerCase();
  if (q === "") {
    return true;
  }
  return (
    title.toLowerCase().includes(q) ||
    body.toLowerCase().includes(q) ||
    sourceUrl.toLowerCase().includes(q)
  );
}

/** Escape `%`, `_`, and `\` for Postgres ILIKE … ESCAPE '\\'. */
export function ilikeContainsPattern(query: string): string {
  const escaped = query
    .trim()
    .replace(/\\/g, "\\\\")
    .replace(/%/g, "\\%")
    .replace(/_/g, "\\_");
  return `%${escaped}%`;
}
