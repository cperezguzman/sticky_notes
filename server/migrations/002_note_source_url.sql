-- 002_note_source_url.sql — reverse context link (note → external URL)
ALTER TABLE notes
  ADD COLUMN IF NOT EXISTS source_url TEXT NOT NULL DEFAULT '',
  ADD COLUMN IF NOT EXISTS source_domain TEXT NOT NULL DEFAULT '';

CREATE INDEX IF NOT EXISTS notes_user_source_domain_idx
  ON notes (user_id, source_domain)
  WHERE source_domain <> '';
