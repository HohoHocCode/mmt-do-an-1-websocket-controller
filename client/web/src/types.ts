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