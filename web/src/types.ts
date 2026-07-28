export interface NoteIndexEntry {
  id: string | number;
  title: string;
  path?: string;
  sourceUrl?: string;
}

export interface Note {
  id: string | number;
  title: string;
  created: string;
  lastEdited: string;
  body: string;
  sourceUrl?: string;
  path?: string;
}
