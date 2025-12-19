export type UserRole = "admin" | "user";

export interface UserRow {
  id: number;
  username: string;
  role: UserRole;
  password_hash: string | null;
  setup_token_hash: string | null;
  setup_token_created_at: Date | null;
  created_at: Date;
  updated_at: Date;
}

export interface AuthenticatedUser {
  id: number;
  username: string;
  role: UserRole;
}

export interface DiscoveryResult {
  ip: string;
  wsPort: number | null;
  name?: string;
  version?: string;
  nonce?: string;
  lastSeenMs: number;
}

export interface ControllerStatus {
  status: "idle" | "running" | "stopped" | "error";
  pid?: number;
  startedAt?: number;
  stoppedAt?: number;
  lastExit?: { code: number | null; signal: NodeJS.Signals | null; at: number };
  message?: string;
  command?: string;
}
