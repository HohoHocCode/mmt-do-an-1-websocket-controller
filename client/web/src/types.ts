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
}
