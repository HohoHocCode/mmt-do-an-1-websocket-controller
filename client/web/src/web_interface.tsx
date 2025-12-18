"use client";

import { useState, useEffect, useRef } from "react";
import {
  Terminal,
  Cpu,
  HardDrive,
  Send,
  XCircle,
  RefreshCw,
  Activity,
  MemoryStick,
  Server,
} from "lucide-react";

// === TYPES ===
interface Log {
  text: string;
  type: "info" | "error" | "success" | "command";
  timestamp: Date;
}

interface Process {
  pid: number;
  name: string;
  memory: number;
}

interface SystemInfo {
  os: string;
  arch: string;
  hostname: string;
  cpuCores: number;
  totalMemory: number;
  availableMemory: number;
}

// === COMPONENT ===
export default function WebInterface() {
  const [logs, setLogs] = useState<Log[]>([]);
  const [processes, setProcesses] = useState<Process[]>([]);
  const [sysInfo, setSysInfo] = useState<SystemInfo | null>(null);
  const [command, setCommand] = useState("");
  const [isLoading, setIsLoading] = useState(false);
  const outputRef = useRef<HTMLDivElement>(null);

  // Auto-scroll logs
  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [logs]);

  // Log helper
  const addLog = (text: string, type: Log["type"] = "info") => {
    setLogs((prev) => [...prev, { text, type, timestamp: new Date() }]);
  };

  // API call wrapper
  const apiCall = async (endpoint: string, options?: RequestInit) => {
    try {
      const res = await fetch(`/api/${endpoint}`, {
        headers: { "Content-Type": "application/json" },
        ...options,
      });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return await res.json();
    } catch (error: unknown) {
      const err = error as Error;
      addLog(`API Error [${endpoint}]: ${err.message}`, "error");
      return null;
    }
  };

  // Load processes
  const loadProcesses = async () => {
    const data = await apiCall("processes");
    if (data && Array.isArray(data)) {
      setProcesses(data);
    }
  };

  // Load system info
  const loadSysInfo = async () => {
    const data = await apiCall("sysinfo");
    if (data) {
      setSysInfo(data);
    }
  };

  // Send command
  const sendCommand = async (cmd: string) => {
    if (!cmd.trim()) return;
    addLog(`$ ${cmd}`, "command");
    setCommand("");
    setIsLoading(true);

    const data = await apiCall("execute", {
      method: "POST",
      body: JSON.stringify({ command: cmd }),
    });

    if (data) {
      addLog(
        data.output || data.error || "No output",
        data.error ? "error" : "success"
      );
    }
    setIsLoading(false);
  };

  // Kill process
  const killProcess = async (pid: number) => {
    const data = await apiCall("kill", {
      method: "POST",
      body: JSON.stringify({ pid }),
    });
    if (data) {
      addLog(data.message, data.error ? "error" : "success");
      loadProcesses();
    }
  };

  // Initial load + auto-refresh
  useEffect(() => {
    loadSysInfo();
    loadProcesses();
    const interval = setInterval(loadProcesses, 5000);
    return () => clearInterval(interval);
  }, []);

  // Enter key handler
  const handleKeyPress = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter" && !isLoading) {
      sendCommand(command);
    }
  };

  return (
    <>
      <div className="min-h-screen bg-background text-foreground p-6">
        <div className="max-w-7xl mx-auto space-y-6">
          {/* Header */}
          <div className="flex items-center justify-between mb-8">
            <h1 className="text-3xl font-bold flex items-center gap-3">
              <Server className="w-8 h-8" />
              Remote Admin Panel
            </h1>
            <button
              onClick={() => {
                loadSysInfo();
                loadProcesses();
                addLog("Refreshed data", "info");
              }}
              className="p-2 rounded-lg bg-secondary hover:bg-secondary/80 transition-colors"
            >
              <RefreshCw className="w-5 h-5" />
            </button>
          </div>

          {/* System Info Grid */}
          {sysInfo && (
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
              <div className="bg-card p-5 rounded-xl border">
                <p className="text-sm text-muted-foreground flex items-center gap-2">
                  <Cpu className="w-4 h-4" /> Hostname
                </p>
                <p className="text-xl font-semibold mt-1">{sysInfo.hostname}</p>
              </div>

              <div className="bg-card p-5 rounded-xl border">
                <p className="text-sm text-muted-foreground flex items-center gap-2">
                  <Activity className="w-4 h-4" /> OS & Arch
                </p>
                <p className="text-xl font-semibold mt-1">
                  {sysInfo.os} ({sysInfo.arch})
                </p>
              </div>

              <div className="bg-card p-5 rounded-xl border">
                <p className="text-sm text-muted-foreground flex items-center gap-2">
                  <Cpu className="w-4 h-4" /> CPU Cores
                </p>
                <p className="text-xl font-semibold mt-1">{sysInfo.cpuCores}</p>
              </div>

              <div className="bg-card p-5 rounded-xl border">
                <p className="text-sm text-muted-foreground flex items-center gap-2">
                  <MemoryStick className="w-4 h-4" /> Total RAM
                </p>
                <p className="text-xl font-semibold mt-1">
                  {sysInfo.totalMemory.toFixed(1)} GB
                </p>
              </div>

              <div className="bg-card p-5 rounded-xl border">
                <p className="text-sm text-muted-foreground flex items-center gap-2">
                  <HardDrive className="w-4 h-4" /> Available
                </p>
                <p className="text-xl font-semibold mt-1">
                  {sysInfo.availableMemory.toFixed(1)} GB
                </p>
              </div>

              <div className="bg-card p-5 rounded-xl border">
                <p className="text-sm text-muted-foreground">Usage</p>
                <p className="text-xl font-semibold mt-1">
                  {(
                    ((sysInfo.totalMemory - sysInfo.availableMemory) /
                      sysInfo.totalMemory) *
                    100
                  ).toFixed(1)}
                  %
                </p>
              </div>
            </div>
          )}

          {/* Process List */}
          <div className="bg-card rounded-xl border overflow-hidden">
            <div className="p-4 border-b bg-muted/50">
              <h2 className="text-lg font-semibold flex items-center gap-2">
                <Terminal className="w-5 h-5" />
                Running Processes ({processes.length})
              </h2>
            </div>
            <div className="overflow-x-auto">
              <table className="w-full">
                <thead className="bg-muted/30">
                  <tr>
                    <th className="px-4 py-3 text-left text-sm font-medium">
                      PID
                    </th>
                    <th className="px-4 py-3 text-left text-sm font-medium">
                      Name
                    </th>
                    <th className="px-4 py-3 text-left text-sm font-medium">
                      Memory (KB)
                    </th>
                    <th className="px-4 py-3 text-left text-sm font-medium">
                      Action
                    </th>
                  </tr>
                </thead>
                <tbody>
                  {processes.length === 0 ? (
                    <tr>
                      <td
                        colSpan={4}
                        className="px-4 py-8 text-center text-muted-foreground"
                      >
                        No processes found
                      </td>
                    </tr>
                  ) : (
                    processes.map((proc) => (
                      <tr
                        key={proc.pid}
                        className="border-t hover:bg-muted/20 transition-colors"
                      >
                        <td className="px-4 py-3 font-mono">{proc.pid}</td>
                        <td className="px-4 py-3">{proc.name}</td>
                        <td className="px-4 py-3 font-mono">
                          {proc.memory.toLocaleString()}
                        </td>
                        <td className="px-4 py-3">
                          <button
                            onClick={() => killProcess(proc.pid)}
                            className="text-red-500 hover:text-red-700 transition-colors"
                            title="Kill Process"
                          >
                            <XCircle className="w-4 h-4" />
                          </button>
                        </td>
                      </tr>
                    ))
                  )}
                </tbody>
              </table>
            </div>
          </div>

          {/* Terminal */}
          <div className="bg-card rounded-xl border overflow-hidden">
            <div className="p-4 border-b bg-muted/50">
              <h2 className="text-lg font-semibold flex items-center gap-2">
                <Terminal className="w-5 h-5" />
                Command Terminal
              </h2>
            </div>
            <div
              ref={outputRef}
              className="p-4 h-64 overflow-y-auto bg-black text-green-400 font-mono text-sm space-y-1"
            >
              {logs.length === 0 ? (
                <div className="text-gray-500">
                  Terminal ready. Type a command below...
                </div>
              ) : (
                logs.map((log, i) => (
                  <div
                    key={i}
                    className={
                      log.type === "command"
                        ? "text-cyan-400"
                        : log.type === "error"
                        ? "text-red-400"
                        : log.type === "success"
                        ? "text-green-400"
                        : "text-gray-300"
                    }
                  >
                    [{log.timestamp.toLocaleTimeString()}] {log.text}
                  </div>
                ))
              )}
            </div>
            <div className="p-3 border-t bg-muted/50 flex gap-2">
              <input
                type="text"
                value={command}
                onChange={(e) => setCommand(e.target.value)}
                onKeyPress={handleKeyPress}
                placeholder="Enter command (e.g., list, sysinfo, ls C:\\)"
                className="flex-1 px-3 py-2 bg-background border rounded-lg focus:outline-none focus:ring-2 focus:ring-primary"
                disabled={isLoading}
              />
              <button
                onClick={() => sendCommand(command)}
                disabled={isLoading || !command.trim()}
                className="px-4 py-2 bg-primary text-primary-foreground rounded-lg hover:bg-primary/90 disabled:opacity-50 disabled:cursor-not-allowed flex items-center gap-2 transition-colors"
              >
                {isLoading ? (
                  <div className="w-4 h-4 border-2 border-t-transparent border-white rounded-full animate-spin" />
                ) : (
                  <Send className="w-4 h-4" />
                )}
                Send
              </button>
            </div>
          </div>

          {/* Footer */}
          <div className="text-center text-sm text-muted-foreground py-4">
            Remote Administration Tool • Connected via Web API
          </div>
        </div>
      </div>
    </>
  );
}
