import { AuthUser } from "./types";

const API_URL = import.meta.env.VITE_API_URL || "http://localhost:5179";

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

export type LoginResponse = { token: string; user: AuthUser };
export type StatusResponse = { exists: boolean; hasPassword: boolean };

export function getStatus(username: string) {
  const qs = new URLSearchParams({ username });
  return request<StatusResponse>(`/auth/status?${qs.toString()}`);
}

export function setPassword(payload: { username: string; setupToken: string; newPassword: string }) {
  return request<{ ok: boolean }>("/auth/set-password", {
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

export function discoverDevices(token: string) {
  return request<{ devices: Array<Record<string, unknown>> }>("/devices/discover", { method: "GET" }, token);
}
