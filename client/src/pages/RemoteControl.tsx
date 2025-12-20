import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  ArrowDownLeft,
  Copy,
  Gauge,
  MonitorPlay,
  MousePointer2,
  ScreenShare,
  ShieldCheck,
  ShieldOff,
  StopCircle,
  Video,
  Wifi,
  WifiOff,
} from "lucide-react";
import type {
  ControlMessage,
  ControlStatus,
  ConnectionStatus,
  PeerStatus,
  WebRtcMessage,
  WebRtcRole,
} from "../types";

const STUN_SERVERS: RTCIceServer[] = [{ urls: "stun:stun.l.google.com:19302" }];
const JOIN_TIMEOUT_MS = 10000;
const NEGOTIATION_TIMEOUT_MS = 15000;
const CONTROL_TIMEOUT_MS = 10000;

const QUALITY_PRESETS = {
  low: { label: "Low", constraints: { width: 1280, height: 720, frameRate: 15 } },
  medium: { label: "Medium", constraints: { width: 1600, height: 900, frameRate: 25 } },
  high: { label: "High", constraints: { width: 1920, height: 1080, frameRate: 30 } },
} as const;

type QualityPreset = keyof typeof QUALITY_PRESETS;

type ToastTone = "info" | "success" | "error";

const createRoomId = () => Math.random().toString(36).slice(2, 8).toUpperCase();

function normalizeRoomId(value: string) {
  return value.trim().toUpperCase();
}

function useTimeouts() {
  const timers = useRef<number[]>([]);
  const add = (handler: () => void, ms: number) => {
    const id = window.setTimeout(handler, ms);
    timers.current.push(id);
    return id;
  };
  const clearAll = () => {
    timers.current.forEach((id) => window.clearTimeout(id));
    timers.current = [];
  };
  return { add, clearAll };
}

export default function RemoteControl() {
  const [wsStatus, setWsStatus] = useState<ConnectionStatus>("disconnected");
  const [peerStatus, setPeerStatus] = useState<PeerStatus>("new");
  const [controlStatus, setControlStatus] = useState<ControlStatus>("not_requested");
  const [role, setRole] = useState<WebRtcRole | null>(null);
  const [hostRoomId] = useState(() => createRoomId());
  const [roomIdInput, setRoomIdInput] = useState("");
  const [quality, setQuality] = useState<QualityPreset>("medium");
  const [showCursorOverlay, setShowCursorOverlay] = useState(true);
  const [captureMouse, setCaptureMouse] = useState(true);
  const [captureKeyboard, setCaptureKeyboard] = useState(true);
  const [relativeMouse, setRelativeMouse] = useState(false);
  const [fitMode, setFitMode] = useState<"contain" | "cover">("contain");
  const [toast, setToast] = useState<{ text: string; tone: ToastTone } | null>(null);
  const [hostBusy, setHostBusy] = useState(false);
  const [viewerBusy, setViewerBusy] = useState(false);
  const [controlBusy, setControlBusy] = useState(false);
  const [cursorPosition, setCursorPosition] = useState<{ x: number; y: number } | null>(null);

  const wsRef = useRef<WebSocket | null>(null);
  const pcRef = useRef<RTCPeerConnection | null>(null);
  const dataChannelRef = useRef<RTCDataChannel | null>(null);
  const localStreamRef = useRef<MediaStream | null>(null);
  const pendingCandidatesRef = useRef<RTCIceCandidateInit[]>([]);
  const videoRef = useRef<HTMLVideoElement | null>(null);
  const videoWrapRef = useRef<HTMLDivElement | null>(null);
  const lastMouseSentRef = useRef(0);
  const lastWheelSentRef = useRef(0);
  const pendingMoveRef = useRef<{ x: number; y: number } | null>(null);
  const rafRef = useRef<number | null>(null);
  const cursorPosRef = useRef<{ x: number; y: number } | null>(null);
  const controlStatusRef = useRef(controlStatus);
  const { add: addTimeout, clearAll: clearTimeouts } = useTimeouts();

  const wsUrl = useMemo(() => {
    const env = import.meta.env.VITE_WS_URL as string | undefined;
    if (env && env.length > 0) return env;
    return `ws://${window.location.hostname}:9002`;
  }, []);

  const showToast = useCallback((text: string, tone: ToastTone = "info") => {
    setToast({ text, tone });
    window.setTimeout(() => setToast(null), 4000);
  }, []);

  useEffect(() => {
    controlStatusRef.current = controlStatus;
  }, [controlStatus]);

  const sendWs = useCallback((payload: WebRtcMessage | Record<string, unknown>) => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) {
      showToast("WebSocket not connected", "error");
      return;
    }
    wsRef.current.send(JSON.stringify(payload));
  }, [showToast]);

  const updatePeerStatusFromConnection = useCallback((pc: RTCPeerConnection) => {
    const state = pc.connectionState;
    if (state === "connected") {
      setPeerStatus("connected");
      return;
    }
    if (state === "connecting" || state === "new") {
      setPeerStatus("connecting");
      return;
    }
    if (state === "failed") {
      setPeerStatus("failed");
      return;
    }
    if (state === "disconnected" || state === "closed") {
      setPeerStatus("disconnected");
    }
  }, []);

  const closeDataChannel = useCallback(() => {
    if (dataChannelRef.current) {
      dataChannelRef.current.onmessage = null;
      dataChannelRef.current.onopen = null;
      dataChannelRef.current.onclose = null;
      dataChannelRef.current.close();
      dataChannelRef.current = null;
    }
  }, []);

  const cleanupPeer = useCallback(() => {
    clearTimeouts();
    closeDataChannel();

    if (pcRef.current) {
      pcRef.current.onicecandidate = null;
      pcRef.current.ontrack = null;
      pcRef.current.onconnectionstatechange = null;
      pcRef.current.oniceconnectionstatechange = null;
      pcRef.current.close();
      pcRef.current = null;
    }

    if (localStreamRef.current) {
      localStreamRef.current.getTracks().forEach((track) => track.stop());
      localStreamRef.current = null;
    }

    if (videoRef.current) {
      videoRef.current.srcObject = null;
    }

    pendingCandidatesRef.current = [];
    pendingMoveRef.current = null;
    if (rafRef.current) {
      window.cancelAnimationFrame(rafRef.current);
      rafRef.current = null;
    }

    setPeerStatus("new");
    setControlStatus("not_requested");
    setControlBusy(false);
    setCursorPosition(null);
  }, [clearTimeouts, closeDataChannel]);

  const buildPeerConnection = useCallback((currentRole: WebRtcRole, roomId: string) => {
    const pc = new RTCPeerConnection({ iceServers: STUN_SERVERS });
    pc.onicecandidate = (event) => {
      if (event.candidate) {
        sendWs({
          type: "webrtc",
          roomId,
          role: currentRole,
          action: "signal",
          data: { candidate: event.candidate.toJSON() },
        });
      }
    };
    pc.onconnectionstatechange = () => updatePeerStatusFromConnection(pc);
    pc.oniceconnectionstatechange = () => {
      if (pc.iceConnectionState === "failed" || pc.iceConnectionState === "disconnected") {
        showToast("Peer disconnected", "error");
        cleanupPeer();
      }
    };
    return pc;
  }, [cleanupPeer, sendWs, showToast, updatePeerStatusFromConnection]);

  const handleSignalMessage = useCallback(async (message: WebRtcMessage) => {
    const pc = pcRef.current;
    if (!pc || !message.data) return;

    if (message.data.sdp) {
      const description = new RTCSessionDescription(message.data.sdp);
      await pc.setRemoteDescription(description);
      if (message.data.sdp.type === "offer") {
        const answer = await pc.createAnswer();
        await pc.setLocalDescription(answer);
        sendWs({
          type: "webrtc",
          roomId: message.roomId,
          role: "viewer",
          action: "signal",
          data: { sdp: pc.localDescription },
        });
      }

      if (pendingCandidatesRef.current.length > 0) {
        for (const candidate of pendingCandidatesRef.current) {
          await pc.addIceCandidate(candidate);
        }
        pendingCandidatesRef.current = [];
      }
    }

    if (message.data.candidate) {
      if (pc.remoteDescription) {
        await pc.addIceCandidate(message.data.candidate);
      } else {
        pendingCandidatesRef.current.push(message.data.candidate);
      }
    }
  }, [sendWs]);

  const startHostNegotiation = useCallback(async () => {
    if (!pcRef.current || role !== "host") return;
    try {
      const offer = await pcRef.current.createOffer();
      await pcRef.current.setLocalDescription(offer);
      sendWs({
        type: "webrtc",
        roomId: hostRoomId,
        role: "host",
        action: "signal",
        data: { sdp: pcRef.current.localDescription },
      });
      addTimeout(() => {
        setPeerStatus("failed");
        showToast("Negotiation timed out", "error");
        cleanupPeer();
      }, NEGOTIATION_TIMEOUT_MS);
    } catch (error) {
      showToast("Failed to negotiate", "error");
      cleanupPeer();
    }
  }, [addTimeout, cleanupPeer, hostRoomId, role, sendWs, showToast]);

  const connectWebSocket = useCallback(() => {
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      return Promise.resolve();
    }
    if (wsRef.current && wsRef.current.readyState === WebSocket.CONNECTING) {
      return new Promise<void>((resolve, reject) => {
        const interval = window.setInterval(() => {
          if (!wsRef.current) {
            window.clearInterval(interval);
            reject(new Error("WebSocket closed"));
          } else if (wsRef.current.readyState === WebSocket.OPEN) {
            window.clearInterval(interval);
            resolve();
          }
        }, 200);
      });
    }

    return new Promise<void>((resolve, reject) => {
      setWsStatus("connecting");
      const ws = new WebSocket(wsUrl);
      wsRef.current = ws;

      ws.onopen = () => {
        setWsStatus("connected");
        resolve();
      };

      ws.onerror = () => {
        setWsStatus("disconnected");
        reject(new Error("WebSocket error"));
      };

      ws.onclose = () => {
        setWsStatus("disconnected");
        cleanupPeer();
        setRole(null);
      };

      ws.onmessage = (event) => {
        let message: WebRtcMessage | null = null;
        try {
          message = JSON.parse(event.data) as WebRtcMessage;
        } catch (error) {
          showToast("Invalid signaling message", "error");
        }
        if (!message || message.type !== "webrtc") return;

        if (message.action === "joined") {
          if (message.role === "host") {
            setHostBusy(false);
          } else {
            setViewerBusy(false);
          }
          return;
        }

        if (message.action === "peer-ready") {
          if (message.role === "host") {
            void startHostNegotiation();
          }
          return;
        }

        if (message.action === "peer-left") {
          showToast("Peer left the room", "error");
          cleanupPeer();
          return;
        }

        if (message.action === "signal") {
          void handleSignalMessage(message);
          return;
        }

        if (message.action === "error") {
          showToast(message.message ?? "Signaling error", "error");
        }
      };
    });
  }, [cleanupPeer, handleSignalMessage, showToast, startHostNegotiation, wsUrl]);

  const setupDataChannel = useCallback((channel: RTCDataChannel, localRole: WebRtcRole) => {
    dataChannelRef.current = channel;

    channel.onopen = () => {
      showToast("Control channel ready", "success");
    };

    channel.onclose = () => {
      setControlStatus("not_requested");
    };

    channel.onmessage = (event) => {
      let payload: ControlMessage | null = null;
      try {
        payload = JSON.parse(event.data) as ControlMessage;
      } catch (error) {
        return;
      }
      if (!payload || typeof payload.t !== "string") return;

      if (payload.t === "control-request") {
        if (localRole === "host") {
          setControlStatus("pending");
          showToast("Control request received", "info");
        }
        return;
      }

      if (payload.t === "control-granted") {
        setControlStatus("granted");
        setControlBusy(false);
        showToast("Control granted", "success");
        return;
      }

      if (payload.t === "control-revoked") {
        setControlStatus("revoked");
        setControlBusy(false);
        showToast("Control revoked", "error");
        return;
      }

      if (payload.t === "mouse" || payload.t === "key") {
        if (localRole !== "host" || controlStatus !== "granted") return;
        if (payload.t === "mouse") {
          sendWs({
            cmd: "input-event",
            kind: "mouse",
            action: payload.action,
            x: payload.x,
            y: payload.y,
            button: payload.button,
            deltaY: payload.deltaY,
          });
        } else {
          sendWs({
            cmd: "input-event",
            kind: "key",
            action: payload.action,
            code: payload.code,
            key: payload.key,
            ctrl: payload.ctrl,
            alt: payload.alt,
            shift: payload.shift,
            meta: payload.meta,
          });
        }
      }
    };
  }, [controlStatus, sendWs, showToast]);

  const startHosting = useCallback(async () => {
    if (hostBusy || role) return;
    setHostBusy(true);
    setPeerStatus("connecting");
    try {
      await connectWebSocket();
      const stream = await navigator.mediaDevices.getDisplayMedia({
        video: QUALITY_PRESETS[quality].constraints,
        audio: false,
      });
      localStreamRef.current = stream;
      const pc = buildPeerConnection("host", hostRoomId);
      pcRef.current = pc;
      stream.getTracks().forEach((track) => pc.addTrack(track, stream));
      const channel = pc.createDataChannel("control");
      setupDataChannel(channel, "host");
      setRole("host");
      sendWs({ type: "webrtc", roomId: hostRoomId, role: "host", action: "join" });
      addTimeout(() => {
        setHostBusy(false);
        setPeerStatus("failed");
        showToast("Host join timed out", "error");
        cleanupPeer();
      }, JOIN_TIMEOUT_MS);
    } catch (error) {
      setHostBusy(false);
      setPeerStatus("failed");
      showToast("Failed to start hosting", "error");
      cleanupPeer();
    }
  }, [addTimeout, buildPeerConnection, cleanupPeer, connectWebSocket, hostBusy, hostRoomId, quality, role, sendWs, setupDataChannel, showToast]);

  const stopHosting = useCallback(() => {
    if (role !== "host") return;
    setHostBusy(true);
    sendWs({ type: "webrtc", roomId: hostRoomId, role: "host", action: "leave" });
    cleanupPeer();
    setRole(null);
    setHostBusy(false);
  }, [cleanupPeer, hostRoomId, role, sendWs]);

  const connectViewer = useCallback(async () => {
    if (viewerBusy || role) return;
    const room = normalizeRoomId(roomIdInput);
    if (!room) {
      showToast("Enter a room ID", "error");
      return;
    }
    setViewerBusy(true);
    setPeerStatus("connecting");
    try {
      await connectWebSocket();
      const pc = buildPeerConnection("viewer", room);
      pcRef.current = pc;
      pc.ontrack = (event) => {
        if (videoRef.current) {
          videoRef.current.srcObject = event.streams[0];
        }
      };
      pc.ondatachannel = (event) => {
        setupDataChannel(event.channel, "viewer");
      };
      setRole("viewer");
      sendWs({ type: "webrtc", roomId: room, role: "viewer", action: "join" });
      addTimeout(() => {
        setViewerBusy(false);
        setPeerStatus("failed");
        showToast("Viewer join timed out", "error");
        cleanupPeer();
      }, JOIN_TIMEOUT_MS);
    } catch (error) {
      setViewerBusy(false);
      setPeerStatus("failed");
      showToast("Failed to connect as viewer", "error");
      cleanupPeer();
    }
  }, [addTimeout, buildPeerConnection, cleanupPeer, connectWebSocket, roomIdInput, role, sendWs, setupDataChannel, showToast, viewerBusy]);

  const disconnectViewer = useCallback(() => {
    if (role !== "viewer") return;
    const room = normalizeRoomId(roomIdInput);
    setViewerBusy(true);
    sendWs({ type: "webrtc", roomId: room, role: "viewer", action: "leave" });
    cleanupPeer();
    setRole(null);
    setViewerBusy(false);
  }, [cleanupPeer, roomIdInput, role, sendWs]);

  const sendControlMessage = useCallback((message: ControlMessage) => {
    if (!dataChannelRef.current || dataChannelRef.current.readyState !== "open") {
      showToast("Control channel not open", "error");
      return;
    }
    dataChannelRef.current.send(JSON.stringify(message));
  }, [showToast]);

  const requestControl = useCallback(() => {
    if (controlBusy || role !== "viewer") return;
    setControlBusy(true);
    setControlStatus("pending");
    sendControlMessage({ t: "control-request" });
    addTimeout(() => {
      if (controlStatusRef.current === "pending") {
        setControlStatus("revoked");
        setControlBusy(false);
        showToast("Control request timed out", "error");
      }
    }, CONTROL_TIMEOUT_MS);
  }, [addTimeout, controlBusy, role, sendControlMessage, showToast]);

  const releaseControl = useCallback(() => {
    if (role !== "viewer") return;
    setControlBusy(true);
    sendControlMessage({ t: "control-revoked" });
    setControlStatus("revoked");
    setControlBusy(false);
  }, [role, sendControlMessage]);

  const grantControl = useCallback(() => {
    if (role !== "host" || controlStatus !== "pending") return;
    sendControlMessage({ t: "control-granted" });
    setControlStatus("granted");
  }, [controlStatus, role, sendControlMessage]);

  const revokeControl = useCallback(() => {
    if (role !== "host") return;
    sendControlMessage({ t: "control-revoked" });
    setControlStatus("revoked");
  }, [role, sendControlMessage]);

  const sendMouseMove = useCallback((clientX: number, clientY: number, rect: DOMRect) => {
    const x = Math.min(Math.max((clientX - rect.left) / rect.width, 0), 1);
    const y = Math.min(Math.max((clientY - rect.top) / rect.height, 0), 1);
    if (relativeMouse && cursorPosRef.current) {
      const dx = (clientX - (rect.left + rect.width * cursorPosRef.current.x)) / rect.width;
      const dy = (clientY - (rect.top + rect.height * cursorPosRef.current.y)) / rect.height;
      cursorPosRef.current = {
        x: Math.min(Math.max(cursorPosRef.current.x + dx, 0), 1),
        y: Math.min(Math.max(cursorPosRef.current.y + dy, 0), 1),
      };
    } else {
      cursorPosRef.current = { x, y };
    }
    setCursorPosition(cursorPosRef.current);

    lastMouseSentRef.current = performance.now();
    sendControlMessage({
      t: "mouse",
      action: "move",
      x: cursorPosRef.current?.x ?? x,
      y: cursorPosRef.current?.y ?? y,
    });
  }, [relativeMouse, sendControlMessage]);

  const handleMouseMove = useCallback((event: MouseEvent) => {
    if (!captureMouse || controlStatus !== "granted") return;
    const target = videoWrapRef.current;
    if (!target) return;
    const rect = target.getBoundingClientRect();
    const now = performance.now();
    const maxFps = 60;
    if (now - lastMouseSentRef.current < 1000 / maxFps) {
      pendingMoveRef.current = { x: event.clientX, y: event.clientY };
      if (!rafRef.current) {
        rafRef.current = window.requestAnimationFrame(() => {
          rafRef.current = null;
          if (pendingMoveRef.current) {
            const { x, y } = pendingMoveRef.current;
            pendingMoveRef.current = null;
            sendMouseMove(x, y, rect);
          }
        });
      }
      return;
    }
    sendMouseMove(event.clientX, event.clientY, rect);
  }, [captureMouse, controlStatus, sendMouseMove]);

  const handleMouseButton = useCallback((event: MouseEvent, action: "down" | "up") => {
    if (!captureMouse || controlStatus !== "granted") return;
    const buttonMap: Record<number, "left" | "middle" | "right"> = {
      0: "left",
      1: "middle",
      2: "right",
    };
    const button = buttonMap[event.button];
    if (!button) return;
    sendControlMessage({ t: "mouse", action, button });
  }, [captureMouse, controlStatus, sendControlMessage]);

  const handleWheel = useCallback((event: WheelEvent) => {
    if (!captureMouse || controlStatus !== "granted") return;
    const now = performance.now();
    if (now - lastWheelSentRef.current < 1000 / 30) return;
    lastWheelSentRef.current = now;
    sendControlMessage({ t: "mouse", action: "wheel", deltaY: event.deltaY });
  }, [captureMouse, controlStatus, sendControlMessage]);

  const handleKey = useCallback((event: KeyboardEvent, action: "down" | "up") => {
    if (!captureKeyboard || controlStatus !== "granted") return;
    if (action === "down" && event.repeat) return;
    sendControlMessage({
      t: "key",
      action,
      code: event.code,
      key: event.key,
      ctrl: event.ctrlKey,
      alt: event.altKey,
      shift: event.shiftKey,
      meta: event.metaKey,
    });
  }, [captureKeyboard, controlStatus, sendControlMessage]);

  const toggleFullscreen = useCallback(() => {
    const el = videoWrapRef.current;
    if (!el) return;
    if (document.fullscreenElement) {
      void document.exitFullscreen();
      return;
    }
    void el.requestFullscreen();
  }, []);

  useEffect(() => {
    void connectWebSocket();
    return () => {
      wsRef.current?.close();
    };
  }, [connectWebSocket]);

  useEffect(() => {
    if (role !== "viewer") return;
    const target = videoWrapRef.current;
    if (!target) return;

    const onMove = (event: MouseEvent) => handleMouseMove(event);
    const onDown = (event: MouseEvent) => handleMouseButton(event, "down");
    const onUp = (event: MouseEvent) => handleMouseButton(event, "up");
    const onWheel = (event: WheelEvent) => handleWheel(event);

    target.addEventListener("mousemove", onMove);
    target.addEventListener("mousedown", onDown);
    target.addEventListener("mouseup", onUp);
    target.addEventListener("wheel", onWheel, { passive: true });

    const onKeyDown = (event: KeyboardEvent) => handleKey(event, "down");
    const onKeyUp = (event: KeyboardEvent) => handleKey(event, "up");
    window.addEventListener("keydown", onKeyDown);
    window.addEventListener("keyup", onKeyUp);

    return () => {
      target.removeEventListener("mousemove", onMove);
      target.removeEventListener("mousedown", onDown);
      target.removeEventListener("mouseup", onUp);
      target.removeEventListener("wheel", onWheel);
      window.removeEventListener("keydown", onKeyDown);
      window.removeEventListener("keyup", onKeyUp);
    };
  }, [handleKey, handleMouseButton, handleMouseMove, handleWheel, role]);

  const statusPill = (status: ConnectionStatus | PeerStatus | ControlStatus) => {
    const base = "px-3 py-1 rounded-full text-xs font-semibold border";
    if (status === "connected" || status === "granted") {
      return `${base} border-emerald-400/60 bg-emerald-500/10 text-emerald-100`;
    }
    if (status === "connecting" || status === "pending") {
      return `${base} border-amber-400/60 bg-amber-500/10 text-amber-100`;
    }
    if (status === "failed" || status === "revoked" || status === "disconnected") {
      return `${base} border-rose-400/60 bg-rose-500/10 text-rose-100`;
    }
    return `${base} border-border bg-slate-900/40 text-muted-foreground`;
  };

  return (
    <div className="flex flex-col gap-4 flex-1 min-h-0">
      {toast ? (
        <div
          className={`px-4 py-3 rounded-xl border text-sm shadow-lg flex items-center gap-2 ${
            toast.tone === "success"
              ? "border-emerald-400/60 bg-emerald-500/10 text-emerald-100"
              : toast.tone === "error"
              ? "border-rose-400/60 bg-rose-500/10 text-rose-100"
              : "border-blue-400/60 bg-blue-500/10 text-blue-100"
          }`}
        >
          {toast.text}
        </div>
      ) : null}

      {controlStatus === "granted" && role === "host" ? (
        <div className="px-4 py-3 rounded-xl border border-amber-400/70 bg-amber-500/10 text-amber-100 flex items-center justify-between">
          <div className="flex items-center gap-2">
            <ShieldCheck className="w-4 h-4" />
            Remote control active
          </div>
          <button
            onClick={revokeControl}
            className="px-3 py-2 rounded-lg bg-rose-500/90 text-white text-sm font-semibold hover:bg-rose-500"
          >
            Stop control
          </button>
        </div>
      ) : null}

      <div className="grid grid-cols-1 xl:grid-cols-[360px_1fr] gap-4 flex-1 min-h-0">
        <div className="flex flex-col gap-4 min-h-0">
          <div className="bg-secondary/50 border border-border rounded-2xl p-4 space-y-3 shadow-lg">
            <div className="flex items-center gap-2 text-sm font-medium">
              <Gauge className="w-4 h-4 text-primary" /> Connection status
            </div>
            <div className="grid grid-cols-1 gap-2 text-sm">
              <div className="flex items-center justify-between">
                <span className="text-muted-foreground">WebSocket</span>
                <span className={statusPill(wsStatus)}>{wsStatus}</span>
              </div>
              <div className="flex items-center justify-between">
                <span className="text-muted-foreground">WebRTC</span>
                <span className={statusPill(peerStatus)}>{peerStatus}</span>
              </div>
              <div className="flex items-center justify-between">
                <span className="text-muted-foreground">Control</span>
                <span className={statusPill(controlStatus)}>{controlStatus.replace("_", " ")}</span>
              </div>
            </div>
          </div>

          <div className="bg-secondary/50 border border-border rounded-2xl p-4 space-y-4 shadow-lg">
            <div className="flex items-center gap-2 text-sm font-medium">
              <ScreenShare className="w-4 h-4 text-primary" /> Host session
            </div>
            <div className="space-y-2">
              <label className="text-xs text-muted-foreground">Room ID</label>
              <div className="flex items-center gap-2">
                <input
                  value={hostRoomId}
                  readOnly
                  className="flex-1 px-3 py-2 rounded-lg bg-slate-900/60 border border-border text-sm"
                />
                <button
                  onClick={() => {
                    navigator.clipboard.writeText(hostRoomId).then(() => {
                      showToast("Room ID copied", "success");
                    });
                  }}
                  className="p-2 rounded-lg border border-border bg-slate-900/60 hover:bg-slate-800"
                >
                  <Copy className="w-4 h-4" />
                </button>
              </div>
            </div>
            <div className="space-y-2">
              <label className="text-xs text-muted-foreground">Quality preset</label>
              <div className="grid grid-cols-3 gap-2">
                {(Object.keys(QUALITY_PRESETS) as QualityPreset[]).map((key) => (
                  <button
                    key={key}
                    onClick={() => setQuality(key)}
                    className={`px-3 py-2 rounded-lg text-sm border ${
                      quality === key
                        ? "border-primary bg-primary/20 text-primary"
                        : "border-border bg-slate-900/60 text-muted-foreground"
                    }`}
                    disabled={role === "host"}
                  >
                    {QUALITY_PRESETS[key].label}
                  </button>
                ))}
              </div>
            </div>
            <div className="flex items-center justify-between text-sm">
              <span className="text-muted-foreground">Show cursor overlay</span>
              <button
                onClick={() => setShowCursorOverlay((prev) => !prev)}
                className={`px-3 py-1.5 rounded-full border text-xs ${
                  showCursorOverlay
                    ? "border-emerald-400/60 bg-emerald-500/10 text-emerald-100"
                    : "border-border bg-slate-900/60 text-muted-foreground"
                }`}
              >
                {showCursorOverlay ? "On" : "Off"}
              </button>
            </div>
            <div className="flex items-center gap-2">
              <button
                onClick={startHosting}
                disabled={hostBusy || role !== null}
                className="flex-1 inline-flex items-center justify-center gap-2 px-3 py-2 rounded-lg bg-primary text-white text-sm font-semibold disabled:opacity-60"
              >
                <MonitorPlay className="w-4 h-4" /> Start hosting
              </button>
              <button
                onClick={stopHosting}
                disabled={hostBusy || role !== "host"}
                className="flex-1 inline-flex items-center justify-center gap-2 px-3 py-2 rounded-lg bg-rose-500/80 text-white text-sm font-semibold disabled:opacity-60"
              >
                <StopCircle className="w-4 h-4" /> Stop
              </button>
            </div>
            {role === "host" ? (
              <div className="space-y-2">
                <div className="text-xs text-muted-foreground">Control approvals</div>
                <div className="flex gap-2">
                  <button
                    onClick={grantControl}
                    disabled={controlStatus !== "pending"}
                    className="flex-1 inline-flex items-center justify-center gap-2 px-3 py-2 rounded-lg border border-emerald-400/60 text-emerald-100 text-sm disabled:opacity-60"
                  >
                    <ShieldCheck className="w-4 h-4" /> Grant control
                  </button>
                  <button
                    onClick={revokeControl}
                    disabled={controlStatus === "not_requested"}
                    className="flex-1 inline-flex items-center justify-center gap-2 px-3 py-2 rounded-lg border border-rose-400/60 text-rose-100 text-sm disabled:opacity-60"
                  >
                    <ShieldOff className="w-4 h-4" /> Revoke
                  </button>
                </div>
              </div>
            ) : null}
          </div>

          <div className="bg-secondary/50 border border-border rounded-2xl p-4 space-y-4 shadow-lg">
            <div className="flex items-center gap-2 text-sm font-medium">
              <Video className="w-4 h-4 text-primary" /> Viewer session
            </div>
            <div className="space-y-2">
              <label className="text-xs text-muted-foreground">Room ID</label>
              <input
                value={roomIdInput}
                onChange={(event) => setRoomIdInput(event.target.value)}
                placeholder="Enter host room ID"
                className="w-full px-3 py-2 rounded-lg bg-slate-900/60 border border-border text-sm"
              />
            </div>
            <div className="flex items-center gap-2">
              <button
                onClick={connectViewer}
                disabled={viewerBusy || role !== null}
                className="flex-1 inline-flex items-center justify-center gap-2 px-3 py-2 rounded-lg bg-primary text-white text-sm font-semibold disabled:opacity-60"
              >
                <Wifi className="w-4 h-4" /> Connect
              </button>
              <button
                onClick={disconnectViewer}
                disabled={viewerBusy || role !== "viewer"}
                className="flex-1 inline-flex items-center justify-center gap-2 px-3 py-2 rounded-lg bg-rose-500/80 text-white text-sm font-semibold disabled:opacity-60"
              >
                <WifiOff className="w-4 h-4" /> Disconnect
              </button>
            </div>
            <div className="flex items-center gap-2">
              <button
                onClick={requestControl}
                disabled={controlBusy || role !== "viewer" || controlStatus === "granted"}
                className="flex-1 inline-flex items-center justify-center gap-2 px-3 py-2 rounded-lg border border-emerald-400/60 text-emerald-100 text-sm disabled:opacity-60"
              >
                <ShieldCheck className="w-4 h-4" /> Request control
              </button>
              <button
                onClick={releaseControl}
                disabled={controlBusy || role !== "viewer" || controlStatus !== "granted"}
                className="flex-1 inline-flex items-center justify-center gap-2 px-3 py-2 rounded-lg border border-rose-400/60 text-rose-100 text-sm disabled:opacity-60"
              >
                <ShieldOff className="w-4 h-4" /> Release
              </button>
            </div>
            <div className="space-y-2 text-xs text-muted-foreground">
              <label className="flex items-center justify-between gap-2">
                <span className="flex items-center gap-2">
                  <MousePointer2 className="w-4 h-4" /> Capture mouse
                </span>
                <input
                  type="checkbox"
                  checked={captureMouse}
                  onChange={(event) => setCaptureMouse(event.target.checked)}
                />
              </label>
              <label className="flex items-center justify-between gap-2">
                <span className="flex items-center gap-2">
                  <ArrowDownLeft className="w-4 h-4" /> Capture keyboard
                </span>
                <input
                  type="checkbox"
                  checked={captureKeyboard}
                  onChange={(event) => setCaptureKeyboard(event.target.checked)}
                />
              </label>
              <label className="flex items-center justify-between gap-2">
                <span className="flex items-center gap-2">
                  <Gauge className="w-4 h-4" /> Relative mouse mode
                </span>
                <input
                  type="checkbox"
                  checked={relativeMouse}
                  onChange={(event) => setRelativeMouse(event.target.checked)}
                />
              </label>
            </div>
            <button
              onClick={toggleFullscreen}
              className="w-full inline-flex items-center justify-center gap-2 px-3 py-2 rounded-lg border border-border bg-slate-900/60 text-sm"
            >
              Fullscreen
            </button>
          </div>
        </div>

        <div className="bg-secondary/50 border border-border rounded-2xl p-4 shadow-lg flex flex-col gap-3 min-h-0">
          <div className="flex items-center justify-between text-sm">
            <div className="flex items-center gap-2 font-medium">
              <Video className="w-4 h-4 text-primary" /> Remote feed
            </div>
            <div className="flex items-center gap-2">
              <span className="text-xs text-muted-foreground">Fit</span>
              <button
                onClick={() => setFitMode((prev) => (prev === "contain" ? "cover" : "contain"))}
                className="px-2 py-1 rounded-full border border-border text-xs"
              >
                {fitMode === "contain" ? "Contain" : "Cover"}
              </button>
            </div>
          </div>
          <div
            ref={videoWrapRef}
            className="relative flex-1 rounded-xl border border-dashed border-slate-700/60 bg-slate-950/60 overflow-hidden"
            tabIndex={0}
          >
            {role === "viewer" ? (
              <video
                ref={videoRef}
                autoPlay
                playsInline
                className={`w-full h-full ${fitMode === "contain" ? "object-contain" : "object-cover"}`}
              />
            ) : (
              <div className="absolute inset-0 flex flex-col items-center justify-center text-sm text-muted-foreground gap-2">
                <Video className="w-6 h-6" />
                <span>Connect as viewer to see the stream</span>
              </div>
            )}
            {showCursorOverlay && cursorPosition && role === "viewer" ? (
              <div
                className="absolute w-3 h-3 rounded-full border border-white/70 bg-primary/70 pointer-events-none"
                style={{
                  left: `${(cursorPosition?.x ?? 0) * 100}%`,
                  top: `${(cursorPosition?.y ?? 0) * 100}%`,
                  transform: "translate(-50%, -50%)",
                }}
              />
            ) : null}
          </div>
        </div>
      </div>
    </div>
  );
}