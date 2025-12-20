// user_db.ts
// Client-only "database" for username/password.
// - Seeded from ./user_db_seed.json (static file in the repo)
// - Persisted/updated in LocalStorage at runtime
// NOTE: Demo/coursework only. Do NOT store real passwords like this in production.

import seed from "./user_db_seed.json";

export const USER_DB_STORAGE_KEY = "rdc.userdb";

// Stored format: { [username]: password }
export type UserDb = Record<string, string>;

export type VerifyResult =
  | { ok: true; mode: "login" | "register" }
  | { ok: false; error: string };

function isBrowser(): boolean {
  return typeof window !== "undefined" && typeof window.localStorage !== "undefined";
}

function safeParseDb(raw: string | null): UserDb | null {
  if (!raw) return null;
  try {
    const parsed = JSON.parse(raw);
    if (!parsed || typeof parsed !== "object") return null;

    // Ensure values are strings
    const out: UserDb = {};
    for (const [k, v] of Object.entries(parsed as Record<string, unknown>)) {
      if (typeof v === "string") out[k] = v;
    }
    return out;
  } catch {
    return null;
  }
}

/**
 * Load DB from LocalStorage. If missing, it will be initialized from seed file.
 */
export function loadUserDb(): UserDb {
  if (!isBrowser()) return { ...(seed as UserDb) };

  const fromStorage = safeParseDb(localStorage.getItem(USER_DB_STORAGE_KEY));
  if (fromStorage) return fromStorage;

  const initial = { ...(seed as UserDb) };
  saveUserDb(initial);
  return initial;
}

export function saveUserDb(db: UserDb): void {
  if (!isBrowser()) return;
  try {
    localStorage.setItem(USER_DB_STORAGE_KEY, JSON.stringify(db));
  } catch {
    // ignore
  }
}

/**
 * Get one user record.
 */
export function getUser(username: string): { username: string; password: string } | null {
  const u = username.trim();
  if (!u) return null;
  const db = loadUserDb();
  if (!Object.prototype.hasOwnProperty.call(db, u)) return null;
  return { username: u, password: db[u] };
}

/**
 * List all usernames.
 */
export function listUsers(): string[] {
  const db = loadUserDb();
  return Object.keys(db).sort((a, b) => a.localeCompare(b));
}

/**
 * Simple search: returns usernames containing the query (case-insensitive).
 */
export function searchUsers(query: string): string[] {
  const q = query.trim().toLowerCase();
  if (!q) return listUsers();
  return listUsers().filter((u) => u.toLowerCase().includes(q));
}

/**
 * Update/create a user (explicit operation).
 */
export function upsertUser(username: string, password: string): void {
  const u = username.trim();
  const p = password.trim();
  if (!u) return;
  if (!p) return;
  const db = loadUserDb();
  db[u] = p;
  saveUserDb(db);
}

/**
 * Remove a user.
 */
export function deleteUser(username: string): void {
  const u = username.trim();
  if (!u) return;
  const db = loadUserDb();
  if (!Object.prototype.hasOwnProperty.call(db, u)) return;
  delete db[u];
  saveUserDb(db);
}

/**
 * Your rule:
 * - If username exists in DB: password must match.
 * - If username does not exist: accept any non-empty password and register it.
 */
export function verifyOrRegisterUser(username: string, password: string): VerifyResult {
  const u = username.trim();
  const p = password.trim();
  if (!u) return { ok: false, error: "Username is required." };
  if (!p) return { ok: false, error: "Password is required." };

  const db = loadUserDb();
  if (Object.prototype.hasOwnProperty.call(db, u)) {
    if (db[u] !== p) return { ok: false, error: "Incorrect password for this user." };
    return { ok: true, mode: "login" };
  }

  db[u] = p;
  saveUserDb(db);
  return { ok: true, mode: "register" };
}
