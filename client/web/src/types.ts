export type LogType = "info" | "error" | "success" | "command";

export interface LogEntry {
  text: string;
  type: LogType;
  timestamp: Date;
}

export interface ProcessInfo {
  pid: number;
  name: string;
  memory?: number;
}

export type MediaKind = "screen" | "camera" | "screen_stream";

export interface LastImage {
  kind: MediaKind;
  base64: string;
  ts: number;
  seq?: number;
}

export interface LastVideo {
  base64: string;
  format: string;
  ts: number;
  url?: string;
}

export interface StreamState {
  running: boolean;
  fps: number;
  duration: number;
  lastSeq: number;
}

export type TaskKind =
  | "idle"
  | "screen"
  | "camera"
  | "camera_video"
  | "screen_stream"
  | "process_list"
  | "process_start"
  | "process_kill"
  | "ping"
  | "custom"
  | "reset";

export type TaskStatus = "idle" | "running" | "cancelling";

export interface TargetTaskState {
  type: TaskKind;
  status: TaskStatus;
  label?: string;
}

export interface TargetUiState {
  loadingMedia: boolean;
  loadingProcesses: boolean;
}

export interface RemoteTarget {
  id: string;
  name: string;
  host: string;
  port: number;
  connected: boolean;
  connecting: boolean;
  ws?: WebSocket;
  logs: LogEntry[];
  processes: ProcessInfo[];
  lastImage?: LastImage;
  lastVideo?: LastVideo;
  stream: StreamState;
  task: TargetTaskState;
  ui: TargetUiState;
}
