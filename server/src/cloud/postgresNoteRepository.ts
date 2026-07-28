import type pg from "pg";

import { formatTs } from "./userStore.js";
import { ilikeContainsPattern } from "../noteSearch.js";
import {
  rankContextHits,
  sourceDomainFromUrl,
  type ContextNoteHit,
} from "../sourceUrl.js";

/** Cloud note JSON — UUID id (plan lock). */
export interface CloudNoteJson {
  id: string;
  title: string;
  created: string;
  lastEdited: string;
  body: string;
  sourceUrl: string;
}

export interface CloudNoteIndexEntry {
  id: string;
  title: string;
  sourceUrl: string;
}

function rowToNote(row: {
  id: string;
  title: string;
  body: string;
  source_url?: string;
  created_at: Date;
  updated_at: Date;
}): CloudNoteJson {
  return {
    id: row.id,
    title: row.title,
    body: row.body,
    sourceUrl: row.source_url ?? "",
    created: formatTs(new Date(row.created_at)),
    lastEdited: formatTs(new Date(row.updated_at)),
  };
}

export class PostgresNoteRepository {
  constructor(private readonly pool: pg.Pool) {}

  async list(userId: string, query?: string): Promise<CloudNoteIndexEntry[]> {
    const q = query?.trim() ?? "";
    if (q === "") {
      const r = await this.pool.query(
        `SELECT id, title, source_url FROM notes
         WHERE user_id = $1
         ORDER BY updated_at DESC`,
        [userId],
      );
      return r.rows.map((row) => ({
        id: row.id as string,
        title: row.title as string,
        sourceUrl: (row.source_url as string) ?? "",
      }));
    }

    const pattern = ilikeContainsPattern(q);
    const r = await this.pool.query(
      `SELECT id, title, source_url FROM notes
       WHERE user_id = $1
         AND (
           title ILIKE $2 ESCAPE '\\'
           OR body ILIKE $2 ESCAPE '\\'
           OR source_url ILIKE $2 ESCAPE '\\'
         )
       ORDER BY updated_at DESC`,
      [userId, pattern],
    );
    return r.rows.map((row) => ({
      id: row.id as string,
      title: row.title as string,
      sourceUrl: (row.source_url as string) ?? "",
    }));
  }

  async listByContext(userId: string, pageUrl: string): Promise<ContextNoteHit[]> {
    const domain = sourceDomainFromUrl(pageUrl);
    if (!domain) {
      return [];
    }
    const r = await this.pool.query(
      `SELECT id, title, source_url FROM notes
       WHERE user_id = $1
         AND regexp_replace(lower(source_domain), '^www\\.', '') = $2
       ORDER BY updated_at DESC`,
      [userId, domain],
    );
    const rows = r.rows.map((row) => ({
      id: row.id as string,
      title: row.title as string,
      sourceUrl: (row.source_url as string) ?? "",
    }));
    return rankContextHits(rows, pageUrl);
  }

  async getById(userId: string, id: string): Promise<CloudNoteJson | null> {
    const r = await this.pool.query(
      `SELECT id, title, body, source_url, created_at, updated_at
       FROM notes WHERE user_id = $1 AND id = $2`,
      [userId, id],
    );
    if (r.rowCount === 0) {
      return null;
    }
    return rowToNote(r.rows[0]);
  }

  async create(
    userId: string,
    title: string,
    body = "",
    sourceUrl = "",
  ): Promise<CloudNoteJson> {
    const normalizedTitle = title.trim() === "" ? "Untitled" : title;
    const domain = sourceDomainFromUrl(sourceUrl);
    const r = await this.pool.query(
      `INSERT INTO notes (user_id, title, body, source_url, source_domain)
       VALUES ($1, $2, $3, $4, $5)
       RETURNING id, title, body, source_url, created_at, updated_at`,
      [userId, normalizedTitle, body, sourceUrl, domain],
    );
    return rowToNote(r.rows[0]);
  }

  async update(
    userId: string,
    id: string,
    patch: { title?: string; body?: string; sourceUrl?: string },
  ): Promise<CloudNoteJson | null> {
    const existing = await this.getById(userId, id);
    if (!existing) {
      return null;
    }
    const title =
      patch.title !== undefined
        ? patch.title.trim() === ""
          ? "Untitled"
          : patch.title
        : existing.title;
    const body = patch.body !== undefined ? patch.body : existing.body;
    const sourceUrl =
      patch.sourceUrl !== undefined ? patch.sourceUrl : existing.sourceUrl;
    const domain = sourceDomainFromUrl(sourceUrl);
    const r = await this.pool.query(
      `UPDATE notes
       SET title = $3, body = $4, source_url = $5, source_domain = $6, updated_at = NOW()
       WHERE user_id = $1 AND id = $2
       RETURNING id, title, body, source_url, created_at, updated_at`,
      [userId, id, title, body, sourceUrl, domain],
    );
    if (r.rowCount === 0) {
      return null;
    }
    return rowToNote(r.rows[0]);
  }

  async delete(userId: string, id: string): Promise<boolean> {
    const r = await this.pool.query(
      `DELETE FROM notes WHERE user_id = $1 AND id = $2`,
      [userId, id],
    );
    return (r.rowCount ?? 0) > 0;
  }
}
