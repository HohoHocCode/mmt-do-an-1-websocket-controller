import { AuthUser, ControllerStatus, DiscoveryDevice } from "./types";

const RAW_API_URL = (import.meta.env.VITE_API_URL ?? "").trim();
const DEFAULT_API_BASE = "/api";
const API_BASE = RAW_API_URL === "" ? DEFAULT_API_BASE : RAW_API_URL;
const API_URL = API_BASE.endsWith("/") ? API_BASE.slice(0, -1) : API_BASE;

export class ApiError extends Error {
  status?: number;
  data?: unknown;
  constructor(message: string, status?: number, data?: unknown) {
    super(message);
    this.status = status;
    this.data = data;
  }
}

async function request<T>(
  path: string,
  options: RequestInit = {},
  token?: string
): Promise<T> {
  const headers: Record<string, string> = {
    "Content-Type": "application/json",
    ...(options.headers as Record<string, string> | undefined),
  };

  if (token) headers.Authorization = `Bearer ${token}`;

  const resp = await fetch(`${API_URL}${path}`, { ...options, headers });
  const data = await resp.json().catch(() => null);

  if (!resp.ok) {
    const message = (data as any)?.error || `Request failed (${resp.status})`;
    throw new ApiError(message, resp.status, data);
  }
  return data as T;
}

export type LoginResponse =
  | { token: string; user: AuthUser }
  | { status: "not_found" | "needs_password_set" };
export type StatusResponse = { exists: boolean; hasPassword: boolean };

export function precheckUser(username: string) {
  return request<StatusResponse>("/auth/precheck", {
    method: "POST",
    body: JSON.stringify({ username }),
  });
}

export function registerUser(payload: { username: string; password?: string }) {
  return request<{ ok: boolean; user?: AuthUser; error?: string }>("/auth/register", {
    method: "POST",
    body: JSON.stringify(payload),
  });
}

export function setPassword(payload: { username: string; password: string }) {
  return request<{ ok: boolean; error?: string }>("/auth/set-password", {
    method: "POST",
    body: JSON.stringify(payload),
  });
}

export function login(payload: { username: string; password: string }) {
  return request<LoginResponse>("/auth/login", {
    method: "POST",
    body: JSON.stringify(payload),
  });
}

export function verifyToken(token: string) {
  return request<{ ok: boolean; user?: AuthUser }>("/auth/verify", {
    method: "POST",
    body: JSON.stringify({ token }),
  });
}

export function logAudit(token: string, action: string, meta: Record<string, unknown> = {}) {
  return request<{ ok: boolean }>(
    "/audit",
    { method: "POST", body: JSON.stringify({ action, meta }) },
    token
  );
}

export function discoverDevices(token: string, payload?: { timeoutMs?: number; retries?: number; port?: number }) {
  return request<{ ok: boolean; devices: DiscoveryDevice[] }>(
    "/discover/start",
    { method: "POST", body: JSON.stringify(payload ?? {}) },
    token
  );
}

export function getControllerStatus(token: string) {
  return request<{ ok: boolean; status: ControllerStatus }>("/controller/status", { method: "GET" }, token);
}

export function restartController(token: string) {
  return request<{ ok: boolean; status: ControllerStatus }>("/controller/restart", { method: "POST" }, token);
}

export function stopController(token: string) {
  return request<{ ok: boolean; status: ControllerStatus }>("/controller/stop", { method: "POST" }, token);
}
