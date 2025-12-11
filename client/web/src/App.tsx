import { useEffect, useMemo, useRef, useState } from "react";
import {
  Server,
  Plus,
  Send,
  Power,
  Activity,
  Cpu,
  HardDrive,
  MemoryStick,
  Terminal,
  Users,
  RadioTower,
  RefreshCw,
  XCircle,
  Wifi,
  Trash2,
  FileWarning,
  ShieldAlert,
  FolderOpen,
} from "lucide-react";

type LogType = "info" | "success" | "error" | "command";

interface Log {
  text: string;
  type: LogType;
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

interface RemoteDesktop {
  id: string;
  name: string;
  host: string;
  port: number;
  username: string;
  connected: boolean;
  ws?: WebSocket;

  logs: Log[];
  processes: Process[];
  sysInfo: SystemInfo | null;
  output: string;
}

const DEFAULT_PORT = 8081;

const createId = () => `${Date.now()}-${Math.random().toString(16).slice(2)}`;

function formatLogTime(date: Date) {
  return date.toLocaleTimeString();
}

export default function App() {
  const [desktops, setDesktops] = useState<RemoteDesktop[]>([]);
  const [activeId, setActiveId] = useState<string | null>(null);

  // Form thêm desktop mới
  const [newName, setNewName] = useState("");
  const [newHost, setNewHost] = useState("");
  const [newPort, setNewPort] = useState(DEFAULT_PORT.toString());
  const [globalUsername, setGlobalUsername] = useState("admin");

  // Command
  const [command, setCommand] = useState("");
  const [broadcastMode, setBroadcastMode] = useState(false);
  const [selectedForBroadcast, setSelectedForBroadcast] = useState<string[]>(
    []
  );

  const logsEndRef = useRef<HTMLDivElement | null>(null);
  const outputEndRef = useRef<HTMLDivElement | null>(null);
  const commandInputRef = useRef<HTMLInputElement>(null); // Ref để focus ô nhập lệnh

  const activeDesktop = useMemo(
    () => desktops.find((d) => d.id === activeId) ?? desktops[0] ?? null,
    [desktops, activeId]
  );

  // Nếu chưa chọn activeId mà đã có desktop -> chọn desktop đầu tiên
  useEffect(() => {
    if (!activeId && desktops.length > 0) {
      setActiveId(desktops[0].id);
    }
  }, [desktops, activeId]);

  // Auto scroll logs & output
  useEffect(() => {
    if (logsEndRef.current) {
      logsEndRef.current.scrollIntoView({ behavior: "smooth", block: "end" });
    }
  }, [desktops, activeId]);

  useEffect(() => {
    if (outputEndRef.current) {
      outputEndRef.current.scrollIntoView({ behavior: "smooth", block: "end" });
    }
  }, [desktops, activeId]);

  // Auto refresh processes + sysinfo cho các desktop đã kết nối
  useEffect(() => {
    if (desktops.length === 0) return;

    const interval = setInterval(() => {
      desktops.forEach((d) => {
        if (d.connected && d.ws && d.ws.readyState === WebSocket.OPEN) {
          d.ws.send(JSON.stringify({ type: "processes" }));
          d.ws.send(JSON.stringify({ type: "sysinfo" }));
        }
      });
    }, 5000);

    return () => clearInterval(interval);
  }, [desktops]);

  const addLog = (id: string, text: string, type: LogType = "info") => {
    setDesktops((prev) =>
      prev.map((d) =>
        d.id === id
          ? {
              ...d,
              logs: [...d.logs, { text, type, timestamp: new Date() }],
            }
          : d
      )
    );
  };

  const handleAddDesktop = () => {
    const host = newHost.trim();
    const name = newName.trim() || host || "Remote Desktop";
    if (!host) return;

    const portNum = parseInt(newPort || `${DEFAULT_PORT}`, 10) || DEFAULT_PORT;

    const id = createId();
    const desktop: RemoteDesktop = {
      id,
      name,
      host,
      port: portNum,
      username: globalUsername.trim() || "admin",
      connected: false,
      logs: [],
      processes: [],
      sysInfo: null,
      output: "",
    };

    setDesktops((prev) => [...prev, desktop]);
    setNewName("");
    setNewHost("");
    setNewPort(DEFAULT_PORT.toString());
    if (!activeId) {
      setActiveId(id);
    }
  };

  const connectDesktop = (id: string) => {
    setDesktops((prev) =>
      prev.map((d) => {
        if (d.id !== id) return d;

        if (
          d.ws &&
          (d.ws.readyState === WebSocket.OPEN ||
            d.ws.readyState === WebSocket.CONNECTING)
        ) {
          return d;
        }

        const ws = new WebSocket(`ws://${d.host}:${d.port}`);

        ws.onopen = () => {
          addLog(id, `Kết nối tới ${d.host}:${d.port}`, "success");
          ws.send(
            JSON.stringify({
              type: "login",
              username: d.username || globalUsername || "admin",
            })
          );
        };

        ws.onerror = () => {
          addLog(id, "Lỗi WebSocket (không kết nối được?)", "error");
        };

        ws.onclose = () => {
          addLog(id, "Đã ngắt kết nối", "error");
          setDesktops((inner) =>
            inner.map((dd) =>
              dd.id === id ? { ...dd, connected: false, ws: undefined } : dd
            )
          );
        };

        ws.onmessage = (event) => {
          try {
            const data = JSON.parse(event.data);

            setDesktops((inner) =>
              inner.map((dd) => {
                if (dd.id !== id) return dd;

                if (data.type === "login" && data.success) {
                  const serverInfo = data.serverInfo || data.system || {};
                  const newSysInfo: SystemInfo | null = serverInfo.os
                    ? {
                        os: serverInfo.os,
                        arch: serverInfo.arch || "",
                        hostname: serverInfo.hostname || "",
                        cpuCores: serverInfo.cpuCores ?? 0,
                        totalMemory: serverInfo.totalMemory ?? 0,
                        availableMemory: serverInfo.availableMemory ?? 0,
                      }
                    : dd.sysInfo;

                  addLog(
                    id,
                    `Login thành công • ${serverInfo.os ?? ""} • ${
                      serverInfo.hostname ?? ""
                    }`,
                    "success"
                  );

                  return {
                    ...dd,
                    connected: true,
                    sysInfo: newSysInfo,
                  };
                }

                if (data.type === "command") {
                  return {
                    ...dd,
                    output: dd.output + "\n" + (data.result ?? ""),
                  };
                }

                if (data.type === "processes") {
                  return {
                    ...dd,
                    processes: Array.isArray(data.processes)
                      ? data.processes
                      : dd.processes,
                  };
                }

                if (data.type === "sysinfo") {
                  const info = data.system;
                  if (!info) return dd;
                  const sys: SystemInfo = {
                    os: info.os ?? "",
                    arch: info.arch ?? "",
                    hostname: info.hostname ?? "",
                    cpuCores: info.cpuCores ?? 0,
                    totalMemory: info.totalMemory ?? 0,
                    availableMemory: info.availableMemory ?? 0,
                  };
                  return {
                    ...dd,
                    sysInfo: sys,
                  };
                }

                return dd;
              })
            );
          } catch (err) {
            addLog(id, "Lỗi parse JSON từ server", "error");
          }
        };

        return { ...d, ws };
      })
    );
  };

  const disconnectDesktop = (id: string) => {
    setDesktops((prev) =>
      prev.map((d) => {
        if (d.id !== id) return d;

        if (d.ws && d.ws.readyState === WebSocket.OPEN) {
          d.ws.close();
        }
        return { ...d, connected: false, ws: undefined };
      })
    );
  };

  const toggleBroadcastSelection = (id: string) => {
    setSelectedForBroadcast((prev) =>
      prev.includes(id) ? prev.filter((x) => x !== id) : [...prev, id]
    );
  };

  const sendCommandToDesktop = (
    id: string,
    cmd: string,
    labelPrefix?: string
  ) => {
    const trimmed = cmd.trim();
    if (!trimmed) return;

    setDesktops((prev) =>
      prev.map((d) => {
        if (d.id !== id) return d;

        if (!d.ws || d.ws.readyState !== WebSocket.OPEN) {
          return {
            ...d,
            logs: [
              ...d.logs,
              {
                text: "Không thể gửi lệnh: chưa kết nối",
                type: "error",
                timestamp: new Date(),
              },
            ],
          };
        }

        const displayCmd = labelPrefix
          ? `[${labelPrefix}] $ ${trimmed}`
          : `$ ${trimmed}`;
        d.ws.send(JSON.stringify({ type: "command", command: trimmed }));

        return {
          ...d,
          output: d.output + "\n$ " + trimmed,
          logs: [
            ...d.logs,
            {
              text: displayCmd,
              type: "command",
              timestamp: new Date(),
            },
          ],
        };
      })
    );
  };

  const handleSendCommand = () => {
    const trimmed = command.trim();
    if (!trimmed) return;

    if (broadcastMode) {
      if (selectedForBroadcast.length === 0) return;
      selectedForBroadcast.forEach((id) =>
        sendCommandToDesktop(id, trimmed, "BROADCAST")
      );
    } else if (activeDesktop) {
      sendCommandToDesktop(activeDesktop.id, trimmed);
    }

    setCommand("");
  };

  const handleKeyPressCommand = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter") {
      handleSendCommand();
    }
  };

  // Hàm hỗ trợ điền nhanh lệnh vào ô input (dành cho lệnh cần tham số)
  const prefillCommand = (prefix: string) => {
    setCommand(prefix);
    if (commandInputRef.current) {
      commandInputRef.current.focus();
    }
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-950 via-slate-900 to-slate-950 text-slate-100 p-4 md:p-6 font-sans">
      <div className="max-w-7xl mx-auto flex flex-col gap-4 h-[calc(100vh-2rem)]">
        {/* Header */}
        <header className="flex items-center justify-between gap-4">
          <div className="flex items-center gap-3">
            <div className="p-2 rounded-xl bg-purple-500/10 border border-purple-500/40">
              <Server className="w-7 h-7 text-purple-400" />
            </div>
            <div>
              <h1 className="text-2xl md:text-3xl font-bold tracking-tight">
                Remote Control <span className="text-purple-500">Center</span>
              </h1>
              <p className="text-sm text-slate-400">
                Network Administration & Security Tool
              </p>
            </div>
          </div>

          <div className="flex items-center gap-3">
            <div className="hidden sm:flex items-center gap-2 px-3 py-2 rounded-xl bg-slate-900/70 border border-slate-700/70">
              <Users className="w-4 h-4 text-slate-400" />
              <span className="text-xs text-slate-300">
                Connected:{" "}
                <span className="font-semibold text-purple-400">
                  {desktops.length}
                </span>
              </span>
            </div>
          </div>
        </header>

        <div className="flex-1 flex flex-col md:flex-row gap-4 min-h-0">
          {/* Sidebar */}
          <aside className="w-full md:w-72 flex flex-col gap-4">
            {/* Global username */}
            <div className="bg-slate-900/70 border border-slate-700/70 rounded-2xl p-4 space-y-3 shadow-lg">
              <div className="flex items-center justify-between gap-2 mb-1">
                <span className="text-sm font-medium flex items-center gap-2 text-slate-300">
                  <Users className="w-4 h-4 text-purple-400" />
                  Default Username
                </span>
              </div>
              <input
                type="text"
                value={globalUsername}
                onChange={(e) => setGlobalUsername(e.target.value)}
                className="w-full px-3 py-2 rounded-lg bg-slate-800/80 border border-slate-700 text-sm focus:outline-none focus:ring-2 focus:ring-purple-500/70 placeholder-slate-500"
                placeholder="VD: admin"
              />
            </div>

            {/* Add desktop form */}
            <div className="bg-slate-900/70 border border-slate-700/70 rounded-2xl p-4 space-y-3 shadow-lg">
              <div className="flex items-center justify-between gap-2 mb-1">
                <span className="text-sm font-medium flex items-center gap-2 text-slate-300">
                  <Plus className="w-4 h-4 text-purple-400" />
                  Add Target
                </span>
              </div>

              <div className="space-y-2">
                <input
                  type="text"
                  value={newName}
                  onChange={(e) => setNewName(e.target.value)}
                  className="w-full px-3 py-2 rounded-lg bg-slate-800/80 border border-slate-700 text-sm focus:outline-none focus:ring-2 focus:ring-purple-500/70 placeholder-slate-500"
                  placeholder="Display Name"
                />
                <input
                  type="text"
                  value={newHost}
                  onChange={(e) => setNewHost(e.target.value)}
                  className="w-full px-3 py-2 rounded-lg bg-slate-800/80 border border-slate-700 text-sm focus:outline-none focus:ring-2 focus:ring-purple-500/70 placeholder-slate-500"
                  placeholder="IP Address (e.g. 192.168.1.10)"
                />
                <input
                  type="number"
                  value={newPort}
                  onChange={(e) => setNewPort(e.target.value)}
                  className="w-full px-3 py-2 rounded-lg bg-slate-800/80 border border-slate-700 text-sm focus:outline-none focus:ring-2 focus:ring-purple-500/70 placeholder-slate-500"
                  placeholder="Port (8081)"
                />
              </div>

              <button
                onClick={handleAddDesktop}
                className="w-full mt-2 inline-flex items-center justify-center gap-2 px-3 py-2 rounded-lg bg-purple-600 hover:bg-purple-500 text-sm font-medium transition shadow-md shadow-purple-900/20"
              >
                <Plus className="w-4 h-4" />
                Add Target
              </button>
            </div>

            {/* Broadcast options */}
            <div className="bg-slate-900/70 border border-slate-700/70 rounded-2xl p-4 space-y-3 shadow-lg">
              <div className="flex items-center justify-between gap-2">
                <div className="flex items-center gap-2">
                  <RadioTower className="w-4 h-4 text-purple-400" />
                  <span className="text-sm font-medium text-slate-300">
                    Broadcast Mode
                  </span>
                </div>
                <button
                  onClick={() => {
                    setBroadcastMode((v) => !v);
                    setSelectedForBroadcast([]);
                  }}
                  className={`relative inline-flex h-6 w-11 items-center rounded-full transition ${
                    broadcastMode ? "bg-purple-600" : "bg-slate-700"
                  }`}
                >
                  <span
                    className={`inline-block h-4 w-4 transform rounded-full bg-white transition-transform ${
                      broadcastMode ? "translate-x-6" : "translate-x-1"
                    }`}
                  />
                </button>
              </div>
            </div>

            {/* Desktop list */}
            <div className="flex-1 bg-slate-900/70 border border-slate-700/70 rounded-2xl p-3 flex flex-col min-h-0 shadow-lg">
              <div className="flex items-center justify-between mb-2 px-1">
                <span className="text-xs font-semibold uppercase tracking-wide text-slate-500">
                  Targets
                </span>
              </div>

              <div className="flex-1 overflow-auto space-y-2 pr-1 custom-scrollbar">
                {desktops.length === 0 && (
                  <p className="text-xs text-slate-500 px-1 text-center py-4">
                    No targets added.
                  </p>
                )}

                {desktops.map((d) => {
                  const isActive = d.id === activeDesktop?.id;
                  const inBroadcast = selectedForBroadcast.includes(d.id);

                  return (
                    <div
                      key={d.id}
                      className={`relative rounded-xl border px-3 py-3 text-xs cursor-pointer transition-all ${
                        isActive
                          ? "bg-slate-800 border-purple-500/50 shadow-md shadow-purple-900/10"
                          : "bg-slate-900/40 border-slate-800 hover:border-slate-600"
                      }`}
                      onClick={() => setActiveId(d.id)}
                    >
                      <div className="flex items-start justify-between gap-2">
                        <div className="flex-1 min-w-0">
                          <div className="flex items-center gap-2">
                            <span className="font-bold text-slate-200 truncate text-sm">
                              {d.name}
                            </span>
                            <span
                              className={`h-2 w-2 rounded-full ring-2 ring-slate-900 ${
                                d.connected
                                  ? "bg-emerald-400 shadow-[0_0_8px_rgba(52,211,153,0.6)]"
                                  : "bg-slate-600"
                              }`}
                            />
                          </div>
                          <p className="text-[11px] text-slate-400 truncate mt-1">
                            {d.host}:{d.port}
                          </p>
                          {d.sysInfo && (
                            <p className="text-[10px] text-slate-500 mt-0.5 truncate font-mono">
                              {d.sysInfo.os}
                            </p>
                          )}
                        </div>
                        <div className="flex flex-col items-end gap-2 ml-2">
                          <div className="flex items-center gap-1.5">
                            {broadcastMode && (
                              <button
                                onClick={(e) => {
                                  e.stopPropagation();
                                  toggleBroadcastSelection(d.id);
                                }}
                                className={`h-6 w-6 flex items-center justify-center rounded-lg border transition ${
                                  inBroadcast
                                    ? "bg-purple-600 border-purple-500 text-white"
                                    : "bg-slate-800 border-slate-600 text-transparent"
                                }`}
                              >
                                ✓
                              </button>
                            )}
                            {d.connected ? (
                              <button
                                onClick={(e) => {
                                  e.stopPropagation();
                                  disconnectDesktop(d.id);
                                }}
                                className="inline-flex items-center justify-center h-7 w-7 rounded-lg bg-red-500/10 text-red-400 border border-red-500/20 hover:bg-red-500/20 transition"
                                title="Disconnect"
                              >
                                <Power className="w-3.5 h-3.5" />
                              </button>
                            ) : (
                              <button
                                onClick={(e) => {
                                  e.stopPropagation();
                                  connectDesktop(d.id);
                                }}
                                className="inline-flex items-center justify-center h-7 w-7 rounded-lg bg-emerald-500/10 text-emerald-400 border border-emerald-500/20 hover:bg-emerald-500/20 transition"
                                title="Connect"
                              >
                                <Power className="w-3.5 h-3.5" />
                              </button>
                            )}
                          </div>
                        </div>
                      </div>
                    </div>
                  );
                })}
              </div>
            </div>
          </aside>

          {/* Main content */}
          <main className="flex-1 flex flex-col gap-4 min-h-0">
            {/* Command bar */}
            <div className="bg-slate-900/80 border border-slate-700/80 rounded-2xl p-4 flex flex-col gap-3 shadow-lg">
              <div className="flex items-center justify-between gap-2 flex-wrap">
                <div className="flex items-center gap-2 text-sm">
                  <div className="p-1.5 bg-slate-800 rounded-lg border border-slate-700">
                    <Terminal className="w-4 h-4 text-purple-400" />
                  </div>
                  <span className="font-medium text-slate-200">
                    Remote Shell
                    {activeDesktop && !broadcastMode && (
                      <span className="text-slate-500 ml-2 font-normal">
                        Target: {activeDesktop.name}
                      </span>
                    )}
                  </span>
                </div>
                <div className="flex items-center gap-2 text-xs font-mono bg-slate-950/50 px-2 py-1 rounded border border-slate-800 text-slate-400">
                  <Activity className="w-3 h-3" />
                  <span>
                    {broadcastMode
                      ? `BROADCAST: ${selectedForBroadcast.length} TARGETS`
                      : "SINGLE TARGET MODE"}
                  </span>
                </div>
              </div>

              {/* Quick Actions Panel */}
              <div className="grid grid-cols-2 md:grid-cols-4 gap-2">
                <button
                  onClick={() => {
                    if (activeDesktop)
                      sendCommandToDesktop(activeDesktop.id, "wifi");
                  }}
                  className="flex items-center justify-center gap-2 px-3 py-2 bg-blue-500/10 text-blue-400 border border-blue-500/20 rounded-lg hover:bg-blue-500/20 transition text-xs font-medium"
                >
                  <Wifi className="w-3.5 h-3.5" />
                  Get Wifi Pass
                </button>
                <button
                  onClick={() => prefillCommand("ls .")}
                  className="flex items-center justify-center gap-2 px-3 py-2 bg-slate-700/30 text-slate-300 border border-slate-600/30 rounded-lg hover:bg-slate-700/50 transition text-xs font-medium"
                >
                  <FolderOpen className="w-3.5 h-3.5" />
                  List Files
                </button>
                <button
                  onClick={() => prefillCommand("del ")}
                  className="flex items-center justify-center gap-2 px-3 py-2 bg-orange-500/10 text-orange-400 border border-orange-500/20 rounded-lg hover:bg-orange-500/20 transition text-xs font-medium"
                >
                  <Trash2 className="w-3.5 h-3.5" />
                  Delete File...
                </button>
                <button
                  onClick={() => prefillCommand("corrupt ")}
                  className="flex items-center justify-center gap-2 px-3 py-2 bg-red-500/10 text-red-400 border border-red-500/20 rounded-lg hover:bg-red-500/20 transition text-xs font-medium"
                >
                  <FileWarning className="w-3.5 h-3.5" />
                  Corrupt File...
                </button>
              </div>

              <div className="flex items-center gap-2">
                <div className="relative flex-1">
                  <span className="absolute left-3 top-1/2 -translate-y-1/2 text-purple-500 font-mono text-lg">
                    &gt;
                  </span>
                  <input
                    ref={commandInputRef}
                    type="text"
                    value={command}
                    onChange={(e) => setCommand(e.target.value)}
                    onKeyDown={handleKeyPressCommand}
                    className="w-full pl-8 pr-3 py-2.5 rounded-xl bg-slate-950/80 border border-slate-700 text-sm text-slate-200 focus:outline-none focus:ring-2 focus:ring-purple-600/70 font-mono"
                    placeholder="Enter command (e.g. sysinfo, kill 1234)..."
                  />
                </div>
                <button
                  onClick={handleSendCommand}
                  className="inline-flex items-center justify-center gap-2 px-5 py-2.5 rounded-xl bg-purple-600 hover:bg-purple-500 text-white text-sm font-semibold transition shadow-md shadow-purple-900/30"
                >
                  <Send className="w-4 h-4" />
                  Send
                </button>
              </div>
            </div>

            {activeDesktop ? (
              <>
                {/* System info cards */}
                <div className="grid grid-cols-1 md:grid-cols-3 gap-3 md:gap-4">
                  <div className="bg-slate-900/80 border border-slate-700/80 rounded-2xl p-4 flex flex-col gap-2 relative overflow-hidden group">
                    <div className="absolute -right-4 -top-4 bg-blue-500/5 w-24 h-24 rounded-full group-hover:bg-blue-500/10 transition-colors" />
                    <div className="flex items-center justify-between relative z-10">
                      <span className="text-xs uppercase tracking-wide text-slate-500 font-bold">
                        System
                      </span>
                      <Server className="w-4 h-4 text-blue-400" />
                    </div>
                    <div className="mt-1 relative z-10">
                      <p className="text-sm font-semibold text-slate-200 truncate">
                        {activeDesktop.sysInfo?.hostname || "Unknown Host"}
                      </p>
                      <p className="text-xs text-slate-400 mt-1 font-mono">
                        {activeDesktop.sysInfo
                          ? `${activeDesktop.sysInfo.os} • ${activeDesktop.sysInfo.arch}`
                          : "Waiting for data..."}
                      </p>
                    </div>
                  </div>

                  <div className="bg-slate-900/80 border border-slate-700/80 rounded-2xl p-4 flex flex-col gap-2 relative overflow-hidden group">
                    <div className="absolute -right-4 -top-4 bg-green-500/5 w-24 h-24 rounded-full group-hover:bg-green-500/10 transition-colors" />
                    <div className="flex items-center justify-between relative z-10">
                      <span className="text-xs uppercase tracking-wide text-slate-500 font-bold">
                        Processing Power
                      </span>
                      <Cpu className="w-4 h-4 text-green-400" />
                    </div>
                    <div className="mt-1 relative z-10">
                      <div className="flex items-baseline gap-1">
                        <p className="text-2xl font-bold text-slate-200">
                          {activeDesktop.sysInfo?.cpuCores ?? "-"}
                        </p>
                        <span className="text-xs text-slate-500">Cores</span>
                      </div>
                    </div>
                  </div>

                  <div className="bg-slate-900/80 border border-slate-700/80 rounded-2xl p-4 flex flex-col gap-2 relative overflow-hidden group">
                    <div className="absolute -right-4 -top-4 bg-orange-500/5 w-24 h-24 rounded-full group-hover:bg-orange-500/10 transition-colors" />
                    <div className="flex items-center justify-between relative z-10">
                      <span className="text-xs uppercase tracking-wide text-slate-500 font-bold">
                        Memory Usage
                      </span>
                      <div className="flex items-center gap-1">
                        <HardDrive className="w-3.5 h-3.5 text-orange-400" />
                      </div>
                    </div>
                    <div className="mt-1 relative z-10">
                      <p className="text-sm font-semibold text-slate-200">
                        {activeDesktop.sysInfo
                          ? `${(
                              activeDesktop.sysInfo.totalMemory -
                              activeDesktop.sysInfo.availableMemory
                            ).toLocaleString()} / ${activeDesktop.sysInfo.totalMemory.toLocaleString()} MB`
                          : "-- / --"}
                      </p>
                      {activeDesktop.sysInfo && (
                        <div className="w-full bg-slate-800 h-1.5 mt-2 rounded-full overflow-hidden">
                          <div
                            className="bg-orange-400 h-full rounded-full transition-all duration-500"
                            style={{
                              width: `${
                                ((activeDesktop.sysInfo.totalMemory -
                                  activeDesktop.sysInfo.availableMemory) /
                                  activeDesktop.sysInfo.totalMemory) *
                                100
                              }%`,
                            }}
                          />
                        </div>
                      )}
                    </div>
                  </div>
                </div>

                {/* Main Grid */}
                <div className="flex-1 grid grid-cols-1 lg:grid-cols-2 gap-3 md:gap-4 min-h-0">
                  {/* Processes Panel */}
                  <div className="bg-slate-900/80 border border-slate-700/80 rounded-2xl p-3 md:p-4 flex flex-col min-h-0 shadow-lg">
                    <div className="flex items-center justify-between mb-3">
                      <span className="text-xs font-semibold uppercase tracking-wide text-slate-500 flex items-center gap-2">
                        <Activity className="w-4 h-4" />
                        Active Processes
                      </span>
                      <button
                        onClick={() => {
                          if (
                            activeDesktop.connected &&
                            activeDesktop.ws &&
                            activeDesktop.ws.readyState === WebSocket.OPEN
                          ) {
                            activeDesktop.ws.send(
                              JSON.stringify({ type: "processes" })
                            );
                          }
                        }}
                        className="inline-flex items-center gap-1.5 px-2.5 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 text-[11px] text-slate-300 border border-slate-600 transition"
                      >
                        <RefreshCw className="w-3 h-3" />
                        Refresh
                      </button>
                    </div>
                    <div className="flex-1 overflow-auto rounded-xl border border-slate-800 bg-slate-950/60 custom-scrollbar">
                      <table className="w-full text-[11px]">
                        <thead className="bg-slate-900/90 sticky top-0 z-10 backdrop-blur-sm">
                          <tr className="text-slate-400 font-medium">
                            <th className="text-left px-3 py-2.5 border-b border-slate-800">
                              PID
                            </th>
                            <th className="text-left px-3 py-2.5 border-b border-slate-800">
                              Name
                            </th>
                            <th className="text-right px-3 py-2.5 border-b border-slate-800">
                              Memory (KB)
                            </th>
                            <th className="px-3 py-2.5 border-b border-slate-800 w-10"></th>
                          </tr>
                        </thead>
                        <tbody>
                          {activeDesktop.processes.length === 0 ? (
                            <tr>
                              <td
                                colSpan={4}
                                className="px-3 py-8 text-center text-slate-500"
                              >
                                No process data available.
                              </td>
                            </tr>
                          ) : (
                            activeDesktop.processes.map((p) => (
                              <tr
                                key={p.pid}
                                className="border-b border-slate-800/50 hover:bg-slate-800/40 transition-colors group"
                              >
                                <td className="px-3 py-2 text-slate-400 font-mono">
                                  {p.pid}
                                </td>
                                <td className="px-3 py-2 text-slate-200 font-medium truncate max-w-[120px]">
                                  {p.name}
                                </td>
                                <td className="px-3 py-2 text-right text-slate-400 font-mono">
                                  {p.memory.toLocaleString()}
                                </td>
                                <td className="px-2 py-1 text-center">
                                  <button
                                    onClick={() => {
                                      if (
                                        confirm(
                                          `Kill process ${p.name} (${p.pid})?`
                                        )
                                      ) {
                                        sendCommandToDesktop(
                                          activeDesktop.id,
                                          `kill ${p.pid}`
                                        );
                                      }
                                    }}
                                    className="opacity-0 group-hover:opacity-100 text-red-400 hover:text-red-300 p-1 hover:bg-red-500/10 rounded transition"
                                    title="Kill Process"
                                  >
                                    <XCircle className="w-3.5 h-3.5" />
                                  </button>
                                </td>
                              </tr>
                            ))
                          )}
                        </tbody>
                      </table>
                    </div>
                  </div>

                  {/* Output Panel */}
                  <div className="bg-slate-900/80 border border-slate-700/80 rounded-2xl p-3 md:p-4 flex flex-col min-h-0 shadow-lg">
                    <div className="flex items-center justify-between mb-3">
                      <span className="text-xs font-semibold uppercase tracking-wide text-slate-500 flex items-center gap-2">
                        <Terminal className="w-4 h-4" />
                        Output Console
                      </span>
                      <button
                        onClick={() => {
                          if (!activeDesktop) return;
                          setDesktops((prev) =>
                            prev.map((d) =>
                              d.id === activeDesktop.id
                                ? { ...d, output: "" }
                                : d
                            )
                          );
                        }}
                        className="inline-flex items-center gap-1.5 px-2.5 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 text-[11px] text-slate-300 border border-slate-600 transition"
                      >
                        <Trash2 className="w-3 h-3" />
                        Clear
                      </button>
                    </div>
                    <div className="flex-1 overflow-auto rounded-xl border border-slate-800 bg-black/90 font-mono text-xs p-3 whitespace-pre-wrap custom-scrollbar shadow-inner text-slate-300">
                      {activeDesktop.output || (
                        <span className="text-slate-600 italic">
                          // Terminal ready. Waiting for input...
                        </span>
                      )}
                      <div ref={outputEndRef} />
                    </div>
                  </div>
                </div>

                {/* Logs */}
                <div className="bg-slate-900/80 border border-slate-700/80 rounded-2xl p-3 md:p-4 flex flex-col min-h-[120px] max-h-52 shadow-lg">
                  <div className="flex items-center justify-between mb-2">
                    <span className="text-xs font-semibold uppercase tracking-wide text-slate-500 flex items-center gap-2">
                      <ShieldAlert className="w-4 h-4" />
                      Session Logs
                    </span>
                  </div>
                  <div className="flex-1 overflow-auto rounded-xl border border-slate-800 bg-slate-950/40 text-[11px] custom-scrollbar">
                    {activeDesktop.logs.length === 0 ? (
                      <p className="px-3 py-3 text-slate-500 italic text-center">
                        No logs recorded yet.
                      </p>
                    ) : (
                      <ul className="divide-y divide-slate-800/50">
                        {activeDesktop.logs.map((log, idx) => {
                          const color =
                            log.type === "error"
                              ? "text-red-400 bg-red-500/5 border-l-2 border-red-500"
                              : log.type === "success"
                              ? "text-emerald-400 bg-emerald-500/5 border-l-2 border-emerald-500"
                              : log.type === "command"
                              ? "text-purple-400 bg-purple-500/5 border-l-2 border-purple-500"
                              : "text-slate-300 border-l-2 border-slate-600";

                          return (
                            <li
                              key={idx}
                              className={`px-3 py-1.5 flex gap-3 ${color}`}
                            >
                              <span className="text-[10px] text-slate-500 pt-0.5 min-w-[60px]">
                                {formatLogTime(log.timestamp)}
                              </span>
                              <span>{log.text}</span>
                            </li>
                          );
                        })}
                      </ul>
                    )}
                    <div ref={logsEndRef} />
                  </div>
                </div>
              </>
            ) : (
              <div className="flex-1 flex flex-col items-center justify-center text-center">
                <div className="p-4 rounded-full bg-slate-800/50 mb-4 border border-slate-700">
                  <RadioTower className="w-12 h-12 text-slate-600" />
                </div>
                <h3 className="text-lg font-medium text-slate-300">
                  No Target Selected
                </h3>
                <p className="text-sm text-slate-500 max-w-xs mt-2">
                  Select a desktop from the sidebar or add a new connection
                  target to begin administration.
                </p>
              </div>
            )}
          </main>
        </div>
      </div>
    </div>
  );
}
