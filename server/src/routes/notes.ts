/**
 * REST routes for note CRUD — file mode (numeric ids) or cloud (UUID + user scope).
 */

import { Router, type Request, type Response } from "express";

import type { PostgresNoteRepository } from "../cloud/postgresNoteRepository.js";
import type { NoteRepository } from "../noteRepository.js";
import { normalizeSourceUrl, sourceDomainFromUrl } from "../sourceUrl.js";

function parseBodyFields(req: Request): {
  title?: string;
  body?: string;
  sourceUrl?: string;
  error?: string;
} {
  const out: { title?: string; body?: string; sourceUrl?: string; error?: string } =
    {};
  if (req.body?.title !== undefined) {
    if (typeof req.body.title !== "string") {
      return { error: "title must be a string" };
    }
    out.title = req.body.title;
  }
  if (req.body?.body !== undefined) {
    if (typeof req.body.body !== "string") {
      return { error: "body must be a string" };
    }
    out.body = req.body.body;
  }
  if (req.body?.sourceUrl !== undefined) {
    const normalized = normalizeSourceUrl(req.body.sourceUrl);
    if (!normalized.ok) {
      return { error: normalized.error };
    }
    out.sourceUrl = normalized.url;
  }
  return out;
}

export function createNotesRouter(repo: NoteRepository): Router {
  const router = Router();

  router.get("/", async (req: Request, res: Response) => {
    const q = typeof req.query.q === "string" ? req.query.q : undefined;
    res.json(await repo.list(q));
  });

  router.get("/context", async (req: Request, res: Response) => {
    const raw = typeof req.query.url === "string" ? req.query.url : "";
    const normalized = normalizeSourceUrl(raw);
    if (!normalized.ok || normalized.url === "") {
      res.status(400).json({ error: "url must be a valid http(s) URL" });
      return;
    }
    const notes = await repo.listByContext(normalized.url);
    res.json({
      url: normalized.url,
      domain: sourceDomainFromUrl(normalized.url),
      notes,
    });
  });

  router.get("/:id", async (req: Request, res: Response) => {
    const id = Number.parseInt(String(req.params.id), 10);
    if (Number.isNaN(id)) {
      res.status(400).json({ error: "id must be an integer" });
      return;
    }
    const note = await repo.getById(id);
    if (!note) {
      res.status(404).json({ error: "note not found" });
      return;
    }
    res.json(note);
  });

  router.post("/", async (req: Request, res: Response) => {
    const fields = parseBodyFields(req);
    if (fields.error) {
      res.status(400).json({ error: fields.error });
      return;
    }
    const title = typeof req.body?.title === "string" ? req.body.title : "";
    const body = typeof req.body?.body === "string" ? req.body.body : "";
    const note = await repo.create(title, body, fields.sourceUrl ?? "");
    res.status(201).json(note);
  });

  router.put("/:id", async (req: Request, res: Response) => {
    const id = Number.parseInt(String(req.params.id), 10);
    if (Number.isNaN(id)) {
      res.status(400).json({ error: "id must be an integer" });
      return;
    }
    const fields = parseBodyFields(req);
    if (fields.error) {
      res.status(400).json({ error: fields.error });
      return;
    }
    if (
      fields.title === undefined &&
      fields.body === undefined &&
      fields.sourceUrl === undefined
    ) {
      res.status(400).json({ error: "provide title, body, and/or sourceUrl" });
      return;
    }
    const note = await repo.update(id, fields);
    if (!note) {
      res.status(404).json({ error: "note not found" });
      return;
    }
    res.json(note);
  });

  router.delete("/:id", async (req: Request, res: Response) => {
    const id = Number.parseInt(String(req.params.id), 10);
    if (Number.isNaN(id)) {
      res.status(400).json({ error: "id must be an integer" });
      return;
    }
    const deleted = await repo.delete(id);
    if (!deleted) {
      res.status(404).json({ error: "note not found" });
      return;
    }
    res.status(204).send();
  });

  return router;
}

const UUID_RE =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;

export function createCloudNotesRouter(repo: PostgresNoteRepository): Router {
  const router = Router();

  function userId(req: Request): string | null {
    return req.session?.userId ?? null;
  }

  router.get("/", async (req: Request, res: Response) => {
    const uid = userId(req);
    if (!uid) {
      res.status(401).json({ error: "unauthorized" });
      return;
    }
    const q = typeof req.query.q === "string" ? req.query.q : undefined;
    res.json(await repo.list(uid, q));
  });

  router.get("/context", async (req: Request, res: Response) => {
    const uid = userId(req);
    if (!uid) {
      res.status(401).json({ error: "unauthorized" });
      return;
    }
    const raw = typeof req.query.url === "string" ? req.query.url : "";
    const normalized = normalizeSourceUrl(raw);
    if (!normalized.ok || normalized.url === "") {
      res.status(400).json({ error: "url must be a valid http(s) URL" });
      return;
    }
    const notes = await repo.listByContext(uid, normalized.url);
    res.json({
      url: normalized.url,
      domain: sourceDomainFromUrl(normalized.url),
      notes,
    });
  });

  router.get("/:id", async (req: Request, res: Response) => {
    const uid = userId(req);
    if (!uid) {
      res.status(401).json({ error: "unauthorized" });
      return;
    }
    const id = String(req.params.id);
    if (!UUID_RE.test(id)) {
      res.status(400).json({ error: "id must be a UUID" });
      return;
    }
    const note = await repo.getById(uid, id);
    if (!note) {
      res.status(404).json({ error: "note not found" });
      return;
    }
    res.json(note);
  });

  router.post("/", async (req: Request, res: Response) => {
    const uid = userId(req);
    if (!uid) {
      res.status(401).json({ error: "unauthorized" });
      return;
    }
    const fields = parseBodyFields(req);
    if (fields.error) {
      res.status(400).json({ error: fields.error });
      return;
    }
    const title = typeof req.body?.title === "string" ? req.body.title : "";
    const body = typeof req.body?.body === "string" ? req.body.body : "";
    const note = await repo.create(uid, title, body, fields.sourceUrl ?? "");
    res.status(201).json(note);
  });

  router.put("/:id", async (req: Request, res: Response) => {
    const uid = userId(req);
    if (!uid) {
      res.status(401).json({ error: "unauthorized" });
      return;
    }
    const id = String(req.params.id);
    if (!UUID_RE.test(id)) {
      res.status(400).json({ error: "id must be a UUID" });
      return;
    }
    const fields = parseBodyFields(req);
    if (fields.error) {
      res.status(400).json({ error: fields.error });
      return;
    }
    if (
      fields.title === undefined &&
      fields.body === undefined &&
      fields.sourceUrl === undefined
    ) {
      res.status(400).json({ error: "provide title, body, and/or sourceUrl" });
      return;
    }
    const note = await repo.update(uid, id, fields);
    if (!note) {
      res.status(404).json({ error: "note not found" });
      return;
    }
    res.json(note);
  });

  router.delete("/:id", async (req: Request, res: Response) => {
    const uid = userId(req);
    if (!uid) {
      res.status(401).json({ error: "unauthorized" });
      return;
    }
    const id = String(req.params.id);
    if (!UUID_RE.test(id)) {
      res.status(400).json({ error: "id must be a UUID" });
      return;
    }
    const deleted = await repo.delete(uid, id);
    if (!deleted) {
      res.status(404).json({ error: "note not found" });
      return;
    }
    res.status(204).send();
  });

  return router;
}
