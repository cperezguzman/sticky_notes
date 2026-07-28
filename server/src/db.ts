import { readFile, readdir } from "node:fs/promises";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import pg from "pg";

const { Pool } = pg;

let pool: pg.Pool | null = null;

export function getPool(): pg.Pool {
  if (!pool) {
    const url = process.env.DATABASE_URL;
    if (!url) {
      throw new Error("DATABASE_URL is required for cloud storage");
    }
    pool = new Pool({ connectionString: url });
  }
  return pool;
}

export async function closePool(): Promise<void> {
  if (pool) {
    await pool.end();
    pool = null;
  }
}

export async function runMigrations(client?: pg.Pool | pg.PoolClient): Promise<void> {
  const db = client ?? getPool();
  await db.query(`
    CREATE TABLE IF NOT EXISTS schema_migrations (
      id TEXT PRIMARY KEY,
      applied_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
    );
  `);

  const migrationsDir = join(dirname(fileURLToPath(import.meta.url)), "..", "migrations");
  const files = (await readdir(migrationsDir))
    .filter((f) => f.endsWith(".sql"))
    .sort();

  for (const file of files) {
    const id = file;
    const existing = await db.query("SELECT 1 FROM schema_migrations WHERE id = $1", [id]);
    if (existing.rowCount && existing.rowCount > 0) {
      continue;
    }
    const sql = await readFile(join(migrationsDir, file), "utf8");
    const clientConn = "connect" in db ? await (db as pg.Pool).connect() : null;
    const runner = clientConn ?? (db as pg.PoolClient);
    try {
      await runner.query("BEGIN");
      await runner.query(sql);
      await runner.query("INSERT INTO schema_migrations (id) VALUES ($1)", [id]);
      await runner.query("COMMIT");
      console.log(`[migrate] applied ${id}`);
    } catch (err) {
      await runner.query("ROLLBACK");
      throw err;
    } finally {
      clientConn?.release();
    }
  }
}

export function resolveStorageMode(): "cloud" | "files" {
  const explicit = (process.env.STICKY_STORAGE ?? "").toLowerCase();
  if (explicit === "files") {
    return "files";
  }
  if (explicit === "cloud") {
    return "cloud";
  }
  return process.env.DATABASE_URL ? "cloud" : "files";
}
