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

export type ConnectionStatus = "connecting" | "connected" | "disconnected";
export type PeerStatus = "new" | "connecting" | "connected" | "failed" | "disconnected";
export type ControlStatus = "not_requested" | "pending" | "granted" | "revoked";

export type WebRtcRole = "host" | "viewer";
export type WebRtcAction = "join" | "joined" | "signal" | "leave" | "peer-ready" | "peer-left" | "error";

export interface WebRtcSignalData {
  sdp?: RTCSessionDescriptionInit;
  candidate?: RTCIceCandidateInit;
}

export interface WebRtcMessage {
  type: "webrtc";
  roomId: string;
  role: WebRtcRole;
  action: WebRtcAction;
  data?: WebRtcSignalData;
  status?: string;
  message?: string;
}

export type ControlMessage =
  | { t: "control-request" }
  | { t: "control-granted" }
  | { t: "control-revoked" }
  | {
      t: "mouse";
      action: "move" | "down" | "up" | "wheel";
      x?: number;
      y?: number;
      button?: "left" | "middle" | "right";
      deltaY?: number;
    }
  | {
      t: "key";
      action: "down" | "up";
      code: string;
      key: string;
      ctrl: boolean;
      alt: boolean;
      shift: boolean;
      meta: boolean;
    };