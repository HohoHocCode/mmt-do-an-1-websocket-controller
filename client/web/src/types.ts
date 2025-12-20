// src/types.ts
export interface Log {
  text: string;
  type: 'info' | 'error' | 'success' | 'command';
  timestamp: Date;
}

export interface Process {
  pid: number;
  name: string;
  memory: number;
}

export interface SystemInfo {
  os: string;
  arch: string;
  hostname: string;
  cpuCores: number;
  totalMemory: number;
  availableMemory: number;
}

export type Role = 'admin' | 'user';

export interface AuthUser {
  username: string;
  role: Role;
}

export interface DiscoveryDevice {
  name?: string;
  ip?: string;
  wsPort?: number | null;
  version?: string;
  nonce?: string;
  lastSeenMs?: number;
}

export type HotkeyAction = "connect" | "reset" | "stream" | "processes";

export type HotkeyMap = Record<HotkeyAction, string>;

export interface ControllerStatus {
  status: "idle" | "running" | "stopped" | "error";
  pid?: number;
  startedAt?: number;
  stoppedAt?: number;
  lastExit?: { code: number | null; signal: string | null; at: number };
  message?: string;
  command?: string;
}

export interface WsMessage {
  cmd?: string;
  status?: string;
  requestId?: string;
  [key: string]: unknown;
}

export interface FileItem {
  name: string;
  path: string;
  is_dir: boolean;
  size: number;
}

export interface ListFilesResponse extends WsMessage {
  cmd?: "list-files";
  status?: "ok" | "error";
  dir?: string;
  items?: FileItem[];
  error?: string;
  message?: string;
}

export interface DownloadFileResponse extends WsMessage {
  cmd?: "download-file";
  status?: "ok" | "error";
  path?: string;
  offset?: number;
  bytes_read?: number;
  eof?: boolean;
  data_base64?: string;
  error?: string;
  message?: string;
}

export interface DeleteFileResponse extends WsMessage {
  cmd?: "delete-file";
  status?: "ok" | "error";
  path?: string;
  deleted?: boolean;
  error?: string;
  message?: string;
}

export interface ClipboardResponse extends WsMessage {
  cmd?: "clipboard-get";
  status?: "ok" | "error";
  text?: string;
  error?: string;
  message?: string;
}
