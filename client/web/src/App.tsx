import Header from "./components/Header";
import { useEffect, useMemo, useRef, useState } from "react";
import {
  Activity,
  AlertCircle,
  Camera,
  Check,
  Cpu,
  Download,
  Film,
  Image as ImageIcon,
  Loader2,
  Monitor,
  Plus,
  RadioTower,
  RefreshCw,
  RotateCcw,
  Send,
  Trash2,
  Video,
  Wifi,
  XCircle,
} from "lucide-react";
import LoginPage from "./LoginPage";
import {
  LastImage,
  LastVideo,
  LogEntry,
  LogType,
  ProcessInfo,
  RemoteTarget,
  TaskKind,
} from "./types";

const APP_NAME = "REMOTE DESKTOP CONTROL";
const DEFAULT_PORT = 9002;
const LOG_LIMIT = 400;
const CANCEL_DELAY_MS = 380;

const createId = () => `${Date.now()}-${Math.random().toString(16).slice(2)}`;

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
  a.download = `${img.kind}_${host}_${new Date(img.ts).toISOString().replace(/[:.]/g, "-")}.jpg`;

  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

function downloadLastVideo(video: LastVideo, host: string) {
  if (!video.url) return;

  const a = document.createElement("a");
  a.href = video.url;
  a.download = `camera_video_${host}_${new Date(video.ts).toISOString().replace(/[:.]/g, "-")}.${video.format}`;

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

interface Toast {
  id: string;
  message: string;
  tone: "info" | "error" | "success";
}

type TaskPanel = "process" | "stream" | "command";
type ConnectionBadgeStatus = "Connected" | "Connecting" | "Disconnected" | "No target";

function buildInitialTarget(name: string, host: string, port: number): RemoteTarget {
  return {
    id: createId(),
    name,
    host,
    port,
    connected: false,
    connecting: false,
    logs: [],
    processes: [],
    stream: { running: false, fps: 5, duration: 5, lastSeq: -1 },
    task: { type: "idle", status: "idle" },
    ui: { loadingMedia: false, loadingProcesses: false },
  };
}

function StatusBadge({ status, targetLabel }: { status: ConnectionBadgeStatus; targetLabel: string }) {
  const color =
    status === "Connected"
      ? "bg-emerald-500/80 text-emerald-100"
      : status === "Connecting"
      ? "bg-amber-400/70 text-amber-950"
      : status === "Disconnected"
      ? "bg-rose-400/70 text-rose-950"
      : "bg-slate-500/70 text-slate-950";

  return (
    <div className="flex items-center gap-2">
      <span className={`inline-flex items-center gap-2 rounded-full px-3 py-1 text-xs font-semibold ${color}`}>
        <span className="h-2 w-2 rounded-full bg-white/80 shadow-sm" />
        {status}
      </span>
      <span className="text-xs text-slate-300 font-mono truncate">{targetLabel}</span>
    </div>
  );
}

function ToastStack({ toasts, onDismiss }: { toasts: Toast[]; onDismiss: (id: string) => void }) {
  return (
    <div className="fixed bottom-4 right-4 space-y-2 z-50">
      {toasts.map((toast) => (
        <div
          key={toast.id}
          className={`min-w-[260px] rounded-xl border px-4 py-3 shadow-lg backdrop-blur bg-slate-900/80 flex items-start gap-3 text-sm
            ${toast.tone === "error" ? "border-rose-400/60 text-rose-100" : toast.tone === "success" ? "border-emerald-400/60 text-emerald-100" : "border-slate-600/60 text-slate-100"}
          `}
        >
          <AlertCircle className="w-4 h-4 mt-0.5 shrink-0" />
          <div className="flex-1">{toast.message}</div>
          <button
            onClick={() => onDismiss(toast.id)}
            className="text-xs text-slate-400 hover:text-white transition"
            aria-label="Dismiss notification"
          >
            ✕
          </button>
        </div>
      ))}
    </div>
  );
}

export default function App() {
  const [targets, setTargets] = useState<RemoteTarget[]>([]);
  const [activeId, setActiveId] = useState<string | null>(null);
  const [panel, setPanel] = useState<TaskPanel>("stream");

  const [newName, setNewName] = useState("");
  const [newHost, setNewHost] = useState("");
  const [newPort, setNewPort] = useState(String(DEFAULT_PORT));

  const [broadcastMode, setBroadcastMode] = useState(false);
  const [selectedForBroadcast, setSelectedForBroadcast] = useState<string[]>([]);

  const [customCmd, setCustomCmd] = useState('{"cmd":"ping"}');
  const [startPath, setStartPath] = useState("");
  const [videoDuration, setVideoDuration] = useState(10);
  const [streamDuration, setStreamDuration] = useState(5);
  const [streamFps, setStreamFps] = useState(5);

  const [uiLoggedIn, setUiLoggedIn] = useState(false);
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");

  const [toasts, setToasts] = useState<Toast[]>([]);

  const logsEndRef = useRef<HTMLDivElement | null>(null);
  const taskControllersRef = useRef<Record<string, AbortController>>({});
  const cancelTimersRef = useRef<Record<string, number>>({});
  const streamTimersRef = useRef<Record<string, number>>({});
  const autoCompleteTimersRef = useRef<Record<string, number>>({});

  const active = useMemo(() => targets.find((t) => t.id === activeId) ?? targets[0] ?? null, [targets, activeId]);
  const canSend = useMemo(() => {
    if (broadcastMode) {
      return selectedForBroadcast.some((id) => {
        const target = targets.find((t) => t.id === id);
        return target?.connected;
      });
    }
    return !!active?.connected;
  }, [active?.connected, broadcastMode, selectedForBroadcast, targets]);

  useEffect(() => {
    if (!activeId && targets.length > 0) setActiveId(targets[0].id);
  }, [targets, activeId]);

  useEffect(() => {
    logsEndRef.current?.scrollIntoView({ behavior: "smooth", block: "end" });
  }, [targets, activeId]);

  useEffect(() => {
    return () => {
      Object.values(taskControllersRef.current).forEach((controller) => controller.abort());
      Object.values(cancelTimersRef.current).forEach((timer) => window.clearTimeout(timer));
      Object.values(streamTimersRef.current).forEach((timer) => window.clearTimeout(timer));
      Object.values(autoCompleteTimersRef.current).forEach((timer) => window.clearTimeout(timer));

      targets.forEach((t) => {
        if (t.ws && (t.ws.readyState === WebSocket.OPEN || t.ws.readyState === WebSocket.CONNECTING)) {
          try {
            t.ws.close();
          } catch {}
        }
        if (t.lastVideo?.url) URL.revokeObjectURL(t.lastVideo.url);
      });
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (!active) return;
    setStreamDuration(active.stream.duration);
    setStreamFps(active.stream.fps);
  }, [active?.id]);

  const pushToast = (message: string, tone: Toast["tone"] = "info") => {
    const id = createId();
    setToasts((prev) => [...prev, { id, message, tone }]);
    window.setTimeout(() => {
      setToasts((prev) => prev.filter((t) => t.id !== id));
    }, 3200);
  };

  const addLog = (id: string, text: string, type: LogType = "info") => {
    setTargets((prev) =>
      prev.map((t) =>
        t.id === id
          ? { ...t, logs: [...t.logs, { text, type, timestamp: new Date() }].slice(-LOG_LIMIT) }
          : t
      )
    );
  };

  const updateTargetById = (id: string, updater: (target: RemoteTarget) => RemoteTarget) => {
    setTargets((prev) => prev.map((t) => (t.id === id ? updater(t) : t)));
  };

  const clearTaskForTarget = (id: string) => {
    const controller = taskControllersRef.current[id];
    if (controller) controller.abort();
    delete taskControllersRef.current[id];
    if (cancelTimersRef.current[id]) window.clearTimeout(cancelTimersRef.current[id]);
    if (autoCompleteTimersRef.current[id]) {
      window.clearTimeout(autoCompleteTimersRef.current[id]);
      delete autoCompleteTimersRef.current[id];
    }
    updateTargetById(id, (t) => ({ ...t, task: { type: "idle", status: "idle" } }));
  };

  const cancelTaskForTarget = (id: string, reason?: string) => {
    const controller = taskControllersRef.current[id];
    if (controller) controller.abort();
    delete taskControllersRef.current[id];
    if (streamTimersRef.current[id]) {
      window.clearTimeout(streamTimersRef.current[id]);
      delete streamTimersRef.current[id];
    }
    if (autoCompleteTimersRef.current[id]) {
      window.clearTimeout(autoCompleteTimersRef.current[id]);
      delete autoCompleteTimersRef.current[id];
    }
    if (reason) addLog(id, reason, "info");
    if (cancelTimersRef.current[id]) window.clearTimeout(cancelTimersRef.current[id]);
    updateTargetById(id, (t) => ({ ...t, task: { ...t.task, status: "cancelling" } }));
    cancelTimersRef.current[id] = window.setTimeout(() => {
      updateTargetById(id, (t) => ({ ...t, task: { type: "idle", status: "idle" } }));
      delete cancelTimersRef.current[id];
    }, CANCEL_DELAY_MS);
  };

  const startTaskForTarget = (
    id: string,
    type: TaskKind,
    runner: (signal: AbortSignal) => boolean,
    label?: string,
    autoCompleteMs: number | null = 8000
  ) => {
    const controller = new AbortController();

    const begin = () => {
      taskControllersRef.current[id] = controller;
      updateTargetById(id, (t) => ({ ...t, task: { type, status: "running", label } }));
      const ok = runner(controller.signal);
      if (!ok) {
        clearTaskForTarget(id);
        return;
      }

      if (autoCompleteMs !== null) {
        if (autoCompleteTimersRef.current[id]) {
          window.clearTimeout(autoCompleteTimersRef.current[id]);
        }
        const autoTimer = window.setTimeout(() => {
          if (controller.signal.aborted) return;
          updateTargetById(id, (t) =>
            t.task.type === type && t.task.status === "running"
              ? { ...t, task: { type: "idle", status: "idle" } }
              : t
          );
          delete autoCompleteTimersRef.current[id];
        }, autoCompleteMs);
        autoCompleteTimersRef.current[id] = autoTimer;
        controller.signal.addEventListener("abort", () => {
          window.clearTimeout(autoCompleteTimersRef.current[id]);
          delete autoCompleteTimersRef.current[id];
        });
      }
    };

    const currentStatus = targets.find((t) => t.id === id)?.task.status;
    if (currentStatus === "running") {
      cancelTaskForTarget(id, "Cancelling previous task…");
      window.setTimeout(() => {
        if (!controller.signal.aborted) begin();
      }, CANCEL_DELAY_MS + 40);
      return;
    }

    begin();
  };

  const sendJsonToTarget = (id: string, payload: unknown, label?: string) => {
    let sent = false;
    setTargets((prev) =>
      prev.map((t) => {
        if (t.id !== id) return t;
        if (!t.ws || t.ws.readyState !== WebSocket.OPEN) {
          const logEntry: LogEntry = { text: "Cannot send: not connected", type: "error", timestamp: new Date() };
          return {
            ...t,
            logs: [...t.logs, logEntry].slice(-LOG_LIMIT),
          };
        }

        const raw = JSON.stringify(payload);
        const ok = safeSend(t.ws, raw);
        sent = ok;

        const logEntry: LogEntry = {
          text: label ? `$ ${label}  ${raw}` : `$ ${raw}`,
          type: ok ? "command" : "error",
          timestamp: new Date(),
        };

        return {
          ...t,
          logs: [...t.logs, logEntry].slice(-LOG_LIMIT),
        };
      })
    );
    if (!sent) pushToast("Send failed (socket not open)", "error");
    return sent;
  };

  const toggleBroadcastSelection = (id: string) => {
    setSelectedForBroadcast((prev) => (prev.includes(id) ? prev.filter((x) => x !== id) : [...prev, id]));
  };

  const getTargetIdsForAction = () => {
    if (broadcastMode) return selectedForBroadcast;
    if (active) return [active.id];
    return [];
  };

  const handleLogin = () => {
    const u = username.trim();
    const p = password.trim();
    if (!u || !p || p.length < 4) {
      pushToast("Please provide username and a password (min 4 chars)", "error");
      return;
    }

    localStorage.setItem("rdc.lastUsername", u);
    localStorage.setItem("rdc.lastLoginAt", new Date().toISOString());
    localStorage.removeItem("rdc.lastLogoutAt");

    setUiLoggedIn(true);
    setPassword("");
  };

  const handleAddTarget = () => {
    const host = newHost.trim();
    if (!host) {
      pushToast("Host/IP is required", "error");
      return;
    }
    const name = newName.trim() || host;
    const port = Number.parseInt(newPort || String(DEFAULT_PORT), 10) || DEFAULT_PORT;

    const t = buildInitialTarget(name, host, port);
    setTargets((prev) => [...prev, t]);
    setNewName("");
    setNewHost("");
    setNewPort(String(DEFAULT_PORT));
    if (!activeId) setActiveId(t.id);
  };

  const disconnectTarget = (id: string) => {
    const target = targets.find((t) => t.id === id);
    if (target?.ws && (target.ws.readyState === WebSocket.OPEN || target.ws.readyState === WebSocket.CONNECTING)) {
      try {
        target.ws.close();
      } catch {}
    }
    if (target?.lastVideo?.url) URL.revokeObjectURL(target.lastVideo.url);

    clearTaskForTarget(id);
    if (streamTimersRef.current[id]) {
      window.clearTimeout(streamTimersRef.current[id]);
      delete streamTimersRef.current[id];
    }
    if (autoCompleteTimersRef.current[id]) {
      window.clearTimeout(autoCompleteTimersRef.current[id]);
      delete autoCompleteTimersRef.current[id];
    }
    updateTargetById(id, (t) => ({
      ...t,
      connected: false,
      connecting: false,
      ws: undefined,
      lastVideo: t.lastVideo ? { ...t.lastVideo, url: undefined } : undefined,
      stream: { ...t.stream, running: false },
      ui: { loadingMedia: false, loadingProcesses: false },
    }));
  };

  const connectTarget = (id: string) => {
    updateTargetById(id, (t) => {
      if (t.ws && (t.ws.readyState === WebSocket.OPEN || t.ws.readyState === WebSocket.CONNECTING)) return t;

      const url = `ws://${t.host}:${t.port}/`;
      const ws = new WebSocket(url);
      const connectingLog: LogEntry = { text: `Connecting to ${url}`, type: "info", timestamp: new Date() };

      ws.onopen = () => {
        addLog(id, "WebSocket connected", "success");
        updateTargetById(id, (x) => ({ ...x, connected: true, connecting: false }));
        safeSend(ws, JSON.stringify({ cmd: "ping" }));
      };

      ws.onerror = () => {
        addLog(id, "WebSocket error (network / server down?)", "error");
        updateTargetById(id, (x) => ({ ...x, connecting: false }));
      };

      ws.onclose = () => {
        addLog(id, "Disconnected", "error");
        clearTaskForTarget(id);
        updateTargetById(id, (x) => {
          if (x.lastVideo?.url) URL.revokeObjectURL(x.lastVideo.url);
          return {
            ...x,
            connected: false,
            connecting: false,
            ws: undefined,
            lastVideo: x.lastVideo ? { ...x.lastVideo, url: undefined } : undefined,
            stream: { ...x.stream, running: false },
            ui: { loadingMedia: false, loadingProcesses: false },
          };
        });
      };

      ws.onmessage = (evt) => {
        const parsed = safeJsonParse(String(evt.data));
        if (!parsed || typeof parsed !== "object") {
          addLog(id, `RAW: ${String(evt.data)}`);
          return;
        }
        const data = parsed as Record<string, unknown>;

        if (data.cmd === "screen_stream" && typeof data.image_base64 === "string") {
          const seq = typeof data.seq === "number" ? data.seq : undefined;
          updateTargetById(id, (x) => ({
            ...x,
            lastImage: { kind: "screen_stream", base64: data.image_base64 as string, ts: Date.now(), seq },
            stream: {
              ...x.stream,
              running: true,
              lastSeq: typeof seq === "number" ? seq : x.stream.lastSeq,
            },
            ui: { ...x.ui, loadingMedia: false },
          }));
          return;
        }

        if (typeof data.image_base64 === "string") {
          const kind: LastImage["kind"] = data.cmd === "camera" ? "camera" : "screen";

          updateTargetById(id, (x) => ({
            ...x,
            lastImage: { kind, base64: data.image_base64 as string, ts: Date.now() },
            lastVideo: undefined,
            ui: { ...x.ui, loadingMedia: false },
            task: x.task.type === "camera" || x.task.type === "screen" ? { type: "idle", status: "idle" } : x.task,
          }));

          addLog(id, `Received image (${kind})`, "success");
          return;
        }

        if (data.cmd === "camera_video" && typeof data.video_base64 === "string") {
          const format = typeof data.format === "string" ? data.format : "bin";
          const mime = format.toLowerCase() === "avi" ? "video/x-msvideo" : "application/octet-stream";
          const blob = b64ToBlob(data.video_base64 as string, mime);
          const urlObj = URL.createObjectURL(blob);

          updateTargetById(id, (x) => {
            if (x.lastVideo?.url) URL.revokeObjectURL(x.lastVideo.url);
            return {
              ...x,
              lastVideo: { base64: data.video_base64 as string, format, ts: Date.now(), url: urlObj },
              ui: { ...x.ui, loadingMedia: false },
              task: x.task.type === "camera_video" ? { type: "idle", status: "idle" } : x.task,
            };
          });

          addLog(id, `Received camera video (${format})`, "success");
          return;
        }

        if (data.status === "ok" && Array.isArray(data.data)) {
          const procs: ProcessInfo[] = (data.data as unknown[])
            .map((p) => {
              if (!p || typeof p !== "object") return null;
              const obj = p as Record<string, unknown>;
              const pid = typeof obj.pid === "number" ? obj.pid : Number(obj.pid);
              const name = typeof obj.name === "string" ? obj.name : String(obj.name ?? "");
              const memory = typeof obj.memory === "number" ? obj.memory : undefined;
              if (!Number.isFinite(pid)) return null;
              return { pid, name, memory } as ProcessInfo;
            })
            .filter((p): p is ProcessInfo => Boolean(p));

          updateTargetById(id, (x) => ({
            ...x,
            processes: procs,
            ui: { ...x.ui, loadingProcesses: false },
            task: x.task.type === "process_list" ? { type: "idle", status: "idle" } : x.task,
          }));
          addLog(id, `Loaded ${procs.length} processes`, "success");
          return;
        }

        if (typeof data.status === "string") {
          const status = data.status as string;
          const msg = typeof data.message === "string" ? data.message : JSON.stringify(data);
          addLog(id, `${status.toUpperCase()}: ${msg}`, status === "ok" ? "success" : "error");
          return;
        }

        addLog(id, `MSG: ${JSON.stringify(data)}`);
      };

      return {
        ...t,
        ws,
        connecting: true,
        logs: [...t.logs, connectingLog].slice(-LOG_LIMIT),
      };
    });
  };

  const removeTarget = (id: string) => {
    disconnectTarget(id);
    setTargets((prev) => prev.filter((t) => t.id !== id));
    setSelectedForBroadcast((prev) => prev.filter((x) => x !== id));
    if (activeId === id) setActiveId(null);
  };

  const runTask = (task: TaskKind, payloadBuilder: () => unknown, label: string, autoCompleteMs?: number | null) => {
    const ids = getTargetIdsForAction();
    if (ids.length === 0) {
      pushToast("No target selected", "error");
      return;
    }

    ids.forEach((id) => {
      startTaskForTarget(id, task, () => {
        const ok = sendJsonToTarget(id, payloadBuilder(), label);
        if (ok && (task === "screen" || task === "camera" || task === "camera_video" || task === "screen_stream")) {
          updateTargetById(id, (t) => ({ ...t, ui: { ...t.ui, loadingMedia: true } }));
        }
        if (ok && (task === "process_list" || task === "process_start" || task === "process_kill")) {
          updateTargetById(id, (t) => ({ ...t, ui: { ...t.ui, loadingProcesses: true } }));
        }
        return ok;
      }, label, autoCompleteMs === undefined ? 8000 : autoCompleteMs);
    });
  };

  const actionPing = () => runTask("ping", () => ({ cmd: "ping" }), "ping");
  const actionListProcesses = () => runTask("process_list", () => ({ cmd: "process_list" }), "process_list");
  const actionScreen = () => runTask("screen", () => ({ cmd: "screen" }), "screen");
  const actionCamera = () => runTask("camera", () => ({ cmd: "camera" }), "camera");
  const actionCameraVideo = () => {
    const duration = Math.max(1, Math.min(30, videoDuration));
    runTask("camera_video", () => ({ cmd: "camera_video", duration }), "camera_video", duration * 1000 + 2000);
  };

  const actionScreenStream = () => {
    const duration = Math.max(1, Math.min(60, streamDuration));
    const fps = Math.max(1, Math.min(30, streamFps));
    const ids = getTargetIdsForAction();
    if (ids.length === 0) {
      pushToast("No target selected", "error");
      return;
    }

    ids.forEach((id) => {
      startTaskForTarget(
        id,
        "screen_stream",
        (signal) => {
          const ok = sendJsonToTarget(id, { cmd: "screen_stream", duration, fps }, "screen_stream");
          if (!ok) return false;

          updateTargetById(id, (t) => ({
            ...t,
            stream: { ...t.stream, running: true, duration, fps },
            ui: { ...t.ui, loadingMedia: true },
          }));

          const timer = window.setTimeout(() => {
            if (signal.aborted) return;
            updateTargetById(id, (t) => ({
              ...t,
              stream: { ...t.stream, running: false },
              task: t.task.type === "screen_stream" ? { type: "idle", status: "idle" } : t.task,
              ui: { ...t.ui, loadingMedia: false },
            }));
          }, duration * 1000 + 500);

          streamTimersRef.current[id] = timer;
          signal.addEventListener("abort", () => {
            window.clearTimeout(timer);
          });

          return true;
        },
        "screen_stream",
        null
      );
    });
  };

  const actionKillProcess = (pid: number) => {
    runTask("process_kill", () => ({ cmd: "process_kill", pid }), `process_kill:${pid}`);
  };

  const actionStartProcess = () => {
    const path = startPath.trim();
    if (!path) {
      pushToast("Process path required", "error");
      return;
    }
    runTask("process_start", () => ({ cmd: "process_start", path }), "process_start");
    setStartPath("");
  };

  const actionSendCustomJson = () => {
    const parsed = safeJsonParse(customCmd);
    if (!parsed) {
      if (active) addLog(active.id, "Invalid JSON", "error");
      pushToast("Invalid JSON payload", "error");
      return;
    }
    runTask("custom", () => parsed, "custom");
  };

  const resetUiStateForTarget = (id: string) => {
    if (streamTimersRef.current[id]) {
      window.clearTimeout(streamTimersRef.current[id]);
      delete streamTimersRef.current[id];
    }
    if (autoCompleteTimersRef.current[id]) {
      window.clearTimeout(autoCompleteTimersRef.current[id]);
      delete autoCompleteTimersRef.current[id];
    }

    updateTargetById(id, (t) => {
      if (t.lastVideo?.url) URL.revokeObjectURL(t.lastVideo.url);
      return {
        ...t,
        logs: [],
        processes: [],
        lastImage: undefined,
        lastVideo: undefined,
        stream: { ...t.stream, running: false, lastSeq: -1 },
        task: { type: "idle", status: "idle" },
        ui: { loadingMedia: false, loadingProcesses: false },
      };
    });
  };

  const handleReset = () => {
    const ids = getTargetIdsForAction();
    if (ids.length === 0) {
      pushToast("No target selected", "error");
      return;
    }

    ids.forEach((id) => {
      cancelTaskForTarget(id, "Cancelling active task…");
      resetUiStateForTarget(id);
      startTaskForTarget(id, "reset", () => {
        const ok = sendJsonToTarget(id, { cmd: "cancel_all" }, "reset");
        return ok;
      }, "Resetting");
    });
  };

  const statusLabel: ConnectionBadgeStatus = !active
    ? "No target"
    : active.connected
    ? "Connected"
    : active.connecting
    ? "Connecting"
    : "Disconnected";

  const targetLabel = active ? `${active.host}:${active.port}` : "--";

  if (!uiLoggedIn) {
    return (
      <LoginPage
        appName={APP_NAME}
        username={username}
        password={password}
        onUsernameChange={setUsername}
        onPasswordChange={setPassword}
        onSubmit={handleLogin}
      />
    );
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-[#030712] via-[#0b1224] to-[#0f172a] text-slate-100 p-4 md:p-6 relative overflow-hidden">
      <div className="pointer-events-none absolute inset-0">
        <div className="absolute -top-56 -left-40 h-[680px] w-[680px] rounded-full bg-blue-600/15 blur-3xl" />
        <div className="absolute -bottom-56 -right-48 h-[720px] w-[720px] rounded-full bg-cyan-500/15 blur-3xl" />
      </div>
      <div className="max-w-7xl mx-auto flex flex-col gap-4 h-[calc(100vh-2rem)] relative z-10">
        <Header title="Remote Control Center" connectedCount={targets.filter((t) => t.connected).length} />

        <div className="bg-slate-900/60 border border-slate-800 rounded-2xl p-4 flex flex-col gap-3 shadow-xl">
          <div className="flex flex-col md:flex-row md:items-center gap-3">
            <StatusBadge status={statusLabel} targetLabel={targetLabel} />
            {active?.task.status === "cancelling" && (
              <span className="text-xs text-amber-200 bg-amber-500/10 border border-amber-500/30 rounded-full px-3 py-1 inline-flex items-center gap-2">
                <Loader2 className="w-3 h-3 animate-spin" /> Cancelling…
              </span>
            )}
            {active?.task.status === "running" && active.task.type !== "idle" && (
              <span className="text-xs text-emerald-200 bg-emerald-500/10 border border-emerald-500/30 rounded-full px-3 py-1 inline-flex items-center gap-2">
                <Loader2 className="w-3 h-3 animate-spin" /> {active.task.label || active.task.type}
              </span>
            )}
            <div className="flex items-center gap-2 ml-auto">
              <button
                onClick={handleReset}
                className="inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-rose-500/10 border border-rose-400/40 text-sm hover:bg-rose-500/15"
              >
                <RotateCcw className="w-4 h-4" /> Reset
              </button>
              <button
                onClick={() => (active ? disconnectTarget(active.id) : null)}
                disabled={!active}
                className="inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-800 border border-slate-700 text-sm disabled:opacity-50"
              >
                <XCircle className="w-4 h-4" /> Disconnect
              </button>
            </div>
          </div>
          <div className="text-xs text-slate-400 flex items-center gap-2">
            <Wifi className="w-3 h-3" /> Connection updates live. Reset clears media, logs, and cancels ongoing tasks via cancel_all.
          </div>
        </div>

        <div className="flex-1 flex flex-col md:flex-row gap-4 min-h-0">
          <aside className="w-full md:w-80 flex flex-col gap-4 min-h-0">
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
                  className={`relative inline-flex h-6 w-11 items-center rounded-full transition ${broadcastMode ? "bg-primary" : "bg-slate-700"}`}
                >
                  <span
                    className={`inline-block h-4 w-4 transform rounded-full bg-white transition-transform ${broadcastMode ? "translate-x-6" : "translate-x-1"}`}
                  />
                </button>
              </div>
              {broadcastMode && (
                <p className="text-xs text-muted-foreground">Select targets below, then actions will be sent to all selected.</p>
              )}
            </div>

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
                        isActive ? "bg-slate-900/60 border-primary/60" : "bg-slate-950/20 border-border hover:border-slate-500/60"
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
                                inBroadcast ? "bg-primary border-primary text-white" : "bg-slate-900/60 border-border text-transparent"
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

          <main className="flex-1 flex flex-col gap-4 min-h-0">
            {!active ? (
              <div className="flex-1 bg-secondary/30 border border-border rounded-2xl p-6 flex items-center justify-center text-muted-foreground">
                Add a target to start.
              </div>
            ) : (
              <>
                <div className="bg-secondary/50 border border-border rounded-2xl p-4 shadow-lg">
                  <div className="flex items-center gap-2 mb-4">
                    {(["process", "stream", "command"] as TaskPanel[]).map((tab) => (
                      <button
                        key={tab}
                        onClick={() => setPanel(tab)}
                        className={`px-3 py-1.5 rounded-lg text-xs border transition ${
                          panel === tab
                            ? "bg-primary/20 border-primary/40 text-primary"
                            : "bg-slate-900/60 border-border text-muted-foreground hover:bg-slate-800"
                        }`}
                      >
                        {tab === "process" ? "Process" : tab === "stream" ? "Stream" : "Command"}
                      </button>
                    ))}
                  </div>

                  <div className="flex flex-col lg:flex-row gap-4 lg:items-end lg:justify-between">
                    <div className="min-w-0">
                      <div className="flex items-center gap-2">
                        <span className="text-sm font-semibold truncate">{active.name}</span>
                        <span className="text-xs text-muted-foreground font-mono truncate">({active.host}:{active.port})</span>
                      </div>
                      <p className="text-xs text-muted-foreground mt-1 flex items-center gap-2">
                        {active.connected ? "Connected" : active.connecting ? "Connecting" : "Disconnected"}
                        {broadcastMode && (
                          <span className="inline-flex items-center gap-1 text-primary bg-primary/10 rounded-full px-2 py-0.5">
                            <RadioTower className="w-3 h-3" /> broadcast
                          </span>
                        )}
                      </p>
                    </div>

                    <div className="flex flex-wrap gap-2">
                      {panel === "stream" && (
                        <>
                          <button
                            onClick={actionScreen}
                            disabled={!canSend}
                            className={`inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border transition text-sm ${
                              !canSend ? "opacity-50 cursor-not-allowed" : "hover:bg-slate-800"
                            }`}
                          >
                            <Monitor className="w-4 h-4" /> Screen
                          </button>

                          <button
                            onClick={actionCamera}
                            disabled={!canSend}
                            className={`inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border transition text-sm ${
                              !canSend ? "opacity-50 cursor-not-allowed" : "hover:bg-slate-800"
                            }`}
                          >
                            <Camera className="w-4 h-4" /> Camera
                          </button>

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
                              className={`inline-flex items-center gap-2 px-2 py-1 rounded-md bg-primary/20 transition text-sm ${
                                !canSend ? "opacity-50 cursor-not-allowed" : "hover:bg-primary/30"
                              }`}
                            >
                              <Video className="w-4 h-4" /> Record
                            </button>
                          </div>

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
                              className={`inline-flex items-center gap-2 px-2 py-1 rounded-md bg-primary/20 transition text-sm ${
                                !canSend ? "opacity-50 cursor-not-allowed" : "hover:bg-primary/30"
                              }`}
                            >
                              <Send className="w-4 h-4" /> Stream
                            </button>
                          </div>
                        </>
                      )}

                      {panel === "process" && (
                        <button
                          onClick={actionListProcesses}
                          className="inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border hover:bg-slate-800 transition text-sm"
                        >
                          <RefreshCw className="w-4 h-4" /> Processes
                        </button>
                      )}

                      {panel === "command" && (
                        <button
                          onClick={actionPing}
                          className="inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border hover:bg-slate-800 transition text-sm"
                        >
                          <Activity className="w-4 h-4" /> Ping
                        </button>
                      )}
                    </div>
                  </div>

                  <div className="mt-3 flex flex-col md:flex-row gap-2 md:items-center">
                    {panel === "process" && (
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

                    {panel === "command" && (
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
                  <div className="bg-secondary/50 border border-border rounded-2xl p-4 shadow-lg flex flex-col min-h-0 lg:col-span-2">
                    {panel === "stream" && (
                      <>
                        <div className="flex items-center gap-3 mb-3">
                          <span className="text-xs font-semibold uppercase tracking-wide text-muted-foreground flex items-center gap-2">
                            <Monitor className="w-4 h-4" /> Media
                          </span>

                          {active.lastImage && !active.stream.running && (
                            <button
                              onClick={() => downloadLastImage(active.lastImage!, active.host)}
                              className="inline-flex items-center gap-1 text-xs text-primary hover:opacity-80"
                            >
                              <Download className="w-4 h-4" /> Image
                            </button>
                          )}

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
                          {active.ui.loadingMedia && (
                            <div className="absolute top-4 right-4 inline-flex items-center gap-2 text-xs bg-slate-900/80 border border-slate-700 px-3 py-1.5 rounded-full">
                              <Loader2 className="w-3 h-3 animate-spin" /> waiting for media…
                            </div>
                          )}

                          {active.lastImage ? (
                            <img
                              src={`data:image/jpeg;base64,${active.lastImage.base64}`}
                              className="w-full h-full object-contain"
                              alt="Latest capture"
                            />
                          ) : active.lastVideo?.url ? (
                            <video src={active.lastVideo.url} controls className="w-full h-full object-contain" />
                          ) : (
                            <div className="text-sm text-muted-foreground text-center">Use Screen / Camera / Stream</div>
                          )}
                        </div>
                      </>
                    )}

                    {panel === "process" && (
                      <>
                        <div className="flex items-center justify-between mb-3">
                          <span className="text-xs font-semibold uppercase tracking-wide text-muted-foreground">Process List</span>

                          <button
                            onClick={actionListProcesses}
                            className="inline-flex items-center gap-2 px-2 py-1 rounded-lg bg-slate-900/60 border border-border text-xs"
                          >
                            <RefreshCw className="w-3 h-3" /> Refresh
                          </button>
                        </div>

                        <div className="flex-1 overflow-auto rounded-xl border border-border bg-slate-950/30 relative">
                          {active.ui.loadingProcesses && (
                            <div className="absolute inset-0 bg-slate-950/60 flex items-center justify-center text-xs text-muted-foreground gap-2">
                              <Loader2 className="w-4 h-4 animate-spin" /> Loading processes…
                            </div>
                          )}
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
                              {active.processes.length === 0 && !active.ui.loadingProcesses && (
                                <tr>
                                  <td colSpan={4} className="px-3 py-6 text-center text-muted-foreground">
                                    No processes yet. Refresh to load.
                                  </td>
                                </tr>
                              )}
                            </tbody>
                          </table>
                        </div>
                      </>
                    )}
                  </div>

                  <div className="bg-secondary/50 border border-border rounded-2xl p-4 shadow-lg flex flex-col min-h-0">
                    <div className="flex items-center justify-between mb-3">
                      <span className="text-xs font-semibold uppercase tracking-wide text-muted-foreground">Logs</span>

                      <button
                        onClick={() => setTargets((prev) => prev.map((t) => (t.id === active.id ? { ...t, logs: [] } : t)))}
                        className="inline-flex items-center gap-2 px-2.5 py-1.5 rounded-lg bg-slate-900/60 hover:bg-slate-800 text-[11px] border border-border transition"
                      >
                        <Trash2 className="w-3 h-3" />
                        Clear
                      </button>
                    </div>

                    <div className="flex-1 overflow-auto rounded-xl border border-border bg-slate-950/30 p-3">
                      {active.logs.map((l, i) => (
                        <div key={i} className="text-xs">
                          <span className="text-muted-foreground font-mono">[{formatTime(l.timestamp)}]</span> {" "}
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
                      {active.logs.length === 0 && (
                        <div className="text-xs text-muted-foreground">No logs yet. Actions will appear here.</div>
                      )}
                      <div ref={logsEndRef} />
                    </div>
                  </div>
                </div>
              </>
            )}
          </main>
        </div>
      </div>

      <ToastStack toasts={toasts} onDismiss={(id) => setToasts((prev) => prev.filter((t) => t.id !== id))} />
    </div>
  );
}
