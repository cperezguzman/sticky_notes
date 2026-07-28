/**
 * Note file codec — TypeScript port of `src/note_file_codec.cpp`.
 *
 * ## On-disk format (sectioned plain text)
 *
 * ```
 * Title:
 * My note
 * ID:
 * 3
 * Created:
 * Created: April 21, 2026 at 10:39 PM
 * Last Edited:
 * Last Edited: April 21, 2026 at 11:02 PM
 * Source:          (optional — external context URL)
 * https://example.com/page
 * Body:
 * line one
 * line two
 * ```
 *
 * Each section label sits on its own line; the value follows on the next line
 * (Body consumes everything until EOF). This must stay compatible with the C++
 * CLI/GUI so all three frontends share the same files.
 *
 * ## Two shapes
 *
 * - `ParsedNoteFile` — raw fields as stored (Created/Last Edited still have prefixes)
 * - `NoteJson` — API / UI shape (display timestamps without the prefixes)
 */

/** Raw fields after parsing a `note_<id>.txt` file. */
export interface ParsedNoteFile {
  title: string;
  /** String form of the numeric id as written on disk. */
  id: string;
  /** Full value line, e.g. `"Created: April 21, 2026 at 10:39 PM"`. */
  createdLine: string;
  /** Full value line, e.g. `"Last Edited: …"`. */
  lastEditedLine: string;
  /** Optional external context URL (reverse link). Empty if unset. */
  sourceUrl: string;
  /** Body text; may include trailing newline. */
  body: string;
}

/** JSON shape returned by the HTTP API and consumed by the React UI. */
export interface NoteJson {
  id: number;
  title: string;
  /** Human-readable created time (prefix stripped). */
  created: string;
  /** Human-readable last-edited time (prefix stripped). */
  lastEdited: string;
  body: string;
  /** External context URL for “Open source”; empty if unset. */
  sourceUrl: string;
  path?: string;
}

/** Remove a trailing CR so Windows-saved files still parse on Linux. */
function stripTrailingCr(s: string): string {
  return s.endsWith("\r") ? s.slice(0, -1) : s;
}

/**
 * Strip optional `"Created: "` / `"Last Edited: "` prefixes from value lines
 * so the UI can show a clean timestamp string.
 */
export function displayTimestamp(raw: string): string {
  const created = "Created: ";
  const edited = "Last Edited: ";
  if (raw.startsWith(created)) {
    return raw.slice(created.length);
  }
  if (raw.startsWith(edited)) {
    return raw.slice(edited.length);
  }
  return raw;
}

/**
 * Format like C++ `get_created` / `get_last_edit(..., "date_time")`:
 * `"Created: April 21, 2026 at 10:39 PM"`
 *
 * Uses `en-US` locale so month names match what the C++ parser expects when
 * notes are round-tripped between TS and C++.
 */
export function formatTimestampLine(
  kind: "created" | "lastEdited",
  date: Date = new Date(),
): string {
  const prefix = kind === "created" ? "Created: " : "Last Edited: ";
  const month = date.toLocaleString("en-US", { month: "long" });
  const day = date.getDate();
  const year = date.getFullYear();
  const time = date.toLocaleString("en-US", {
    hour: "numeric",
    minute: "2-digit",
    hour12: true,
  });
  return `${prefix}${month} ${day}, ${year} at ${time}`;
}

/**
 * Parse a full note file into fields. Returns `null` if the layout is invalid
 * (unknown section label, missing value line, or missing title/id).
 */
export function parseNoteFile(text: string): ParsedNoteFile | null {
  const lines = text.split("\n").map(stripTrailingCr);
  const out: ParsedNoteFile = {
    title: "",
    id: "",
    createdLine: "",
    lastEditedLine: "",
    sourceUrl: "",
    body: "",
  };

  let i = 0;
  while (i < lines.length) {
    const part = lines[i];
    i += 1;
    if (part === "") {
      continue; // blank lines before a label are ignored
    }

    if (part === "Title:") {
      if (i >= lines.length) {
        return null;
      }
      out.title = lines[i];
      i += 1;
    } else if (part === "ID:") {
      if (i >= lines.length) {
        return null;
      }
      out.id = lines[i];
      i += 1;
    } else if (part === "Created:") {
      if (i >= lines.length) {
        return null;
      }
      out.createdLine = lines[i];
      i += 1;
    } else if (part === "Last Edited:") {
      if (i >= lines.length) {
        return null;
      }
      out.lastEditedLine = lines[i];
      i += 1;
    } else if (part === "Source:") {
      if (i >= lines.length) {
        return null;
      }
      out.sourceUrl = lines[i].trim();
      i += 1;
    } else if (part === "Body:") {
      // Everything from here to EOF is the body (may be multi-line)
      const bodyLines = lines.slice(i);
      out.body = bodyLines.join("\n");
      return out.title !== "" && out.id !== "" ? out : null;
    } else {
      // Unknown label → not our format
      return null;
    }
  }

  // Reached EOF without Body: — only accept if title/id/body somehow filled
  return out.title !== "" && out.id !== "" && out.body !== "" ? out : null;
}

/**
 * Serialize fields back to the sectioned on-disk format.
 * Ensures the body ends with a trailing newline (C++ save behaviour).
 */
export function serializeNoteFile(fields: {
  title: string;
  id: number | string;
  createdLine: string;
  lastEditedLine: string;
  body: string;
  sourceUrl?: string;
}): string {
  const body = fields.body.endsWith("\n") || fields.body === ""
    ? fields.body
    : fields.body + "\n";
  const source = (fields.sourceUrl ?? "").trim();
  const sourceBlock = source === "" ? "" : `Source:\n${source}\n`;
  return (
    `Title:\n${fields.title}\n` +
    `ID:\n${fields.id}\n` +
    `Created:\n${fields.createdLine}\n` +
    `Last Edited:\n${fields.lastEditedLine}\n` +
    sourceBlock +
    `Body:\n${body}`
  );
}

/**
 * Convert parsed disk fields into the API/UI JSON shape.
 * Returns null if the id string is not a valid integer.
 */
export function parsedToNoteJson(
  parsed: ParsedNoteFile,
  path?: string,
): NoteJson | null {
  const id = Number.parseInt(parsed.id, 10);
  if (Number.isNaN(id)) {
    return null;
  }
  const note: NoteJson = {
    id,
    title: parsed.title,
    created: displayTimestamp(parsed.createdLine),
    lastEdited: displayTimestamp(parsed.lastEditedLine),
    body: parsed.body,
    sourceUrl: parsed.sourceUrl,
  };
  if (path !== undefined) {
    note.path = path;
  }
  return note;
}
