/** Port of src/note_file_codec.cpp — sectioned notes/note_<id>.txt format. */

export interface ParsedNoteFile {
  title: string;
  id: string;
  createdLine: string;
  lastEditedLine: string;
  body: string;
}

export interface NoteJson {
  id: number;
  title: string;
  created: string;
  lastEdited: string;
  body: string;
  path?: string;
}

function stripTrailingCr(s: string): string {
  return s.endsWith("\r") ? s.slice(0, -1) : s;
}

/** Strip optional "Created: " / "Last Edited: " prefixes from value lines. */
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
 * Format like C++ get_created / get_last_edit(..., "date_time"):
 * "Created: April 21, 2026 at 10:39 PM"
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

export function parseNoteFile(text: string): ParsedNoteFile | null {
  const lines = text.split("\n").map(stripTrailingCr);
  // Drop trailing empty line from final newline so Body handling matches getline loop
  const out: ParsedNoteFile = {
    title: "",
    id: "",
    createdLine: "",
    lastEditedLine: "",
    body: "",
  };

  let i = 0;
  while (i < lines.length) {
    const part = lines[i];
    i += 1;
    if (part === "") {
      continue;
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
    } else if (part === "Body:") {
      const bodyLines = lines.slice(i);
      // C++ getline keeps empty trailing line content; join remaining lines with \n
      out.body = bodyLines.join("\n");
      return out.title !== "" && out.id !== "" ? out : null;
    } else {
      return null;
    }
  }

  return out.title !== "" && out.id !== "" && out.body !== "" ? out : null;
}

export function serializeNoteFile(fields: {
  title: string;
  id: number | string;
  createdLine: string;
  lastEditedLine: string;
  body: string;
}): string {
  const body = fields.body.endsWith("\n") || fields.body === ""
    ? fields.body
    : fields.body + "\n";
  return (
    `Title:\n${fields.title}\n` +
    `ID:\n${fields.id}\n` +
    `Created:\n${fields.createdLine}\n` +
    `Last Edited:\n${fields.lastEditedLine}\n` +
    `Body:\n${body}`
  );
}

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
  };
  if (path !== undefined) {
    note.path = path;
  }
  return note;
}
