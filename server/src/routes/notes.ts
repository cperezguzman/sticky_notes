import { Router, type Request, type Response } from "express";

import type { NoteRepository } from "../noteRepository.js";

export function createNotesRouter(repo: NoteRepository): Router {
  const router = Router();

  router.get("/", async (_req: Request, res: Response) => {
    const notes = await repo.list();
    res.json(notes);
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
    const title = typeof req.body?.title === "string" ? req.body.title : "";
    const body = typeof req.body?.body === "string" ? req.body.body : "";
    if (req.body?.title !== undefined && typeof req.body.title !== "string") {
      res.status(400).json({ error: "title must be a string" });
      return;
    }
    if (req.body?.body !== undefined && typeof req.body.body !== "string") {
      res.status(400).json({ error: "body must be a string" });
      return;
    }
    const note = await repo.create(title, body);
    res.status(201).json(note);
  });

  router.put("/:id", async (req: Request, res: Response) => {
    const id = Number.parseInt(String(req.params.id), 10);
    if (Number.isNaN(id)) {
      res.status(400).json({ error: "id must be an integer" });
      return;
    }
    const patch: { title?: string; body?: string } = {};
    if (req.body?.title !== undefined) {
      if (typeof req.body.title !== "string") {
        res.status(400).json({ error: "title must be a string" });
        return;
      }
      patch.title = req.body.title;
    }
    if (req.body?.body !== undefined) {
      if (typeof req.body.body !== "string") {
        res.status(400).json({ error: "body must be a string" });
        return;
      }
      patch.body = req.body.body;
    }
    if (patch.title === undefined && patch.body === undefined) {
      res.status(400).json({ error: "provide title and/or body" });
      return;
    }
    const note = await repo.update(id, patch);
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
