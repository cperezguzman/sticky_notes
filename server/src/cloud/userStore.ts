import { randomBytes } from "node:crypto";

import bcrypt from "bcryptjs";
import type pg from "pg";

export interface CloudUser {
  id: string;
  email: string;
  passwordHash: string;
  emailVerifiedAt: Date | null;
}

function formatTs(d: Date): string {
  return d.toLocaleString("en-US", {
    month: "long",
    day: "numeric",
    year: "numeric",
    hour: "numeric",
    minute: "2-digit",
    hour12: true,
  });
}

export class UserStore {
  constructor(private readonly pool: pg.Pool) {}

  async findByEmail(email: string): Promise<CloudUser | null> {
    const r = await this.pool.query(
      `SELECT id, email, password_hash, email_verified_at
       FROM users WHERE email = $1`,
      [email.toLowerCase()],
    );
    if (r.rowCount === 0) {
      return null;
    }
    const row = r.rows[0];
    return {
      id: row.id,
      email: row.email,
      passwordHash: row.password_hash,
      emailVerifiedAt: row.email_verified_at,
    };
  }

  async findById(id: string): Promise<CloudUser | null> {
    const r = await this.pool.query(
      `SELECT id, email, password_hash, email_verified_at
       FROM users WHERE id = $1`,
      [id],
    );
    if (r.rowCount === 0) {
      return null;
    }
    const row = r.rows[0];
    return {
      id: row.id,
      email: row.email,
      passwordHash: row.password_hash,
      emailVerifiedAt: row.email_verified_at,
    };
  }

  async createUser(email: string, password: string): Promise<CloudUser> {
    const passwordHash = await bcrypt.hash(password, 12);
    const r = await this.pool.query(
      `INSERT INTO users (email, password_hash)
       VALUES ($1, $2)
       RETURNING id, email, password_hash, email_verified_at`,
      [email.toLowerCase(), passwordHash],
    );
    const row = r.rows[0];
    return {
      id: row.id,
      email: row.email,
      passwordHash: row.password_hash,
      emailVerifiedAt: row.email_verified_at,
    };
  }

  async verifyPassword(user: CloudUser, password: string): Promise<boolean> {
    return bcrypt.compare(password, user.passwordHash);
  }

  async createVerifyToken(userId: string): Promise<string> {
    // Drop any previous verify tokens so only the latest link works
    await this.pool.query(
      `DELETE FROM email_tokens WHERE user_id = $1 AND purpose = 'verify'`,
      [userId],
    );
    const token = randomBytes(32).toString("hex");
    const expires = new Date(Date.now() + 24 * 60 * 60 * 1000);
    await this.pool.query(
      `INSERT INTO email_tokens (token, user_id, purpose, expires_at)
       VALUES ($1, $2, 'verify', $3)`,
      [token, userId, expires],
    );
    return token;
  }

  async consumeVerifyToken(token: string): Promise<CloudUser | null> {
    const r = await this.pool.query(
      `SELECT t.user_id, t.expires_at, u.email, u.password_hash, u.email_verified_at, u.id
       FROM email_tokens t
       JOIN users u ON u.id = t.user_id
       WHERE t.token = $1 AND t.purpose = 'verify'`,
      [token],
    );
    if (r.rowCount === 0) {
      return null;
    }
    const row = r.rows[0];
    if (new Date(row.expires_at) < new Date()) {
      await this.pool.query(`DELETE FROM email_tokens WHERE token = $1`, [token]);
      return null;
    }
    await this.pool.query(`UPDATE users SET email_verified_at = NOW() WHERE id = $1`, [
      row.user_id,
    ]);
    await this.pool.query(`DELETE FROM email_tokens WHERE user_id = $1 AND purpose = 'verify'`, [
      row.user_id,
    ]);
    return {
      id: row.id,
      email: row.email,
      passwordHash: row.password_hash,
      emailVerifiedAt: new Date(),
    };
  }
}

export { formatTs };
