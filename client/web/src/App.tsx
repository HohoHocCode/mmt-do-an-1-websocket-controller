import Header from "./components/Header";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type { KeyboardEvent as ReactKeyboardEvent } from "react";
import {
  Activity,
  Bell,
  Camera,
  Check,
  ChevronDown,
  Cpu,
  Download,
  Film,
  Keyboard,
  Image as ImageIcon,
  Loader2,
  Lock,
  Monitor,
  Plus,
  Power,
  PowerOff,
  RadioTower,
  RefreshCw,
  Search,
  Send,
  Shield,
  Trash2,
  Video,
  Wifi,
  XCircle,
} from "lucide-react";
import LoginPage from "./LoginPage";
import { useAuth } from "./auth";
import { ApiError, discoverDevices, getControllerStatus, logAudit, restartController, stopController } from "./api";
import type { AuthUser, ControllerStatus, DiscoveryDevice, HotkeyAction, HotkeyMap, WsMessage } from "./types";

// [ADDED] App name (để LoginPage / Header dùng thống nhất)
const APP_NAME = import.meta.env.VITE_APP_NAME || "Remote Desktop Control"; // [ADDED]

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

interface TargetActivity {
  state: "idle" | "running" | "error";
  label: string;
  updatedAt: number;
}

interface PendingAction {
  requestId: string;
  startedAt: number;
}

interface ToastPayload {
  text: string;
  tone: "info" | "success" | "error";
}

interface RemoteTarget {
  id: string;
  name: string;
  host: string;
  port: number;
  connected: boolean;
  connecting: boolean;
  ws?: WebSocket;
  authStatus?: "idle" | "ok" | "failed";
  authUser?: AuthUser | null;

  logs: Log[];
  processes: Proc[];
  lastImage?: LastImage;
  lastVideo?: LastVideo;
  stream: StreamState;
  activity: TargetActivity;
  lastError?: string | null;
  pending: Record<string, PendingAction | undefined>;
}

const DEFAULT_PORT = 9002;
const DISCOVERY_PORT = Number(import.meta.env.VITE_DISCOVERY_PORT || 41000);
const HOTKEY_STORAGE_KEY = "rdc.hotkeys";
const HOTKEY_ENABLED_KEY = "rdc.hotkeys.enabled";
const DEFAULT_HOTKEYS: HotkeyMap = {
  connect: "ctrl+k",
  reset: "ctrl+shift+x",
  stream: "ctrl+shift+s",
  processes: "ctrl+shift+p",
};
const createId = () => `${Date.now()}-${Math.random().toString(16).slice(2)}`;
const createRequestId = () => {
  if (typeof crypto !== "undefined" && "randomUUID" in crypto) {
    return crypto.randomUUID();
  }
  return `req-${Date.now()}-${Math.random().toString(16).slice(2)}`;
};
const ACTION_TIMEOUT_MS = 20000;

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

function normalizeHotkey(combo: string) {
  return combo
    .trim()
    .toLowerCase()
    .replace("meta", "ctrl");
}

function formatEventHotkey(e: KeyboardEvent | ReactKeyboardEvent): string | null {
  const parts: string[] = [];
  if (e.ctrlKey || e.metaKey) parts.push("ctrl");
  if (e.shiftKey) parts.push("shift");
  if (e.altKey) parts.push("alt");
  const key = e.key?.toLowerCase();
  if (!key || key === "control" || key === "shift" || key === "alt" || key === "meta") return null;
  parts.push(key.length === 1 ? key : key.replace("arrow", "arrow "));
  return normalizeHotkey(parts.join("+"));
}

function prettyHotkey(combo: string) {
  return combo
    .split("+")
    .map((part) => part.length === 1 ? part.toUpperCase() : part[0].toUpperCase() + part.slice(1))
    .join(" + ");
}

function loadHotkeys(): HotkeyMap {
  try {
    const raw = localStorage.getItem(HOTKEY_STORAGE_KEY);
    if (!raw) return DEFAULT_HOTKEYS;
    const parsed = JSON.parse(raw) as HotkeyMap;
    return { ...DEFAULT_HOTKEYS, ...parsed };
  } catch {
    return DEFAULT_HOTKEYS;
  }
}

const isFormElement = (target: EventTarget | null) => {
  if (!target || !(target as HTMLElement).tagName) return false;
  const tag = (target as HTMLElement).tagName.toLowerCase();
  return tag === "input" || tag === "textarea" || tag === "select";
};

export default function App() {
  const { user, token, locked, lockedReason, lockSession } = useAuth();
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
  const activityTimers = useRef<Record<string, ReturnType<typeof setTimeout> | undefined>>({});
  const toastTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const pendingActionsRef = useRef<Record<string, Record<string, PendingAction | undefined>>>({});
  const pendingTimeoutsRef = useRef<Record<string, ReturnType<typeof setTimeout> | undefined>>({});
  const hotkeyHandlerRef = useRef<(action: HotkeyAction) => void>(() => {});
  const [discovering, setDiscovering] = useState(false);
  const [discoveredDevices, setDiscoveredDevices] = useState<DiscoveryDevice[]>([]);
  const [discoverError, setDiscoverError] = useState<string | null>(null);
  const [powerMenuOpen, setPowerMenuOpen] = useState(false);
  const [pendingControllerAction, setPendingControllerAction] = useState<"restart" | "stop" | null>(null);
  const [toast, setToast] = useState<ToastPayload | null>(null);
  const [controllerStatus, setControllerStatus] = useState<ControllerStatus | null>(null);
  const [controllerBusy, setControllerBusy] = useState(false);
  const [hotkeysEnabled, setHotkeysEnabled] = useState<boolean>(() => localStorage.getItem(HOTKEY_ENABLED_KEY) === "true");
  const [hotkeys, setHotkeys] = useState<HotkeyMap>(() => loadHotkeys());
  const [hotkeyConflicts, setHotkeyConflicts] = useState<Record<string, string>>({});

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

  const selectedTargets = broadcastMode
    ? targets.filter((t) => selectedForBroadcast.includes(t.id))
    : active
    ? [active]
    : [];
  const anyConnected = selectedTargets.some((t) => t.connected);
  const anyBusy = selectedTargets.some((t) => t.activity.state === "running");
  const canSend = selectedTargets.length > 0 && anyConnected && !!token && !locked;
  const actionsDisabled = !canSend;
  const activePending = active?.pending ?? {};
  const isPendingForActive = (cmd: string) => Boolean(activePending[cmd]);

  useEffect(() => {
    if (!activeId && targets.length > 0) setActiveId(targets[0].id);
  }, [targets, activeId]);

  useEffect(() => {
    logsEndRef.current?.scrollIntoView({ behavior: "smooth", block: "end" });
  }, [targets, activeId]);

  const fetchControllerStatus = useCallback(async () => {
    if (!token) return;
    try {
      const res = await getControllerStatus(token);
      if (res.ok) setControllerStatus(res.status);
    } catch (err) {
      console.warn("Controller status check failed", err);
    }
  }, [token]);

  useEffect(() => {
    fetchControllerStatus();
  }, [fetchControllerStatus]);

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
        Object.values(activityTimers.current).forEach((timer) => {
          if (timer) clearTimeout(timer);
        });
        Object.values(pendingTimeoutsRef.current).forEach((timer) => {
          if (timer) clearTimeout(timer);
        });
        if (toastTimer.current) clearTimeout(toastTimer.current);
      } catch {}
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []); // cố tình [] để cleanup 1 lần

  const addLog = (id: string, text: string, type: LogType = "info") => {
    setTargets((prev) =>
      prev.map((t) =>
        t.id === id
          ? {
              ...t,
              // [PATCH] giới hạn log
              logs: [...t.logs, { text, type, timestamp: new Date() }].slice(-LOG_LIMIT),
            }
          : t
          )
    );
  };

  const clearActivityTimer = (id: string) => {
    const timer = activityTimers.current[id];
    if (timer) {
      clearTimeout(timer);
    }
    activityTimers.current[id] = undefined;
  };

  const setActivity = (id: string, activity: TargetActivity) => {
    setTargets((prev) =>
      prev.map((t) =>
        t.id === id
          ? {
              ...t,
              activity: { ...activity, updatedAt: Date.now() },
            }
          : t
      )
    );
  };

  const markRunning = (id: string, label: string) => {
    clearActivityTimer(id);
    setActivity(id, { state: "running", label, updatedAt: Date.now() });
    activityTimers.current[id] = setTimeout(() => {
      setActivity(id, { state: "idle", label: "Ready", updatedAt: Date.now() });
      activityTimers.current[id] = undefined;
    }, 4200);
  };

  const markIdle = (id: string, label = "Ready") => {
    clearActivityTimer(id);
    setActivity(id, { state: "idle", label, updatedAt: Date.now() });
  };

  const markError = (id: string, label: string) => {
    clearActivityTimer(id);
    setActivity(id, { state: "error", label, updatedAt: Date.now() });
    setTargets((prev) =>
      prev.map((t) => (t.id === id ? { ...t, lastError: label } : t))
    );
  };

  const pendingKey = (id: string, cmd: string) => `${id}:${cmd}`;

  const isActionPending = (id: string, cmd: string) =>
    Boolean(pendingActionsRef.current[id]?.[cmd]);

  const setPendingAction = (id: string, cmd: string, requestId: string, label?: string) => {
    const key = pendingKey(id, cmd);
    if (pendingTimeoutsRef.current[key]) {
      clearTimeout(pendingTimeoutsRef.current[key]);
    }
    const startedAt = Date.now();
    if (!pendingActionsRef.current[id]) {
      pendingActionsRef.current[id] = {};
    }
    pendingActionsRef.current[id][cmd] = { requestId, startedAt };
    setTargets((prev) =>
      prev.map((t) =>
        t.id === id
          ? { ...t, pending: { ...t.pending, [cmd]: { requestId, startedAt } } }
          : t
      )
    );
    pendingTimeoutsRef.current[key] = setTimeout(() => {
      clearPendingAction(id, cmd);
      markError(id, `${label || cmd} timeout`);
      showToast({ text: `${label || cmd} timeout`, tone: "error" });
    }, ACTION_TIMEOUT_MS);
  };

  const clearPendingAction = (id: string, cmd: string) => {
    const key = pendingKey(id, cmd);
    if (pendingTimeoutsRef.current[key]) {
      clearTimeout(pendingTimeoutsRef.current[key]);
      pendingTimeoutsRef.current[key] = undefined;
    }
    if (pendingActionsRef.current[id]) {
      delete pendingActionsRef.current[id][cmd];
    }
    setTargets((prev) =>
      prev.map((t) => {
        if (t.id !== id) return t;
        const { [cmd]: _, ...rest } = t.pending;
        return { ...t, pending: rest };
      })
    );
  };

  const clearPendingByRequestId = (id: string, requestId: string) => {
    const pending = pendingActionsRef.current[id];
    if (!pending) return;
    const cmd = Object.keys(pending).find((key) => pending[key]?.requestId === requestId);
    if (cmd) {
      clearPendingAction(id, cmd);
    }
  };

  const clearAllPending = (id: string) => {
    const pending = pendingActionsRef.current[id];
    if (!pending) return;
    Object.keys(pending).forEach((cmd) => clearPendingAction(id, cmd));
  };

  const toggleBroadcastSelection = (id: string) => {
    setSelectedForBroadcast((prev) =>
      prev.includes(id) ? prev.filter((x) => x !== id) : [...prev, id]
    );
  };

  const showToast = (payload: ToastPayload) => {
    if (toastTimer.current) clearTimeout(toastTimer.current);
    setToast(payload);
    toastTimer.current = setTimeout(() => setToast(null), 3000);
  };

  const updateHotkey = (action: HotkeyAction, combo: string | null) => {
    setHotkeys((prev) => {
      const next = { ...prev, [action]: combo ? normalizeHotkey(combo) : "" };
      localStorage.setItem(HOTKEY_STORAGE_KEY, JSON.stringify(next));
      return next;
    });
  };

  const resetHotkeys = () => {
    setHotkeys(DEFAULT_HOTKEYS);
    localStorage.setItem(HOTKEY_STORAGE_KEY, JSON.stringify(DEFAULT_HOTKEYS));
  };

  useEffect(() => {
    const used = new Map<string, string>();
    const conflicts: Record<string, string> = {};
    (Object.entries(hotkeys) as [string, string][]).forEach(([actionKey, combo]) => {
      if (!combo) return;
      const normalized = normalizeHotkey(combo);
      if (used.has(normalized)) {
        const other = used.get(normalized)!;
        conflicts[actionKey] = other;
        conflicts[other] = actionKey;
      } else {
        used.set(normalized, actionKey);
      }
    });
    setHotkeyConflicts(conflicts);
  }, [hotkeys]);

  useEffect(() => {
    localStorage.setItem(HOTKEY_ENABLED_KEY, hotkeysEnabled ? "true" : "false");
  }, [hotkeysEnabled]);

  const recordAudit = async (action: string, meta: Record<string, unknown> = {}) => {
    if (!token) return;
    try {
      await logAudit(token, action, meta);
    } catch (err) {
      console.warn("Failed to log audit", err);
    }
  };

  const handleControllerRestart = async () => {
    if (!token) return;
    setControllerBusy(true);
    try {
      const res = await restartController(token);
      setControllerStatus(res.status);
      showToast({ text: "Controller restarting...", tone: "info" });
      await recordAudit("controller_restart", {});
    } catch (err) {
      const message = err instanceof ApiError ? err.message : "Restart failed";
      showToast({ text: message, tone: "error" });
    } finally {
      setControllerBusy(false);
      setPendingControllerAction(null);
    }
  };

  const handleControllerStop = async () => {
    if (!token) return;
    setControllerBusy(true);
    try {
      const res = await stopController(token);
      setControllerStatus(res.status);
      showToast({ text: "Controller stopped", tone: "success" });
      await recordAudit("controller_stop", {});
    } catch (err) {
      const message = err instanceof ApiError ? err.message : "Stop failed";
      showToast({ text: message, tone: "error" });
    } finally {
      setControllerBusy(false);
      setPendingControllerAction(null);
    }
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
      authStatus: "idle",
      authUser: null,
      stream: { running: false, fps: 5, duration: 5, lastSeq: -1 },
      activity: { state: "idle", label: "Disconnected", updatedAt: Date.now() },
      lastError: null,
      pending: {},
    };

    setTargets((prev) => [...prev, t]);
    setNewName("");
    setNewHost("");
    setNewPort(String(DEFAULT_PORT));
    if (!activeId) setActiveId(id);
  };

  const handleAddDiscovered = (device: DiscoveryDevice) => {
    const host = device.ip || "";
    if (!host) return;
    setNewHost(host);
    setNewPort(String(device.wsPort || DEFAULT_PORT));
    setNewName(device.name || host);
    showToast({ text: "Device info loaded. Click Add or Connect.", tone: "success" });
  };

  const handleConnectDiscovered = (device: DiscoveryDevice) => {
    const host = device.ip || "";
    if (!host) return;
    const port = device.wsPort ?? DEFAULT_PORT;
    const id = createId();
    const target: RemoteTarget = {
      id,
      name: device.name || host,
      host,
      port,
      connected: false,
      connecting: false,
      logs: [{ text: "Pending discovery connect", type: "info", timestamp: new Date() }],
      processes: [],
      authStatus: "idle",
      authUser: null,
      stream: { running: false, fps: 5, duration: 5, lastSeq: -1 },
      activity: { state: "running", label: "Connecting", updatedAt: Date.now() },
      lastError: null,
      pending: {},
    };
    setTargets((prev) => [...prev, target]);
    setActiveId(id);
    setTimeout(() => connectTarget(id), 80);
    showToast({ text: `Connecting to ${host}`, tone: "info" });
  };

  const handleDiscoverDevices = async () => {
    if (!token) return;
    setDiscovering(true);
    setDiscoverError(null);
    try {
      const res = await discoverDevices(token, { retries: 2 });
      const list = (res.devices || []).map((d) => ({
        ...d,
        lastSeenMs: d.lastSeenMs ?? Date.now(),
      })) as DiscoveryDevice[];
      setDiscoveredDevices(list);
      await recordAudit("discover_devices", { found: list.length });
      if (!list.length) {
        setDiscoverError(`No agents responded on UDP ${DISCOVERY_PORT}. Ensure discovery is allowed on your LAN.`);
      } else {
        showToast({ text: `Found ${list.length} device(s)`, tone: "success" });
      }
    } catch (err) {
      const msg = err instanceof ApiError ? err.message : "Discovery failed";
      setDiscoverError(msg);
    } finally {
      setDiscovering(false);
    }
  };

  const disconnectTarget = (id: string) => {
    clearAllPending(id);
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
          activity: { state: "idle", label: "Disconnected", updatedAt: Date.now() },
          lastError: null,
          pending: {},
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
          setTargets((inner) =>
            inner.map((x) =>
              x.id === id
                ? { ...x, connected: true, connecting: false, authStatus: token ? "idle" : "failed" }
                : x
            )
          );
          markIdle(id, "Connected");
          if (token) {
            safeSend(ws, JSON.stringify({ cmd: "auth", token }));
          }
          // ping an toàn
          safeSend(ws, JSON.stringify({ cmd: "ping" }));
        };

        ws.onerror = () => {
          addLog(id, "WebSocket error (network / server down?)", "error");
          markError(id, "Connection error");
          // [PATCH] nếu lỗi sớm, thường sẽ onclose; nhưng nếu không, vẫn hạ connecting
          setTargets((inner) =>
            inner.map((x) => (x.id === id ? { ...x, connecting: false } : x))
          );
        };

        ws.onclose = () => {
          addLog(id, "Disconnected", "error");
          clearAllPending(id);
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
                authStatus: "idle",
                authUser: null,
                activity: { state: "idle", label: "Disconnected", updatedAt: Date.now() },
                pending: {},
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
          const data = parsed as WsMessage;
          if (typeof data.requestId === "string") {
            clearPendingByRequestId(id, data.requestId);
          }

          if (data.cmd === "auth") {
            const status = typeof data.status === "string" ? data.status : "error";
            const info: AuthUser | null =
              status === "ok" && typeof data.username === "string" && typeof data.role === "string"
                ? { username: data.username as string, role: data.role as AuthUser["role"] }
                : null;
            setTargets((inner) =>
              inner.map((x) =>
                x.id === id ? { ...x, authStatus: status === "ok" ? "ok" : "failed", authUser: info } : x
              )
            );
            addLog(
              id,
              status === "ok" ? `Session authenticated as ${info?.username}` : `Auth failed: ${data.message || "invalid"}`,
              status === "ok" ? "success" : "error"
            );
            if (status === "ok") {
              markIdle(id, "Authenticated");
            } else {
              markError(id, "Auth failed");
            }
            return;
          }

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
            markRunning(id, "Streaming");
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
            markIdle(id, "Image received");
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
            markIdle(id, "Video ready");
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
            markIdle(id, "Processes updated");
            return;
          }

          // generic status
          if (typeof data.status === "string") {
            const status = data.status as string;
            const msg = typeof data.message === "string" ? data.message : JSON.stringify(data);
            addLog(id, `${status.toUpperCase()}: ${msg}`, status === "ok" ? "success" : "error");
            if (status === "ok") {
              markIdle(id, "Completed");
            } else {
              markError(id, msg);
            }
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
          activity: { state: "running", label: "Connecting", updatedAt: Date.now() },
          lastError: null,
        };
      })
    );
  };

  const extractCommand = (payload: unknown) => {
    if (!payload || typeof payload !== "object") return null;
    const cmdValue = (payload as { cmd?: unknown }).cmd;
    return typeof cmdValue === "string" ? cmdValue : null;
  };

  const attachRequestId = (payload: unknown, requestId: string) => {
    if (!payload || typeof payload !== "object") return payload;
    return { ...(payload as Record<string, unknown>), requestId };
  };

  const sendJsonToTarget = (id: string, payload: unknown, label?: string) => {
    const cmd = extractCommand(payload);
    if (cmd && isActionPending(id, cmd)) {
      showToast({ text: `Action "${cmd}" is already running`, tone: "info" });
      return false;
    }

    const target = targets.find((t) => t.id === id);
    if (!target) return false;

    if (locked) {
      setTargets((prev) =>
        prev.map((t) =>
          t.id === id
            ? {
                ...t,
                logs: [
                  ...t.logs,
                  { text: "Session locked. Please sign in again to run actions.", type: "error" as LogType, timestamp: new Date() },
                ].slice(-LOG_LIMIT),
                activity: { state: "error", label: "Session locked", updatedAt: Date.now() },
                lastError: "Session locked",
              }
            : t
        )
      );
      return false;
    }
    if (!token) {
      setTargets((prev) =>
        prev.map((t) =>
          t.id === id
            ? {
                ...t,
                logs: [
                  ...t.logs,
                  { text: "Missing auth token. Please login again.", type: "error" as LogType, timestamp: new Date() },
                ].slice(-LOG_LIMIT),
                activity: { state: "error", label: "Missing auth token", updatedAt: Date.now() },
                lastError: "Missing auth token",
              }
            : t
        )
      );
      return false;
    }
    if (!target.ws || target.ws.readyState !== WebSocket.OPEN) {
      setTargets((prev) =>
        prev.map((t) =>
          t.id === id
            ? {
                ...t,
                logs: [
                  ...t.logs,
                  { text: "Cannot send: not connected", type: "error" as LogType, timestamp: new Date() },
                ].slice(-LOG_LIMIT),
                activity: { state: "error", label: "Not connected", updatedAt: Date.now() },
                lastError: "Not connected",
              }
            : t
        )
      );
      return false;
    }

    const requestId = cmd ? createRequestId() : "";
    const outbound = cmd ? attachRequestId(payload, requestId) : payload;
    const raw = JSON.stringify(outbound);

    const ok = safeSend(target.ws, raw);
    if (!ok) {
      setTargets((prev) =>
        prev.map((t) =>
          t.id === id
            ? {
                ...t,
                logs: [
                  ...t.logs,
                  { text: "Send failed (socket not open)", type: "error" as LogType, timestamp: new Date() },
                ].slice(-LOG_LIMIT),
              }
            : t
        )
      );
      return false;
    }

    if (cmd && requestId) {
      setPendingAction(id, cmd, requestId, label ?? cmd);
    }
    setTargets((prev) =>
      prev.map((t) =>
        t.id === id
          ? {
              ...t,
              logs: [
                ...t.logs,
                { text: label ? `$ ${label}  ${raw}` : `$ ${raw}`, type: "command" as LogType, timestamp: new Date() },
              ].slice(-LOG_LIMIT),
            }
          : t
      )
    );
    return true;
  };

  const sendJson = (payload: unknown, label?: string, options?: { markRunning?: boolean }) => {
    const targetIds: string[] = [];
    if (broadcastMode) {
      if (selectedForBroadcast.length === 0) return;
      targetIds.push(...selectedForBroadcast);
    } else if (active) {
      targetIds.push(active.id);
    }

    const sentIds: string[] = [];

    if (broadcastMode) {
      targetIds.forEach((id) => {
        const sent = sendJsonToTarget(id, payload, label ? `BROADCAST:${label}` : "BROADCAST");
        if (sent) sentIds.push(id);
      });
    } else if (active) {
      const sent = sendJsonToTarget(active.id, payload, label);
      if (sent) sentIds.push(active.id);
    }

    if (options?.markRunning) {
      sentIds.forEach((id) => markRunning(id, label || "Working"));
    }
  };

  const actionPing = () => sendJson({ cmd: "ping" }, "ping", { markRunning: true });
  const actionListProcesses = () => sendJson({ cmd: "process_list" }, "process_list", { markRunning: true });
  const actionScreen = () => sendJson({ cmd: "screen" }, "screen", { markRunning: true });
  const actionCamera = () => sendJson({ cmd: "camera" }, "camera", { markRunning: true });
  const actionCameraVideo = () =>
    sendJson(
      { cmd: "camera_video", duration: Math.max(1, Math.min(30, videoDuration)) },
      "camera_video",
      { markRunning: true }
    );
  const actionScreenStream = () => {
    const dur = Math.max(1, Math.min(60, streamDuration));
    const fps = Math.max(1, Math.min(30, streamFps));
    if (broadcastMode) {
      sendJson({ cmd: "screen_stream", duration: dur, fps }, "screen_stream", { markRunning: true });
      return;
    }
    if (!active) return;
    const sent = sendJsonToTarget(active.id, { cmd: "screen_stream", duration: dur, fps }, "screen_stream");
    if (!sent) return;
    markRunning(active.id, "screen_stream");
    setTargets((prev) =>
      prev.map((t) =>
        t.id === active.id ? { ...t, stream: { ...t.stream, running: true, duration: dur, fps } } : t
      )
    );
  };

  const actionKillProcess = (pid: number) =>
    sendJson({ cmd: "process_kill", pid }, `process_kill:${pid}`, { markRunning: true });
  const actionStartProcess = () => {
    const path = startPath.trim();
    if (!path) return;
    sendJson({ cmd: "process_start", path }, "process_start", { markRunning: true });
    setStartPath("");
  };

  const handleResetTasks = () => {
    sendJson({ cmd: "cancel_all" }, "cancel_all");
    setTargets((prev) =>
      prev.map((t) => ({
        ...t,
        stream: { ...t.stream, running: false },
        activity: { state: "idle", label: "Idle after reset", updatedAt: Date.now() },
        lastError: null,
      }))
    );
    showToast({ text: "Reset done", tone: "success" });
    recordAudit("reset", { target: active?.host ?? "broadcast" });
  };

  const requestControllerAction = (action: "restart" | "stop") => {
    setPendingControllerAction(action);
    setPowerMenuOpen(false);
  };

  const confirmControllerAction = () => {
    if (!pendingControllerAction) return;
    if (pendingControllerAction === "restart") {
      void handleControllerRestart();
    } else {
      void handleControllerStop();
    }
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
    clearActivityTimer(id);
    clearAllPending(id);

    setTargets((prev) => prev.filter((x) => x.id !== id));
    setSelectedForBroadcast((prev) => prev.filter((x) => x !== id));
    if (activeId === id) setActiveId(null);
  };

  useEffect(() => {
    hotkeyHandlerRef.current = (actionKey: HotkeyAction) => {
      if (actionKey === "connect") {
        if (active) {
          connectTarget(active.id);
        } else {
          showToast({ text: "No target selected", tone: "error" });
        }
        return;
      }
      if (actionKey === "reset") {
        handleResetTasks();
        return;
      }
      if (!canSend) {
        showToast({ text: "Connect to a target first", tone: "error" });
        return;
      }
      if (actionsDisabled) {
        showToast({ text: "Target is busy, wait for it to finish.", tone: "error" });
        return;
      }
      if (actionKey === "stream") {
        actionScreenStream();
      } else if (actionKey === "processes") {
        actionListProcesses();
      }
    };
  });

  useEffect(() => {
    if (!hotkeysEnabled) return;
    const handler = (e: KeyboardEvent) => {
      if (isFormElement(e.target)) return;
      if (e.repeat) return;
      const combo = formatEventHotkey(e);
      if (!combo) return;
      const match = (Object.entries(hotkeys) as [HotkeyAction, string][]).find(
        ([, hk]) => hk && normalizeHotkey(hk) === combo
      );
      if (!match) return;
      e.preventDefault();
      hotkeyHandlerRef.current(match[0]);
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, [hotkeysEnabled, hotkeys]);

  const HotkeyRow = ({ action, label }: { action: HotkeyAction; label: string }) => {
    const conflictWith = hotkeyConflicts[action];
    const value = hotkeys[action] ?? "";
    return (
      <div className="space-y-1">
        <div className="flex items-center justify-between gap-2">
          <span className="text-xs font-medium text-slate-100">{label}</span>
          {conflictWith ? (
            <span className="text-[11px] text-amber-300">Conflicts with {conflictWith}</span>
          ) : null}
        </div>
        <div className="flex items-center gap-2">
          <input
            aria-label={`${label} hotkey`}
            value={value ? prettyHotkey(value) : ""}
            placeholder="Press keys"
            readOnly
            onKeyDown={(e) => {
              e.preventDefault();
              e.stopPropagation();
              const combo = formatEventHotkey(e);
              if (combo) updateHotkey(action, combo);
            }}
            className="flex-1 px-3 py-2 rounded-lg bg-slate-900/60 border border-border text-sm focus:outline-none focus:ring-2 focus:ring-primary/60"
          />
          <button
            onClick={() => updateHotkey(action, null)}
            className="px-2 py-1 text-[11px] rounded-lg border border-border bg-slate-900/60 hover:bg-slate-800"
          >
            Clear
          </button>
        </div>
      </div>
    );
  };

  if (!token || !user || locked) {
    return <LoginPage appName={APP_NAME} lockedReason={lockedReason} />;
  }

  return (
    <>
    <div className="min-h-screen bg-[#040A18] text-slate-100 p-4 md:p-6 relative overflow-hidden">
      <div className="pointer-events-none absolute inset-0">
        <div className="absolute -top-56 -left-40 h-[680px] w-[680px] rounded-full bg-blue-600/20 blur-3xl" />
        <div className="absolute -bottom-56 -right-48 h-[720px] w-[720px] rounded-full bg-cyan-500/20 blur-3xl" />
      </div>
      <div className="max-w-7xl mx-auto flex flex-col gap-4 h-[calc(100vh-2rem)]">
        <Header title="Remote Control Center" connectedCount={targets.filter((t) => t.connected).length} />

        <div className="bg-secondary/40 border border-border rounded-2xl px-4 py-3 shadow-lg flex flex-wrap items-center gap-3 justify-between">
          <div className="flex items-center gap-3 flex-wrap">
            <div className="inline-flex items-center gap-2 rounded-full border border-border bg-slate-900/50 px-3 py-1.5 text-xs">
              <Shield className="w-4 h-4 text-primary" />
              <span className="font-semibold text-slate-50">{user.username}</span>
              <span className="text-[10px] uppercase tracking-wide text-slate-300 bg-slate-800 px-2 py-0.5 rounded-full">
                {user.role}
              </span>
            </div>
            <div className="inline-flex items-center gap-2 text-xs text-muted-foreground rounded-full border border-border bg-slate-900/50 px-3 py-1.5">
              <Wifi className="w-4 h-4 text-emerald-300" /> {targets.filter((t) => t.connected).length} connected
            </div>
            <div
              className={`inline-flex items-center gap-2 text-xs px-3 py-1.5 rounded-full border ${
                anyBusy ? "border-amber-400/60 bg-amber-500/10 text-amber-100" : "border-border bg-slate-900/50 text-muted-foreground"
              }`}
            >
              <Bell className="w-4 h-4" />
              {anyBusy ? "Task running" : "Idle"}
            </div>
            <div
              className={`inline-flex items-center gap-2 text-xs px-3 py-1.5 rounded-full border ${
                controllerStatus?.status === "running"
                  ? "border-emerald-400/50 bg-emerald-500/10 text-emerald-100"
                  : controllerStatus?.status === "error"
                  ? "border-rose-400/50 bg-rose-500/10 text-rose-100"
                  : "border-border bg-slate-900/50 text-muted-foreground"
              }`}
            >
              <PowerOff className="w-4 h-4" />
              <span>Controller: {controllerStatus?.status ?? "idle"}</span>
              {controllerStatus?.pid ? <span className="text-[10px] text-slate-300">PID {controllerStatus.pid}</span> : null}
            </div>
          </div>

          <div className="flex items-center gap-2 flex-wrap">
            <button
              onClick={handleResetTasks}
              className="inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border text-sm hover:bg-slate-800 disabled:opacity-60"
              disabled={selectedTargets.length === 0}
            >
              <RefreshCw className="w-4 h-4" /> Reset / Cancel All
            </button>
            <button
              onClick={() => {
                recordAudit("lock", { source: "ui" });
                lockSession("Session locked. Sign in to continue.");
              }}
              className="inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border text-sm hover:bg-slate-800"
            >
              <Lock className="w-4 h-4" /> Lock session
            </button>

            <div className="relative">
              <button
                onClick={() => {
                  setPowerMenuOpen((v) => !v);
                  fetchControllerStatus();
                }}
                className="inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-primary text-sm font-semibold text-slate-50 shadow-lg disabled:opacity-60"
                disabled={controllerBusy}
              >
                <Power className="w-4 h-4" /> Controller <ChevronDown className="w-4 h-4" />
              </button>
              {powerMenuOpen ? (
                <div className="absolute right-0 mt-2 w-48 rounded-xl border border-border bg-slate-900/90 shadow-xl z-10">
                  <button
                    onClick={() => {
                      requestControllerAction("restart");
                      setPowerMenuOpen(false);
                    }}
                    className="w-full px-3 py-2 text-left text-sm hover:bg-slate-800 flex items-center gap-2 disabled:opacity-60"
                    disabled={user.role !== "admin" || controllerBusy}
                  >
                    <RefreshCw className="w-4 h-4" /> Restart controller
                  </button>
                  <button
                    onClick={() => {
                      requestControllerAction("stop");
                      setPowerMenuOpen(false);
                    }}
                    className="w-full px-3 py-2 text-left text-sm hover:bg-slate-800 flex items-center gap-2 disabled:opacity-60"
                    disabled={user.role !== "admin" || controllerBusy}
                  >
                    <PowerOff className="w-4 h-4" /> Stop controller
                  </button>
                  <button
                    onClick={() => {
                      fetchControllerStatus();
                      setPowerMenuOpen(false);
                    }}
                    className="w-full px-3 py-2 text-left text-sm hover:bg-slate-800 flex items-center gap-2"
                  >
                    <Search className="w-4 h-4" /> Refresh status
                  </button>
                </div>
              ) : null}
            </div>
          </div>
        </div>

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

            {/* Discovery */}
            <div className="bg-secondary/50 border border-border rounded-2xl p-4 space-y-3 shadow-lg">
              <div className="flex items-center justify-between gap-2">
                <div className="flex items-center gap-2">
                  <Search className="w-4 h-4 text-primary" />
                  <span className="text-sm font-medium">Discover devices</span>
                </div>
                <button
                  onClick={handleDiscoverDevices}
                  disabled={discovering}
                  className="inline-flex items-center gap-2 px-3 py-1.5 rounded-lg bg-slate-900/70 border border-border text-xs hover:bg-slate-800 disabled:opacity-50"
                >
                  {discovering ? <Loader2 className="w-4 h-4 animate-spin" /> : <RefreshCw className="w-4 h-4" />}
                  Discover on LAN
                </button>
              </div>
              {discoverError ? <p className="text-[11px] text-amber-200">{discoverError}</p> : null}
              <div className="space-y-2 max-h-32 overflow-auto pr-1">
                {discoveredDevices.length === 0 ? (
                  <p className="text-xs text-muted-foreground">No responses yet.</p>
                ) : (
                  discoveredDevices.map((d, idx) => (
                    <div key={`${d.ip}-${idx}`} className="rounded-lg border border-border bg-slate-900/60 p-2 text-xs">
                      <div className="flex items-center justify-between gap-2">
                        <div className="min-w-0">
                          <p className="font-semibold truncate">{d.name || d.ip || "Unknown"}</p>
                          <p className="text-[11px] text-muted-foreground truncate">
                            {d.ip}:{d.wsPort || DEFAULT_PORT} {d.version ? `· v${d.version}` : ""}
                          </p>
                          <p className="text-[10px] text-slate-400 truncate">
                            Last seen {d.lastSeenMs ? new Date(d.lastSeenMs).toLocaleTimeString() : "just now"}
                          </p>
                        </div>
                        <div className="flex items-center gap-1 shrink-0">
                          <button
                            onClick={() => handleAddDiscovered(d)}
                            className="inline-flex items-center gap-1 px-2 py-1 rounded-md bg-primary/20 text-[11px] hover:bg-primary/30"
                          >
                            Use
                          </button>
                          <button
                            onClick={() => handleConnectDiscovered(d)}
                            className="inline-flex items-center gap-1 px-2 py-1 rounded-md bg-emerald-600/20 text-[11px] hover:bg-emerald-500/30 border border-emerald-500/30"
                          >
                            Connect
                          </button>
                        </div>
                      </div>
                    </div>
                  ))
                )}
              </div>
              <p className="text-[11px] text-muted-foreground">
                Broadcasts on UDP {DISCOVERY_PORT}. No port scans performed.
              </p>
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

            {/* Hotkeys */}
            <div className="bg-secondary/50 border border-border rounded-2xl p-4 space-y-3 shadow-lg">
              <div className="flex items-center justify-between gap-2">
                <div className="flex items-center gap-2">
                  <Keyboard className="w-4 h-4 text-primary" />
                  <span className="text-sm font-medium">Hotkeys</span>
                </div>
                <button
                  onClick={() => setHotkeysEnabled((v) => !v)}
                  className={`relative inline-flex h-6 w-11 items-center rounded-full transition ${
                    hotkeysEnabled ? "bg-primary" : "bg-slate-700"
                  }`}
                >
                  <span
                    className={`inline-block h-4 w-4 transform rounded-full bg-white transition-transform ${
                      hotkeysEnabled ? "translate-x-6" : "translate-x-1"
                    }`}
                  />
                </button>
              </div>
              <p className="text-[11px] text-muted-foreground">
                Hotkeys are captured only while the input is focused. Global hotkeys work when enabled and you are not typing
                in a field.
              </p>
              <div className="space-y-2">
                <HotkeyRow action="connect" label="Connect active" />
                <HotkeyRow action="reset" label="Reset / Cancel All" />
                <HotkeyRow action="stream" label="Start screen stream" />
                <HotkeyRow action="processes" label="Refresh processes" />
              </div>
              <div className="flex justify-end gap-2 text-[11px] text-muted-foreground">
                <button
                  onClick={resetHotkeys}
                  className="px-2 py-1 rounded-lg border border-border bg-slate-900/60 hover:bg-slate-800"
                >
                  Reset defaults
                </button>
              </div>
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
                          <div className="flex items-center gap-1 text-[10px] text-muted-foreground mt-1">
                            <Shield className="w-3 h-3 text-slate-400" />
                            {t.authStatus === "ok" ? (
                              <span className="text-emerald-200">Token verified</span>
                            ) : t.authStatus === "failed" ? (
                              <span className="text-rose-200">Auth failed</span>
                            ) : (
                              <span>Awaiting auth</span>
                            )}
                          </div>
                          <div className="flex items-center gap-1 text-[10px] mt-1">
                            <Activity className="w-3 h-3 text-slate-400" />
                            <span
                              className={
                                t.activity.state === "error"
                                  ? "text-rose-300"
                                  : t.activity.state === "running"
                                  ? "text-amber-200"
                                  : "text-emerald-200"
                              }
                            >
                              {t.activity.label}
                            </span>
                          </div>
                          {t.lastError ? (
                            <p className="text-[10px] text-rose-200 truncate mt-0.5">Last error: {t.lastError}</p>
                          ) : null}
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
                        <span className="ml-2 text-[11px]">
                          · {active.activity.label}
                        </span>
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
                            disabled={actionsDisabled || isPendingForActive("screen")}
                            className={`inline-flex items-center gap-2 px-3 py-2 rounded-lg
                                      bg-slate-900/60 border border-border transition text-sm
                                      ${actionsDisabled || isPendingForActive("screen") ? "opacity-50 cursor-not-allowed" : "hover:bg-slate-800"}`}
                          >
                            <Monitor className="w-4 h-4" /> Screen
                          </button>

                          <button
                            onClick={actionCamera}
                            disabled={actionsDisabled || isPendingForActive("camera")}
                            className={`inline-flex items-center gap-2 px-3 py-2 rounded-lg
                                      bg-slate-900/60 border border-border transition text-sm
                                      ${actionsDisabled || isPendingForActive("camera") ? "opacity-50 cursor-not-allowed" : "hover:bg-slate-800"}`}
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
                              disabled={actionsDisabled || isPendingForActive("camera_video")}
                              className={`inline-flex items-center gap-2 px-2 py-1 rounded-md
                                        bg-primary/20 transition text-sm
                                        ${actionsDisabled || isPendingForActive("camera_video") ? "opacity-50 cursor-not-allowed" : "hover:bg-primary/30"}`}
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
                              disabled={actionsDisabled || isPendingForActive("screen_stream") || active.stream.running}
                              className={`inline-flex items-center gap-2 px-2 py-1 rounded-md
                                        bg-primary/20 transition text-sm
                                        ${actionsDisabled || isPendingForActive("screen_stream") || active.stream.running ? "opacity-50 cursor-not-allowed" : "hover:bg-primary/30"}`}
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
                            disabled={actionsDisabled || isPendingForActive("process_list")}
                            className={`inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border transition text-sm ${
                              actionsDisabled || isPendingForActive("process_list") ? "opacity-50 cursor-not-allowed" : "hover:bg-slate-800"
                            }`}
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
                            disabled={actionsDisabled || isPendingForActive("ping")}
                            className={`inline-flex items-center gap-2 px-3 py-2 rounded-lg bg-slate-900/60 border border-border transition text-sm ${
                              actionsDisabled || isPendingForActive("ping") ? "opacity-50 cursor-not-allowed" : "hover:bg-slate-800"
                            }`}
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
                          disabled={actionsDisabled || isPendingForActive("process_start")}
                          className={`inline-flex items-center gap-2 px-2 py-1 rounded-md bg-primary/20 transition text-sm ${
                            actionsDisabled || isPendingForActive("process_start") ? "opacity-50 cursor-not-allowed" : "hover:bg-primary/30"
                          }`}
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
                          disabled={actionsDisabled}
                          className={`inline-flex items-center gap-2 px-2 py-1 rounded-md bg-primary/20 transition text-sm ${
                            actionsDisabled ? "opacity-50 cursor-not-allowed" : "hover:bg-primary/30"
                          }`}
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
                                      disabled={actionsDisabled}
                                      className={`h-6 w-6 rounded-lg border border-border bg-slate-900/60 ${
                                        actionsDisabled ? "opacity-50 cursor-not-allowed" : "hover:bg-slate-800"
                                      }`}
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

    {pendingControllerAction ? (
      <div className="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center z-20 px-4">
        <div className="w-full max-w-sm rounded-2xl bg-slate-900 border border-border p-5 space-y-3 shadow-2xl">
          <div className="flex items-center gap-2">
            <Power className="w-5 h-5 text-primary" />
            <div>
              <p className="text-lg font-semibold text-slate-50 capitalize">
                {pendingControllerAction} controller?
              </p>
              <p className="text-xs text-muted-foreground">
                This only affects the controller process, not the OS. Active streams will stop.
              </p>
            </div>
          </div>
          {controllerStatus?.pid ? (
            <p className="text-[11px] text-muted-foreground">Current PID: {controllerStatus.pid}</p>
          ) : null}
          <div className="flex items-center justify-end gap-2">
            <button
              onClick={() => setPendingControllerAction(null)}
              className="px-3 py-2 rounded-lg border border-border text-sm hover:bg-slate-800"
            >
              Cancel
            </button>
            <button
              onClick={confirmControllerAction}
              disabled={controllerBusy}
              className="px-3 py-2 rounded-lg bg-primary text-sm font-semibold text-slate-50 hover:opacity-90 disabled:opacity-60"
            >
              Confirm
            </button>
          </div>
        </div>
      </div>
    ) : null}

    {toast ? (
      <div
        className={`fixed bottom-6 right-6 z-30 rounded-lg border px-4 py-2 text-sm shadow-lg ${
          toast.tone === "success"
            ? "border-emerald-400/40 bg-emerald-500/20 text-emerald-100"
            : toast.tone === "error"
            ? "border-rose-400/40 bg-rose-500/20 text-rose-100"
            : "border-slate-500/40 bg-slate-800/70 text-slate-100"
        }`}
      >
        {toast.text}
      </div>
    ) : null}
    </>
  );
}
