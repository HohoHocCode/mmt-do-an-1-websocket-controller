import Header from "./components/Header";
import { useEffect, useMemo, useRef, useState } from "react";
import {
  Activity,
  Camera,
  Check,
  Cpu,
  Download,
  Film,
  Image as ImageIcon,
  Monitor,
  Plus,
  RadioTower,
  RefreshCw,
  Send,
  Trash2,
  Video,
  Wifi,
  XCircle,
} from "lucide-react";
import LoginPage from "./LoginPage";

// [ADDED] App name (để LoginPage / Header dùng thống nhất)
const APP_NAME = "REMOTE DESKTOP CONTROL"; // [ADDED]

// Commands (JSON over WebSocket):
//   {"cmd":"ping"}
//   {"cmd":"process_list"}
//   {"cmd":"process_kill","pid":123}
//   {"cmd":"process_start","path":"/path/to/app"}
//   {"cmd":"screen"}
//   {"cmd":"camera"}
//   {"cmd":"camera_video","duration":10}
//   {"cmd":"screen_stream","duration":5,"fps":5}

type LogType = "info" | "success" | "error" | "command";

interface Log {
  text: string;
  type: LogType;
  timestamp: Date;
}

interface Proc {
  pid: number;
  name: string;
  memory?: number;
}

type LastImageKind = "screen" | "camera" | "screen_stream";

interface LastImage {
  kind: LastImageKind;
  base64: string;
  ts: number;
  seq?: number;
}

interface LastVideo {
  base64: string;
  format: string;
  ts: number;
  url?: string; // blob URL
}

interface StreamState {
  running: boolean;
  fps: number;
  duration: number;
  lastSeq: number;
}

interface RemoteTarget {
  id: string;
  name: string;
  host: string;
  port: number;
  connected: boolean;
  connecting: boolean;
  ws?: WebSocket;

  logs: Log[];
  processes: Proc[];
  lastImage?: LastImage;
  lastVideo?: LastVideo;
  stream: StreamState;
}

const DEFAULT_PORT = 9002;
const createId = () => `${Date.now()}-${Math.random().toString(16).slice(2)}`;
const appendLog = (target: RemoteTarget, log: Log): RemoteTarget => ({
  ...target,
  logs: [...target.logs, log].slice(-LOG_LIMIT),
});

function formatTime(date: Date) {
  return date.toLocaleTimeString();
}

function b64ToBlob(b64: string, mime: string): Blob {
  const byteChars = atob(b64);
  const byteNumbers = new Array(byteChars.length);
  for (let i = 0; i < byteChars.length; i++) byteNumbers[i] = byteChars.charCodeAt(i);
  return new Blob([new Uint8Array(byteNumbers)], { type: mime });
}

function downloadLastImage(img: LastImage, host: string) {
  const blob = b64ToBlob(img.base64, "image/jpeg");
  const url = URL.createObjectURL(blob);

  const a = document.createElement("a");
  a.href = url;
  a.download = `${img.kind}_${host}_${new Date(img.ts)
    .toISOString()
    .replace(/[:.]/g, "-")}.jpg`;

  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

function downloadLastVideo(video: LastVideo, host: string) {
  if (!video.url) return;

  const a = document.createElement("a");
  a.href = video.url;
  a.download = `camera_video_${host}_${new Date(video.ts)
    .toISOString()
    .replace(/[:.]/g, "-")}.${video.format}`;

  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
}

function safeJsonParse(raw: string): unknown {
  try {
    return JSON.parse(raw);
  } catch {
    return null;
  }
}

// [PATCH] helper gửi WS an toàn (tránh crash khi socket vừa close)
function safeSend(ws: WebSocket | undefined, raw: string) {
  if (!ws) return false;
  if (ws.readyState !== WebSocket.OPEN) return false;
  try {
    ws.send(raw);
    return true;
  } catch {
    return false;
  }
}

// [PATCH] giới hạn log để tránh lag/memory leak
const LOG_LIMIT = 400;

export default function App() {
  const [targets, setTargets] = useState<RemoteTarget[]>([]);
  const [activeId, setActiveId] = useState<string | null>(null);

  const [newName, setNewName] = useState("");
  const [newHost, setNewHost] = useState("");
  const [newPort, setNewPort] = useState(String(DEFAULT_PORT));

  const [broadcastMode, setBroadcastMode] = useState(false);
  const [selectedForBroadcast, setSelectedForBroadcast] = useState<string[]>([]);

  const [customCmd, setCustomCmd] = useState('{"cmd":"ping"}');
  type TaskType = "process" | "stream" | "command";
  const [activeTask, setActiveTask] = useState<TaskType>("process");

  const [startPath, setStartPath] = useState("");
  const [videoDuration, setVideoDuration] = useState(10);
  const [streamDuration, setStreamDuration] = useState(5);
  const [streamFps, setStreamFps] = useState(5);

  const logsEndRef = useRef<HTMLDivElement | null>(null);
  const [uiLoggedIn, setUiLoggedIn] = useState(false);
  const [username, setUsername] = useState("");

  // [ADDED] password state cho LoginPage
  const [password, setPassword] = useState(""); // [ADDED]

  const active = useMemo(
    () => targets.find((t) => t.id === activeId) ?? targets[0] ?? null,
    [targets, activeId]
  );

  useEffect(() => {
    if (!active) return;

    // sync UI controls with per-target stream settings
    setStreamDuration(active.stream.duration);
    setStreamFps(active.stream.fps);
  }, [active?.id]);

  const canSend = !!active?.connected;

  useEffect(() => {
    if (!activeId && targets.length > 0) setActiveId(targets[0].id);
  }, [targets, activeId]);

  useEffect(() => {
    logsEndRef.current?.scrollIntoView({ behavior: "smooth", block: "end" });
  }, [targets, activeId]);

  // [PATCH] cleanup khi unmount: close WS + revoke blob URLs
  useEffect(() => {
    return () => {
      try {
        targets.forEach((t) => {
          try {
            if (t.ws && (t.ws.readyState === WebSocket.OPEN || t.ws.readyState === WebSocket.CONNECTING)) {
              t.ws.close();
            }
          } catch {}
          try {
            if (t.lastVideo?.url) URL.revokeObjectURL(t.lastVideo.url);
          } catch {}
        });
      } catch {}
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []); // cố tình [] để cleanup 1 lần

  const addLog = (id: string, text: string, type: LogType = "info") => {
    setTargets((prev) =>
      prev.map((t) =>
        t.id === id
          ? appendLog(t, { text, type, timestamp: new Date() })
          : t
      )
    );
  };

  const toggleBroadcastSelection = (id: string) => {
    setSelectedForBroadcast((prev) =>
      prev.includes(id) ? prev.filter((x) => x !== id) : [...prev, id]
    );
  };

  // [CHANGED] handleLogin thêm validate password (giữ logic cũ)
  const handleLogin = () => {
    const u = username.trim();
    const p = password.trim(); // [ADDED]
    if (!u) return;

    // [ADDED] validate password tối thiểu
    if (!p) return; // [ADDED]
    if (p.length < 4) return; // [ADDED]

    localStorage.setItem("rdc.lastUsername", u);
    localStorage.setItem("rdc.lastLoginAt", new Date().toISOString());
    localStorage.removeItem("rdc.lastLogoutAt");

    setUiLoggedIn(true);

    // [ADDED] khuyến nghị: không giữ password trong state sau khi login
    setPassword(""); // [ADDED]
  };

  const handleAddTarget = () => {
    const host = newHost.trim();
    if (!host) return;
    const name = newName.trim() || host;
    const port = Number.parseInt(newPort || String(DEFAULT_PORT), 10) || DEFAULT_PORT;

    const id = createId();
    const t: RemoteTarget = {
      id,
      name,
      host,
      port,
      connected: false,
      connecting: false,
      logs: [],
      processes: [],
      stream: { running: false, fps: 5, duration: 5, lastSeq: -1 },
    };

    setTargets((prev) => [...prev, t]);
    setNewName("");
    setNewHost("");
    setNewPort(String(DEFAULT_PORT));
    if (!activeId) setActiveId(id);
  };

  const disconnectTarget = (id: string) => {
    setTargets((prev) =>
      prev.map((t) => {
        if (t.id !== id) return t;
        if (t.ws && (t.ws.readyState === WebSocket.OPEN || t.ws.readyState === WebSocket.CONNECTING)) {
          try {
            t.ws.close();
          } catch {}
        }
        if (t.lastVideo?.url) URL.revokeObjectURL(t.lastVideo.url);
        return {
          ...t,
          connected: false,
          connecting: false,
          ws: undefined,
          lastVideo: t.lastVideo ? { ...t.lastVideo, url: undefined } : undefined,
        };
      })
    );
  };

  const connectTarget = (id: string) => {
    setTargets((prev) =>
      prev.map((t) => {
        if (t.id !== id) return t;
        if (t.ws && (t.ws.readyState === WebSocket.OPEN || t.ws.readyState === WebSocket.CONNECTING)) return t;

        const url = `ws://${t.host}:${t.port}/`;
        const ws = new WebSocket(url);

        // [PATCH] KHÔNG gọi addLog() ở đây (tránh setState lồng trong setState)
        const connectingLog: Log = { text: `Connecting to ${url}`, type: "info", timestamp: new Date() };

        ws.onopen = () => {
          addLog(id, "WebSocket connected", "success");
          setTargets((inner) => inner.map((x) => (x.id === id ? { ...x, connected: true, connecting: false } : x)));
          // ping an toàn
          safeSend(ws, JSON.stringify({ cmd: "ping" }));
        };

        ws.onerror = () => {
          addLog(id, "WebSocket error (network / server down?)", "error");
          // [PATCH] nếu lỗi sớm, thường sẽ onclose; nhưng nếu không, vẫn hạ connecting
          setTargets((inner) =>
            inner.map((x) => (x.id === id ? { ...x, connecting: false } : x))
          );
        };

        ws.onclose = () => {
          addLog(id, "Disconnected", "error");
          setTargets((inner) =>
            inner.map((x) => {
              if (x.id !== id) return x;
              if (x.lastVideo?.url) URL.revokeObjectURL(x.lastVideo.url);
              return {
                ...x,
                connected: false,
                connecting: false,
                ws: undefined,
                lastVideo: x.lastVideo ? { ...x.lastVideo, url: undefined } : undefined,
                stream: { ...x.stream, running: false },
              };
            })
          );
        };

        ws.onmessage = (evt) => {
          const parsed = safeJsonParse(String(evt.data));
          if (!parsed || typeof parsed !== "object") {
            addLog(id, `RAW: ${String(evt.data)}`, "info");
            return;
          }
          const data = parsed as Record<string, unknown>;

          // screen_stream frames
          if (data.cmd === "screen_stream" && typeof data.image_base64 === "string") {
            const seq = typeof data.seq === "number" ? data.seq : undefined;
            setTargets((inner) =>
              inner.map((x) =>
                x.id === id
                  ? {
                      ...x,
                      lastImage: { kind: "screen_stream", base64: data.image_base64 as string, ts: Date.now(), seq },
                      stream: { ...x.stream, running: true, lastSeq: typeof seq === "number" ? seq : x.stream.lastSeq },
                    }
                  : x
              )
            );
            return;
          }

          // single images (screen / camera)
          if (typeof data.image_base64 === "string") {
            const kind: LastImageKind = data.cmd === "camera" ? "camera" : "screen";

            setTargets((inner) =>
              inner.map((x) =>
                x.id === id
                  ? {
                      ...x,
                      lastImage: {
                        kind,
                        base64: data.image_base64 as string,
                        ts: Date.now(),
                      },
                      lastVideo: undefined, // 🔥 XOÁ VIDEO KHI CÓ IMAGE
                    }
                  : x
              )
            );

            addLog(id, `Received image (${kind})`, "success");
            return;
          }

          // camera video
          if (data.cmd === "camera_video" && typeof data.video_base64 === "string") {
            const format = typeof data.format === "string" ? data.format : "bin";
            const mime = format.toLowerCase() === "avi" ? "video/x-msvideo" : "application/octet-stream";
            const blob = b64ToBlob(data.video_base64 as string, mime);
            const urlObj = URL.createObjectURL(blob);

            setTargets((inner) =>
              inner.map((x) => {
                if (x.id !== id) return x;
                if (x.lastVideo?.url) URL.revokeObjectURL(x.lastVideo.url);
                return { ...x, lastVideo: { base64: data.video_base64 as string, format, ts: Date.now(), url: urlObj } };
              })
            );

            addLog(id, `Received camera video (${format})`, "success");
            return;
          }

          // process list
          if (data.status === "ok" && Array.isArray(data.data)) {
            const procs = (data.data as unknown[])
              .map((p) => {
                if (!p || typeof p !== "object") return null;
                const obj = p as Record<string, unknown>;
                const pid = typeof obj.pid === "number" ? obj.pid : Number(obj.pid);
                const name = typeof obj.name === "string" ? obj.name : String(obj.name ?? "");
                const memory = typeof obj.memory === "number" ? obj.memory : undefined;
                if (!Number.isFinite(pid)) return null;
                return { pid, name, memory } as Proc;
              })
              .filter(Boolean) as Proc[];

            setTargets((inner) => inner.map((x) => (x.id === id ? { ...x, processes: procs } : x)));
            addLog(id, `Loaded ${procs.length} processes`, "success");
            return;
          }

          // generic status
          if (typeof data.status === "string") {
            const status = data.status as string;
            const msg = typeof data.message === "string" ? data.message : JSON.stringify(data);
            addLog(id, `${status.toUpperCase()}: ${msg}`, status === "ok" ? "success" : "error");
            return;
          }

          addLog(id, `MSG: ${JSON.stringify(data)}`, "info");
        };

        return {
          ...t,
          ws,
          connecting: true,
          // [PATCH] append log ngay trong cùng lần setTargets, không gọi addLog lồng
          logs: [...t.logs, connectingLog].slice(-LOG_LIMIT),
        };
      })
    );
  };

  const sendJsonToTarget = (id: string, payload: unknown, label?: string) => {
    setTargets((prev) =>
      prev.map((t) => {
        if (t.id !== id) return t;
        if (!t.ws || t.ws.readyState !== WebSocket.OPEN) {
          const log: Log = { text: "Cannot send: not connected", type: "error", timestamp: new Date() };
          return appendLog(t, log);
        }
        const raw = JSON.stringify(payload);

        // [PATCH] send an toàn
        const ok = safeSend(t.ws, raw);
        if (!ok) {
          const log: Log = { text: "Send failed (socket not open)", type: "error", timestamp: new Date() };
          return appendLog(t, log);
        }

        const commandLog: Log = {
          text: label ? `$ ${label}  ${raw}` : `$ ${raw}`,
          type: "command",
          timestamp: new Date(),
        };

        return appendLog(t, commandLog);
      })
    );
  };

  const sendJson = (payload: unknown, label?: string) => {
    if (broadcastMode) {
      if (selectedForBroadcast.length === 0) return;
      selectedForBroadcast.forEach((id) =>
        sendJsonToTarget(id, payload, label ? `BROADCAST:${label}` : "BROADCAST")
      );
      return;
    }
    if (active) sendJsonToTarget(active.id, payload, label);
  };

  const actionPing = () => sendJson({ cmd: "ping" }, "ping");
  const actionListProcesses = () => sendJson({ cmd: "process_list" }, "process_list");
  const actionScreen = () => sendJson({ cmd: "screen" }, "screen");
  const actionCamera = () => sendJson({ cmd: "camera" }, "camera");
  const actionCameraVideo = () =>
    sendJson({ cmd: "camera_video", duration: Math.max(1, Math.min(30, videoDuration)) }, "camera_video");
  const actionScreenStream = () => {
    const dur = Math.max(1, Math.min(60, streamDuration));
    const fps = Math.max(1, Math.min(30, streamFps));
    sendJson({ cmd: "screen_stream", duration: dur, fps }, "screen_stream");
    if (!broadcastMode && active) {
      setTargets((prev) =>
        prev.map((t) =>
          t.id === active.id ? { ...t, stream: { ...t.stream, running: true, duration: dur, fps } } : t
        )
      );
    }
  };

  const actionKillProcess = (pid: number) => sendJson({ cmd: "process_kill", pid }, `process_kill:${pid}`);
  const actionStartProcess = () => {
    const path = startPath.trim();
    if (!path) return;
    sendJson({ cmd: "process_start", path }, "process_start");
    setStartPath("");
  };

  const actionSendCustomJson = () => {
    const parsed = safeJsonParse(customCmd);
    if (!parsed) {
      if (active) addLog(active.id, "Invalid JSON", "error");
      return;
    }
    sendJson(parsed, "custom");
  };

  const removeTarget = (id: string) => {
    const t = targets.find((x) => x.id === id);
    if (t?.ws && (t.ws.readyState === WebSocket.OPEN || t.ws.readyState === WebSocket.CONNECTING)) {
      try {
        t.ws.close();
      } catch {}
    }
    if (t?.lastVideo?.url) URL.revokeObjectURL(t.lastVideo.url);

    setTargets((prev) => prev.filter((x) => x.id !== id));
    setSelectedForBroadcast((prev) => prev.filter((x) => x !== id));
    if (activeId === id) setActiveId(null);
  };

  if (!uiLoggedIn) {
    return (
      <LoginPage
        appName={APP_NAME} // [CHANGED] (hoặc bạn giữ "Remote Control Center" cũng được)
        username={username}
        password={password} // [ADDED]
        onUsernameChange={setUsername}
        onPasswordChange={setPassword} // [ADDED]
        onSubmit={handleLogin}
      />
    );
  }

  return (
    <div className="min-h-screen bg-[#040A18] text-slate-100 p-4 md:p-6 relative overflow-hidden">
      <div className="pointer-events-none absolute inset-0">
        <div className="absolute -top-56 -left-40 h-[680px] w-[680px] rounded-full bg-blue-600/20 blur-3xl" />
        <div className="absolute -bottom-56 -right-48 h-[720px] w-[720px] rounded-full bg-cyan-500/20 blur-3xl" />
      </div>
      <div className="max-w-7xl mx-auto flex flex-col gap-4 h-[calc(100vh-2rem)]">
        <Header title="Remote Control Center" connectedCount={targets.filter((t) => t.connected).length} />

        <div className="flex-1 flex flex-col md:flex-row gap-4 min-h-0">
          {/* Sidebar */}
          <aside className="w-full md:w-80 flex flex-col gap-4 min-h-0">
            {/* Add */}
            <div className="bg-secondary/50 border border-border rounded-2xl p-4 space-y-3 shadow-lg">
              <div className="flex items-center gap-2 text-sm font-medium">
                <Plus className="w-4 h-4 text-primary" /> Add Target
              </div>
              <div className="space-y-2">
                <input
                  value={newName}
                  onChange={(e) => setNewName(e.target.value)}
                  placeholder="Display name (optional)"
                  className="w-full px-3 py-2 rounded-lg bg-slate-900/60 border border-border text-sm focus:outline-none focus:ring-2 focus:ring-primary/60"
                />
                <input
                  value={newHost}
                  onChange={(e) => setNewHost(e.target.value)}
                  placeholder="Host / IP (e.g. 127.0.0.1)"
                  className="w-full px-3 py-2 rounded-lg bg-slate-900/60 border border-border text-sm focus:outline-none focus:ring-2 focus:ring-primary/60"
                />
                <input
                  type="number"
                  value={newPort}
                  onChange={(e) => setNewPort(e.target.value)}
                  placeholder="Port (9002)"
                  className="w-full px-3 py-2 rounded-lg bg-slate-900/60 border border-border text-sm focus:outline-none focus:ring-2 focus:ring-primary/60"
                />
              </div>
              <button
                onClick={handleAddTarget}
                className="w-full inline-flex items-center justify-center gap-2 px-3 py-2 rounded-lg bg-primary hover:opacity-90 text-sm font-medium transition shadow-md"
              >
                <Plus className="w-4 h-4" /> Add
              </button>
            </div>

            {/* Broadcast */}
            <div className="bg-secondary/50 border border-border rounded-2xl p-4 space-y-3 shadow-lg">
              <div className="flex items-center justify-between gap-2">
                <div className="flex items-center gap-2">
                  <RadioTower className="w-4 h-4 text-primary" />
                  <span className="text-sm font-medium">Broadcast Mode</span>
                </div>
                <button
                  onClick={() => {
                    setBroadcastMode((v) => !v);
                    setSelectedForBroadcast([]);
                  }}
                  className={`relative inline-flex h-6 w-11 items-center rounded-full transition ${
                    broadcastMode ? "bg-primary" : "bg-slate-700"
                  }`}
                >
                  <span
                    className={`inline-block h-4 w-4 transform rounded-full bg-white transition-transform ${
                      broadcastMode ? "translate-x-6" : "translate-x-1"
                    }`}
                  />
                </button>
              </div>
              {broadcastMode && (
                <p className="text-xs text-muted-foreground">
                  Select targets below, then actions will be sent to all selected.
                </p>
              )}
            </div>

            {/* Targets */}
            <div className="flex-1 bg-secondary/50 border border-border rounded-2xl p-3 flex flex-col min-h-0 shadow-lg">
              <div className="flex items-center justify-between mb-2 px-1">
                <span className="text-xs font-semibold uppercase tracking-wide text-muted-foreground">Targets</span>
              </div>

              <div className="flex-1 overflow-auto space-y-2 pr-1">
                {targets.length === 0 && (
                  <p className="text-xs text-muted-foreground px-1 text-center py-6">No targets added.</p>
                )}

                {targets.map((t) => {
                  const isActive = t.id === active?.id;
                  const inBroadcast = selectedForBroadcast.includes(t.id);

                  return (
                    <div
                      key={t.id}
                      className={`rounded-xl border px-3 py-3 text-xs cursor-pointer transition-all ${
                        isActive
                          ? "bg-slate-900/60 border-primary/60"
                          : "bg-slate-950/20 border-border hover:border-slate-500/60"
                      }`}
                      onClick={() => setActiveId(t.id)}
                    >
                      <div className="flex items-start justify-between gap-2">
                        <div className="flex-1 min-w-0">
                          <div className="flex items-center gap-2">
                            <span className="font-bold truncate text-sm">{t.name}</span>
                            <span
                              className={`h-2 w-2 rounded-full ring-2 ring-slate-900 ${
                                t.connected ? "bg-emerald-400" : t.connecting ? "bg-amber-400" : "bg-slate-600"
                              }`}
                            />
                          </div>
                          <p className="text-[11px] text-muted-foreground truncate mt-1">
                            {t.host}:{t.port}
                          </p>
                        </div>

                        <div className="flex items-center gap-1.5">
                          {broadcastMode && (
                            <button
                              onClick={(e) => {
                                e.stopPropagation();
                                toggleBroadcastSelection(t.id);
                              }}
                              className={`h-6 w-6 flex items-center justify-center rounded-lg border transition ${
                                inBroadcast
                                  ? "bg-primary border-primary text-white"
                                  : "bg-slate-900/60 border-border text-transparent"
                              }`}
                              title="Select for broadcast"
                            >
                              <Check className="w-4 h-4" />
                            </button>
                          )}

                          {t.connected ? (
                            <button
                              onClick={(e) => {
                                e.stopPropagation();
                                disconnectTarget(t.id);
                              }}
                              className="h-6 w-6 flex items-center justify-center rounded-lg border border-border bg-slate-900/60 hover:bg-slate-800 transition"
                              title="Disconnect"
                            >
                              <XCircle className="w-4 h-4 text-rose-300" />
                            </button>
                          ) : (
                            <button
                              onClick={(e) => {
                                e.stopPropagation();
                                connectTarget(t.id);
                              }}
                              className="h-6 w-6 flex items-center justify-center rounded-lg border border-border bg-slate-900/60 hover:bg-slate-800 transition"
                              title="Connect"
                            >
                              <Wifi className="w-4 h-4 text-emerald-300" />
                            </button>
                          )}

                          <button
                            onClick={(e) => {
                              e.stopPropagation();
                              removeTarget(t.id);
                            }}
                            className="h-6 w-6 flex items-center justify-center rounded-lg border border-border bg-slate-900/60 hover:bg-slate-800 transition"
                            title="Remove"
                          >
                            <Trash2 className="w-4 h-4 text-muted-foreground" />
                          </button>
                        </div>
                      </div>
                    </div>
                  );
                })}
              </div>
            </div>
          </aside>

          {/* Main */}
          <main className="flex-1 flex flex-col gap-4 min-h-0">
            {!active ? (
              <div className="flex-1 bg-secondary/30 border border-border rounded-2xl p-6 flex items-center justify-center text-muted-foreground">
                Add a target to start.
              </div>
            ) : (
              <>
                {/* Actions */}
                <div className="bg-secondary/50 border border-border rounded-2xl p-4 shadow-lg">
                  {/* Task Menu */}
                  <div className="flex items-center gap-2 mb-4">
                    <button
                      onClick={() => setActiveTask("process")}
                      className={`px-3 py-1.5 rounded-lg text-xs border transition ${
                        activeTask === "process"
                          ? "bg-primary/20 border-primary/40 text-primary"
                          : "bg-slate-900/60 border-border text-muted-foreground hover:bg-slate-800"
                      }`}
                    >
                      Process
                    </button>

                    <button
                      onClick={() => setActiveTask("stream")}
                      className={`px-3 py-1.5 rounded-lg text-xs border transition ${
                        activeTask === "stream"
                          ? "bg-primary/20 border-primary/40 text-primary"
                          : "bg-slate-900/60 border-border text-muted-foreground hover:bg-slate-800"
                      }`}
                    >
                      Stream
                    </button>

                    <button
                      onClick={() => setActiveTask("command")}
                      className={`px-3 py-1.5 rounded-lg text-xs border transition ${
                        activeTask === "command"
                          ? "bg-primary/20 border-primary/40 text-primary"
                          : "bg-slate-900/60 border-border text-muted-foreground hover:bg-slate-800"
                      }`}
                    >
                      Command
                    </button>
                  </div>

                  <div className="flex flex-col lg:flex-row gap-4 lg:items-end lg:justify-between">
                    <div className="min-w-0">
                      <div className="flex items-center gap-2">
                        <span className="text-sm font-semibold truncate">{active.name}</span>
                        <span className="text-xs text-muted-foreground font-mono truncate">
                          ({active.host}:{active.port})
                        </span>
                      </div>
                      <p className="text-xs text-muted-foreground mt-1">
                        {active.connected ? "Connected" : active.connecting ? "Connecting" : "Disconnected"}
                        {broadcastMode && (
                          <span className="ml-2 inline-flex items-center gap-1 text-primary">
                            <RadioTower className="w-3 h-3" /> broadcast
                          </span>
                        )}
                      </p>
                    </div>

                    <div className="flex flex-wrap gap-2">
                      {/* STREAM TASK */}
                      {activeTask === "stream" && (
                        <>
                          <button
                            onClick={actionScreen}
                            disabled={!canSend}
                            className={`inline-flex items-center gap-2 px-3 py-2 rounded-lg
                                      bg-slate-900/60 border border-border transition text-sm
                                      ${!canSend ? "opacity-50 cursor-not-allowed" : "hover:bg-slate-800"}`}
                          >
                            <Monitor className="w-4 h-4" /> Screen
                          </button>

                          <button
                            onClick={actionCamera}
                            disabled={!canSend}
                            className={`inline-flex items-center gap-2 px-3 py-2 rounded-lg
                                      bg-slate-900/60 border border-border transition text-sm
                                      ${!canSend ? "opacity-50 cursor-not-allowed" : "hover:bg-slate-800"}`}
                          >
                            <Camera className="w-4 h-4" /> Camera
                          </button>

                          {/* RECORD VIDEO */}
                          <div className="flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border">
                            <Film className="w-4 h-4" />
                            <input
                              type="number"
                              min={1}
                              max={30}
                              value={videoDuration}
                              onChange={(e) => setVideoDuration(Number(e.target.value))}
                              className="w-16 bg-transparent text-sm focus:outline-none"
                            />
                            <button
                              onClick={actionCameraVideo}
                              disabled={!canSend}
                              className={`inline-flex items-center gap-2 px-2 py-1 rounded-md
                                        bg-primary/20 transition text-sm
                                        ${!canSend ? "opacity-50 cursor-not-allowed" : "hover:bg-primary/30"}`}
                            >
                              <Video className="w-4 h-4" /> Record
                            </button>
                          </div>

                          {/* SCREEN STREAM */}
                          <div className="flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border">
                            <ImageIcon className="w-4 h-4" />
                            <span className="text-xs text-muted-foreground">dur</span>
                            <input
                              type="number"
                              min={1}
                              max={60}
                              value={streamDuration}
                              onChange={(e) => setStreamDuration(Number(e.target.value))}
                              className="w-14 bg-transparent text-sm focus:outline-none"
                            />
                            <span className="text-xs text-muted-foreground">fps</span>
                            <input
                              type="number"
                              min={1}
                              max={30}
                              value={streamFps}
                              onChange={(e) => setStreamFps(Number(e.target.value))}
                              className="w-14 bg-transparent text-sm focus:outline-none"
                            />
                            <button
                              onClick={actionScreenStream}
                              disabled={!canSend}
                              className={`inline-flex items-center gap-2 px-2 py-1 rounded-md
                                        bg-primary/20 transition text-sm
                                        ${!canSend ? "opacity-50 cursor-not-allowed" : "hover:bg-primary/30"}`}
                            >
                              <Send className="w-4 h-4" /> Stream
                            </button>
                          </div>
                        </>
                      )}

                      {/* PROCESS TASK */}
                      {activeTask === "process" && (
                        <>
                          <button
                            onClick={actionListProcesses}
                            className="inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border hover:bg-slate-800 transition text-sm"
                          >
                            <RefreshCw className="w-4 h-4" /> Processes
                          </button>
                        </>
                      )}

                      {/* COMMAND TASK */}
                      {activeTask === "command" && (
                        <>
                          <button
                            onClick={actionPing}
                            className="inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border hover:bg-slate-800 transition text-sm"
                          >
                            <Activity className="w-4 h-4" /> Ping
                          </button>
                        </>
                      )}
                    </div>
                  </div>

                  {/* Task-specific inputs */}
                  <div className="mt-3 flex flex-col md:flex-row gap-2 md:items-center">
                    {activeTask === "process" && (
                      <div className="flex-1 flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border">
                        <Cpu className="w-4 h-4 text-muted-foreground" />
                        <input
                          value={startPath}
                          onChange={(e) => setStartPath(e.target.value)}
                          placeholder='Process path, e.g. "C:\\Windows\\System32\\notepad.exe"'
                          className="flex-1 bg-transparent text-sm focus:outline-none"
                        />
                        <button
                          onClick={actionStartProcess}
                          className="inline-flex items-center gap-2 px-2 py-1 rounded-md bg-primary/20 hover:bg-primary/30 transition text-sm"
                        >
                          <Send className="w-4 h-4" /> Start
                        </button>
                      </div>
                    )}

                    {activeTask === "command" && (
                      <div className="flex-1 flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border">
                        <span className="text-xs text-muted-foreground">Custom JSON</span>
                        <input
                          value={customCmd}
                          onChange={(e) => setCustomCmd(e.target.value)}
                          className="flex-1 bg-transparent text-sm font-mono focus:outline-none"
                          placeholder='{"cmd":"ping"}'
                        />
                        <button
                          onClick={actionSendCustomJson}
                          className="inline-flex items-center gap-2 px-2 py-1 rounded-md bg-primary/20 hover:bg-primary/30 transition text-sm"
                        >
                          <Send className="w-4 h-4" /> Send
                        </button>
                      </div>
                    )}
                  </div>
                </div>

                <div className="grid grid-cols-1 lg:grid-cols-3 gap-4 flex-1 min-h-0">
                  {/* LEFT PANEL (2/3) */}
                  <div className="bg-secondary/50 border border-border rounded-2xl p-4 shadow-lg flex flex-col min-h-0 lg:col-span-2">
                    {/* ===== STREAM → MEDIA ===== */}
                    {activeTask === "stream" && (
                      <>
                        <div className="flex items-center gap-3 mb-3">
                          <span className="text-xs font-semibold uppercase tracking-wide text-muted-foreground flex items-center gap-2">
                            <Monitor className="w-4 h-4" /> Media
                          </span>

                          {/* DOWNLOAD IMAGE */}
                          {active.lastImage && !active.stream.running && (
                            <button
                              onClick={() => downloadLastImage(active.lastImage!, active.host)}
                              className="inline-flex items-center gap-1 text-xs text-primary hover:opacity-80"
                            >
                              <Download className="w-4 h-4" /> Image
                            </button>
                          )}

                          {/* DOWNLOAD VIDEO */}
                          {active.lastVideo && active.lastVideo.url && (
                            <button
                              onClick={() => downloadLastVideo(active.lastVideo!, active.host)}
                              className="inline-flex items-center gap-1 text-xs text-primary hover:opacity-80"
                            >
                              <Download className="w-4 h-4" /> Video
                            </button>
                          )}

                          <span className="text-xs text-muted-foreground ml-auto">
                            {active.lastImage ? active.lastImage.kind : active.lastVideo ? "camera_video" : "No media"}
                          </span>
                        </div>

                        <div className="flex-1 rounded-xl border border-border bg-slate-950/30 flex items-center justify-center overflow-hidden">
                          {active.lastImage ? (
                            <img
                              src={`data:image/jpeg;base64,${active.lastImage.base64}`}
                              className="w-full h-full object-contain"
                            />
                          ) : active.lastVideo?.url ? (
                            <video src={active.lastVideo.url} controls className="w-full h-full object-contain" />
                          ) : (
                            <div className="text-sm text-muted-foreground text-center">Use Screen / Camera / Stream</div>
                          )}
                        </div>
                      </>
                    )}

                    {/* ===== PROCESS → PROCESS LIST ===== */}
                    {activeTask === "process" && (
                      <>
                        <div className="flex items-center justify-between mb-3">
                          <span className="text-xs font-semibold uppercase tracking-wide text-muted-foreground">
                            Process List
                          </span>

                          <button
                            onClick={actionListProcesses}
                            className="inline-flex items-center gap-2 px-2 py-1 rounded-lg bg-slate-900/60 border border-border text-xs"
                          >
                            <RefreshCw className="w-3 h-3" /> Refresh
                          </button>
                        </div>

                        <div className="flex-1 overflow-auto rounded-xl border border-border bg-slate-950/30">
                          <table className="w-full text-xs">
                            <thead className="sticky top-0 bg-slate-900/70">
                              <tr className="text-muted-foreground">
                                <th className="px-3 py-2 text-left">PID</th>
                                <th className="px-3 py-2 text-left">Name</th>
                                <th className="px-3 py-2 text-right">Memory</th>
                                <th className="px-3 py-2 text-right">Kill</th>
                              </tr>
                            </thead>
                            <tbody>
                              {active.processes.map((p) => (
                                <tr key={p.pid} className="border-b border-border">
                                  <td className="px-3 py-2 font-mono">{p.pid}</td>
                                  <td className="px-3 py-2 truncate">{p.name}</td>
                                  <td className="px-3 py-2 text-right">{p.memory ?? "-"}</td>
                                  <td className="px-3 py-2 text-right">
                                    <button
                                      onClick={() => actionKillProcess(p.pid)}
                                      className="h-6 w-6 rounded-lg border border-border bg-slate-900/60"
                                    >
                                      <XCircle className="w-4 h-4 text-rose-300" />
                                    </button>
                                  </td>
                                </tr>
                              ))}
                            </tbody>
                          </table>
                        </div>
                      </>
                    )}
                  </div>

                  {/* RIGHT PANEL – LOGS (1/3, LUÔN HIỂN THỊ) */}
                  <div className="bg-secondary/50 border border-border rounded-2xl p-4 shadow-lg flex flex-col min-h-0">
                    <div className="flex items-center justify-between mb-3">
                      <span className="text-xs font-semibold uppercase tracking-wide text-muted-foreground">Logs</span>

                      <button
                        onClick={() =>
                          setTargets((prev) => prev.map((t) => (t.id === active.id ? { ...t, logs: [] } : t)))
                        }
                        className="inline-flex items-center gap-2 px-2.5 py-1.5
                      rounded-lg bg-slate-900/60 hover:bg-slate-800
                      text-[11px] border border-border transition"
                      >
                        <Trash2 className="w-3 h-3" />
                        Clear
                      </button>
                    </div>

                    <div className="flex-1 overflow-auto rounded-xl border border-border bg-slate-950/30 p-3">
                      {active.logs.map((l, i) => (
                        <div key={i} className="text-xs">
                          <span className="text-muted-foreground font-mono">[{formatTime(l.timestamp)}]</span>{" "}
                          <span
                            className={
                              l.type === "error"
                                ? "text-rose-400 font-semibold"
                                : l.type === "success"
                                ? "text-emerald-400 font-semibold"
                                : l.type === "command"
                                ? "text-sky-400"
                                : "text-slate-300"
                            }
                          >
                            {l.text}
                          </span>
                        </div>
                      ))}
                      <div ref={logsEndRef} />
                    </div>
                  </div>
                </div>
              </>
            )}
          </main>
        </div>
      </div>
    </div>
  );
}
