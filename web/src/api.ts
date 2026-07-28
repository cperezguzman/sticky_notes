import type { Note, NoteIndexEntry } from "./types";

export class ApiError extends Error {
  status: number;

  constructor(status: number, message: string) {
    super(message);
    this.status = status;
  }
}

const jsonHeaders = { "Content-Type": "application/json" };

async function readError(res: Response): Promise<string> {
  try {
    const data = (await res.json()) as { error?: string };
    if (data.error) {
      return data.error;
    }
  } catch {
    // ignore
  }
  return `HTTP ${res.status}`;
}

async function apiFetch(input: string, init?: RequestInit): Promise<Response> {
  return fetch(input, {
    ...init,
    credentials: "include",
    headers: {
      ...jsonHeaders,
      ...(init?.headers ?? {}),
    },
  });
}

async function expectOk(res: Response): Promise<void> {
  if (!res.ok) {
    throw new ApiError(res.status, await readError(res));
  }
}

export interface AuthMe {
  authRequired: boolean;
  user: string | null;
  email?: string | null;
  storage?: "cloud" | "files";
}

export async function getAuthMe(): Promise<AuthMe> {
  const res = await apiFetch("/auth/me");
  if (res.status === 401) {
    return { authRequired: true, user: null };
  }
  await expectOk(res);
  return res.json() as Promise<AuthMe>;
}

export async function signup(
  email: string,
  password: string,
): Promise<{ ok: boolean; message: string; email: string }> {
  const res = await apiFetch("/auth/signup", {
    method: "POST",
    body: JSON.stringify({ email, password }),
  });
  await expectOk(res);
  return res.json() as Promise<{ ok: boolean; message: string; email: string }>;
}

export async function resendVerification(
  email: string,
): Promise<{ ok: boolean; message: string; email: string }> {
  const res = await apiFetch("/auth/resend-verification", {
    method: "POST",
    body: JSON.stringify({ email }),
  });
  await expectOk(res);
  return res.json() as Promise<{ ok: boolean; message: string; email: string }>;
}

export async function verifyEmail(
  token: string,
): Promise<{ ok: boolean; message: string }> {
  const res = await apiFetch(`/auth/verify?token=${encodeURIComponent(token)}`);
  await expectOk(res);
  return res.json() as Promise<{ ok: boolean; message: string }>;
}

export async function login(email: string, password: string): Promise<AuthMe> {
  const res = await apiFetch("/auth/login", {
    method: "POST",
    body: JSON.stringify({ email, password }),
  });
  await expectOk(res);
  return res.json() as Promise<AuthMe>;
}

export async function logout(): Promise<void> {
  const res = await apiFetch("/auth/logout", { method: "POST", body: "{}" });
  if (res.status !== 204 && !res.ok) {
    throw new ApiError(res.status, await readError(res));
  }
}

export async function listNotes(query = ""): Promise<NoteIndexEntry[]> {
  const q = query.trim();
  const url = q === "" ? "/notes" : `/notes?q=${encodeURIComponent(q)}`;
  const res = await apiFetch(url);
  await expectOk(res);
  return res.json() as Promise<NoteIndexEntry[]>;
}

export async function getNote(id: string | number): Promise<Note> {
  const res = await apiFetch(`/notes/${id}`);
  await expectOk(res);
  return res.json() as Promise<Note>;
}

export async function createNote(title: string, body = ""): Promise<Note> {
  const res = await apiFetch("/notes", {
    method: "POST",
    body: JSON.stringify({ title, body }),
  });
  await expectOk(res);
  return res.json() as Promise<Note>;
}

export async function updateNote(
  id: string | number,
  patch: { title?: string; body?: string; sourceUrl?: string },
): Promise<Note> {
  const res = await apiFetch(`/notes/${id}`, {
    method: "PUT",
    body: JSON.stringify(patch),
  });
  await expectOk(res);
  return res.json() as Promise<Note>;
}

export async function deleteNote(id: string | number): Promise<void> {
  const res = await apiFetch(`/notes/${id}`, { method: "DELETE" });
  await expectOk(res);
}

export async function checkHealth(): Promise<{
  ok: boolean;
  authRequired?: boolean;
  storage?: "cloud" | "files";
}> {
  try {
    const res = await fetch("/health", { credentials: "include" });
    if (!res.ok) {
      return { ok: false };
    }
    return res.json() as Promise<{
      ok: boolean;
      authRequired?: boolean;
      storage?: "cloud" | "files";
    }>;
  } catch {
    return { ok: false };
  }
}
