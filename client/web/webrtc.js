// ============ DOM elements ============

const logEl = document.getElementById("log");
const imgRemote = document.getElementById("remoteScreenImg");

const btnConnectWs   = document.getElementById("btnConnectWs");
const btnStartWebrtc = document.getElementById("btnStartWebrtc");
const btnSnapshot    = document.getElementById("btnSnapshot");
const btnStartStream = document.getElementById("btnStartStream");
const btnStopStream  = document.getElementById("btnStopStream");

// Quality / FPS controls
const fpsInput        = document.getElementById("fpsInput");
const qualitySelect   = document.getElementById("qualitySelect");
const widthSelect     = document.getElementById("widthSelect");
const btnApplyQuality = document.getElementById("btnApplyQuality");

// Clipboard controls
const clipboardText   = document.getElementById("clipboardLocal");
const btnClipSend     = document.getElementById("btnClipboardSend");
const btnClipPull     = document.getElementById("btnClipboardPull");

// File explorer controls [NEW]
const filePathInput   = document.getElementById("filePath");
const btnListDir      = document.getElementById("btnListDir");
const btnGoUp         = document.getElementById("btnGoUp");
const fileErrorEl     = document.getElementById("fileError");
const fileTableBody   = document.querySelector("#fileTable tbody");

// ============ Config ============

// dùng Cloudflare tunnel hay localhost
const USE_CLOUDFLARE = true;

// CHÚ Ý: thay domain này mỗi khi bạn tạo tunnel mới
const WS_URL = USE_CLOUDFLARE
  ? "https://arabia-queensland-ids-amplifier.trycloudflare.com"
  : "ws://localhost:9002";

let ws = null;
let pc = null;
let inputChannel = null;

// buffer để ghép các chunk ảnh
const frameBuffer = new Map();

function log(msg) {
    const time = new Date().toISOString().substr(11, 8);
    logEl.textContent += [${time}] ${msg}\n;
    logEl.scrollTop = logEl.scrollHeight;
    console.log(msg);
}

// =========================
// 1. WebSocket
// =========================
btnConnectWs.onclick = () => {
    if (ws && ws.readyState === WebSocket.OPEN) {
        log("WebSocket already open");
        return;
    }

    log("Connecting WebSocket to " + WS_URL + " ...");
    ws = new WebSocket(WS_URL);

    ws.onopen = () => {
        log("WebSocket OPEN");
        btnStartWebrtc.disabled = false;
    };

    ws.onclose = () => {
        log("WebSocket CLOSED");
        btnStartWebrtc.disabled   = true;
        btnSnapshot.disabled      = true;
        btnStartStream.disabled   = true;
        btnStopStream.disabled    = true;
        btnApplyQuality.disabled  = true;
        btnClipSend.disabled      = true;
        btnClipPull.disabled      = true;

        // [NEW: File explorer]
        btnListDir.disabled       = true;
        btnGoUp.disabled          = true;
    };

    ws.onerror = (e) => {
        log("WebSocket ERROR: " + e.message);
    };

    ws.onmessage = (ev) => {
        log("WS << " + ev.data);

        let msg;
try {
            msg = JSON.parse(ev.data);
        } catch {
            log("Not JSON, ignore");
            return;
        }

        if (!pc) {
            log("No PeerConnection yet, ignoring signaling message");
            return;
        }

        if (msg.type === "answer") {
            log("Got ANSWER from server");
            pc.setRemoteDescription(
                new RTCSessionDescription({ type: "answer", sdp: msg.sdp })
            ).catch(err => log("setRemoteDescription(answer) error: " + err));
        } else if (msg.type === "ice") {
            if (msg.candidate) {
                const ice = new RTCIceCandidate({
                    candidate: msg.candidate,
                    sdpMid: msg.sdpMid ?? msg.mid ?? "0"
                });
                pc.addIceCandidate(ice)
                  .catch(err => log("addIceCandidate error: " + err));
            }
        } else {
            log("Unknown signaling type: " + msg.type);
        }
    };
};

// =========================
// 2. WebRTC + DataChannel
// =========================
btnStartWebrtc.onclick = async () => {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        log("WebSocket not open");
        return;
    }

    if (pc) {
        log("PeerConnection already created");
        return;
    }

    log("Creating RTCPeerConnection...");
    pc = new RTCPeerConnection({
        iceServers: [{ urls: "stun:stun.l.google.com:19302" }]
    });

    // DataChannel "input" từ browser -> server
    inputChannel = pc.createDataChannel("input");

    inputChannel.onopen = () => {
        log("DataChannel 'input' OPEN");
        btnSnapshot.disabled      = false;
        btnStartStream.disabled   = false;
        btnStopStream.disabled    = false;
        btnApplyQuality.disabled  = false;
        btnClipSend.disabled      = false;
        btnClipPull.disabled      = false;

        // [NEW: File explorer]
        btnListDir.disabled       = false;
        btnGoUp.disabled          = false;
    };

    inputChannel.onclose = () => {
        log("DataChannel 'input' CLOSE");
        btnSnapshot.disabled      = true;
        btnStartStream.disabled   = true;
        btnStopStream.disabled    = true;
        btnApplyQuality.disabled  = true;
        btnClipSend.disabled      = true;
        btnClipPull.disabled      = true;

        // [NEW: File explorer]
        btnListDir.disabled       = true;
        btnGoUp.disabled          = true;
    };

    inputChannel.onerror = (e) => {
        log("DataChannel error: " + e.message);
    };

    // Nhận data từ C++ (screen / screen_frame / screen_chunk / clipboard_update / dir_list / file_data)
    inputChannel.onmessage = (ev) => {
        try {
            const msg = JSON.parse(ev.data);

            // 1) Gửi nguyên frame base64 (cũ)
            if (msg.cmd === "screen" || msg.cmd === "screen_frame") {
                const b64 = msg.image_base64 || msg.image;
                if (b64) {
imgRemote.src = "data:image/jpeg;base64," + b64;
                } else {
                    log("[DC] screen message nhưng không có image");
                }
                return;
            }

            // 2) Gửi dạng chunk (screen_chunk)
            if (msg.cmd === "screen_chunk") {
                const id    = msg.frameId;
                const total = msg.total;
                const index = msg.index;
                const data  = msg.data;

                if (
                    typeof id    === "undefined" ||
                    typeof total === "undefined" ||
                    typeof index === "undefined" ||
                    typeof data  !== "string"
                ) {
                    log("[DC] screen_chunk invalid: " + ev.data);
                    return;
                }

                if (!frameBuffer.has(id)) {
                    frameBuffer.set(id, {
                        total,
                        received: 0,
                        parts: new Array(total)
                    });
                }

                const frame = frameBuffer.get(id);
                if (!frame.parts[index]) {
                    frame.parts[index] = data;
                    frame.received++;
                }

                if (frame.received === frame.total) {
                    const base64 = frame.parts.join("");
                    imgRemote.src = "data:image/jpeg;base64," + base64;
                    frameBuffer.delete(id);
                }
                return;
            }

            // 3) Clipboard update từ server
            if (msg.cmd === "clipboard_update") {
                const text = msg.text || "";
                clipboardText.value = text;

                if (navigator.clipboard && navigator.clipboard.writeText) {
                    navigator.clipboard.writeText(text).catch(err => {
                        log("[Clipboard] write failed: " + err);
                    });
                }
                return;
            }

            // 4) Directory listing [NEW]
            if (msg.cmd === "dir_list") {
                handleDirList(msg);
                return;
            }

            // 5) File content [NEW]
            if (msg.cmd === "file_data") {
                handleFileData(msg);
                return;
            }

            // 6) Các message khác
            log("[DC] message: " + ev.data);
        } catch (e) {
            log("[DC] parse error: " + e);
        }
    };

    // ICE candidates từ browser gửi sang server
    pc.onicecandidate = (ev) => {
        if (ev.candidate) {
            const msg = {
                type: "ice",
                candidate: ev.candidate.candidate,
                sdpMid: ev.candidate.sdpMid
            };
            log("WS >> ICE: " + JSON.stringify(msg));
            ws.send(JSON.stringify(msg));
        } else {
            log("ICE gathering finished");
        }
    };
pc.onconnectionstatechange = () => {
        log("PC connectionState = " + pc.connectionState);
    };

    // Gửi OFFER
    try {
        log("Creating offer...");
        const offer = await pc.createOffer();
        await pc.setLocalDescription(offer);

        const msg = { type: "offer", sdp: offer.sdp };

        log("WS >> OFFER");
        ws.send(JSON.stringify(msg));
    } catch (err) {
        log("Error during createOffer/setLocalDescription: " + err);
    }
};

// =========================
// 3. Gửi lệnh qua DataChannel
// =========================
function dcReady() {
    return inputChannel && inputChannel.readyState === "open";
}

function sendDc(obj) {
    if (!dcReady()) return;
    inputChannel.send(JSON.stringify(obj));
}

btnSnapshot.onclick = () => {
    if (!dcReady()) {
        log("DataChannel not ready");
        return;
    }
    sendDc({ cmd: "get_screen" });
};

btnStartStream.onclick = () => {
    if (!dcReady()) {
        log("DataChannel not ready");
        return;
    }

    const fps = parseInt(fpsInput.value, 10) || 10;
    sendDc({ cmd: "start_stream", fps });
    log("DC >> start_stream fps=" + fps);
};

btnStopStream.onclick = () => {
    if (!dcReady()) {
        log("DataChannel not ready");
        return;
    }
    sendDc({ cmd: "stop_stream" });
    log("DC >> stop_stream");
};

// Apply quality/width
btnApplyQuality.onclick = () => {
    if (!dcReady()) {
        log("DataChannel not ready");
        return;
    }
    const quality = parseInt(qualitySelect.value, 10) || 70;
    const maxWidth = parseInt(widthSelect.value, 10) || 1280;

    sendDc({
        cmd: "set_quality",
        quality,
        max_width: maxWidth
    });

    log(DC >> set_quality width=${maxWidth}, quality=${quality});
};

// ================== Mouse & keyboard ==================

// Chuột: move / down / up trên ảnh màn hình
imgRemote.addEventListener("mousemove", (e) => {
    if (!dcReady()) return;
    const rect = imgRemote.getBoundingClientRect();
    if (rect.width === 0 || rect.height === 0) return;

    const x = (e.clientX - rect.left) / rect.width;
    const y = (e.clientY - rect.top) / rect.height;
    sendDc({ cmd: "mouse_move", x, y });
});

// click
imgRemote.addEventListener("mousedown", (e) => {
    if (!dcReady()) return;
    let button = "left";
    if (e.button === 1) button = "middle";
    else if (e.button === 2) button = "right";
    sendDc({ cmd: "mouse_down", button });
    e.preventDefault();
});

imgRemote.addEventListener("mouseup", (e) => {
    if (!dcReady()) return;
    let button = "left";
    if (e.button === 1) button = "middle";
    else if (e.button === 2) button = "right";
    sendDc({ cmd: "mouse_up", button });
    e.preventDefault();
});

// wheel scroll
imgRemote.addEventListener("wheel", (e) => {
    if (!dcReady()) return;
    const delta = e.deltaY; // dương: scroll xuống, âm: scroll lên
    sendDc({ cmd: "mouse_wheel", delta });
    e.preventDefault();
}, { passive: false });

// Keyboard: key_down / key_up
window.addEventListener("keydown", (e) => {
    if (!dcReady()) return;
    if (e.repeat) return; // tránh spam khi giữ phím
    const key = normalizeKeyName(e);
    sendDc({ cmd: "key_down", key });
    e.preventDefault();
}, true);

window.addEventListener("keyup", (e) => {
    if (!dcReady()) return;
    const key = normalizeKeyName(e);
    sendDc({ cmd: "key_up", key });
    e.preventDefault();
}, true);

function normalizeKeyName(e) {
    if (e.key.length === 1) {
        return e.key; // A, b, 1,...
    }

    const k = e.key.toUpperCase();
    switch (k) {
        case "ENTER":      return "ENTER";
        case "ESCAPE":     return "ESC";
        case "TAB":        return "TAB";
        case "BACKSPACE":  return "BACKSPACE";
        case " ":          return "SPACE";
        case "ARROWUP":    return "UP";
        case "ARROWDOWN":  return "DOWN";
        case "ARROWLEFT":  return "LEFT";
        case "ARROWRIGHT": return "RIGHT";
        default:
            if (/^F\d{1,2}$/.test(k)) return k; // F1..F12
            return k;
    }
}

// ================= Clipboard buttons ==================

// Send local clipboard/textarea lên remote
btnClipSend.onclick = async () => {
    if (!dcReady()) {
        log("DataChannel not ready");
        return;
    }

    let text = clipboardText.value;

    // nếu textarea trống, thử đọc clipboard hệ thống
    if (!text && navigator.clipboard && navigator.clipboard.readText) {
        try {
            text = await navigator.clipboard.readText();
            clipboardText.value = text;
        } catch (e) {
            log("[Clipboard] readText failed: " + e);
        }
    }

    sendDc({ cmd: "clipboard_set", text });
    log("[Clipboard] send -> len=" + text.length);
};

// Yêu cầu server gửi clipboard về
btnClipPull.onclick = () => {
    if (!dcReady()) {
        log("DataChannel not ready");
        return;
    }
    sendDc({ cmd: "clipboard_get" });
    log("[Clipboard] request remote clipboard");
};

// ================= File explorer buttons [NEW] ==================

btnListDir.onclick = () => {
    if (!dcReady()) {
        log("DataChannel not ready");
        return;
    }
    const path = filePathInput.value.trim();
    sendDc({ cmd: "list_dir", path });
};

btnGoUp.onclick = () => {
    if (!dcReady()) {
        log("DataChannel not ready");
        return;
    }
    let path = filePathInput.value.trim();
    if (!path) return;

    // Bóc 1 cấp: tách theo / hoặc \
    path = path.replace(/[/\\]+$/, "");          // bỏ slash cuối
    const parts = path.split(/[/\\]+/);
    parts.pop();
    const upPath = parts.join("\\");             // ghép kiểu Windows

    filePathInput.value = upPath;
    sendDc({ cmd: "list_dir", path: upPath });
};

// ================= Handlers for dir_list & file_data [NEW] ==================

function handleDirList(msg) {
    const ok      = !!msg.ok;
const path    = msg.resolved || msg.path || "";
    const entries = msg.entries || [];
    const error   = msg.error || "";

    fileErrorEl.textContent = error ? "[Remote] " + error : "";
    if (path) filePathInput.value = path;

    fileTableBody.innerHTML = "";

    if (!ok) return;

    // Sắp xếp: thư mục trước, rồi file, theo tên
    entries.sort((a, b) => {
        if (a.is_dir && !b.is_dir) return -1;
        if (!a.is_dir && b.is_dir) return 1;
        return (a.name || "").localeCompare(b.name || "");
    });

    for (const e of entries) {
        const tr = document.createElement("tr");

        // Name (click để vào dir / tải file)
        const tdName = document.createElement("td");
        const link = document.createElement("a");
        link.href = "#";
        link.textContent = e.name || "(null)";
        link.onclick = (ev) => {
            ev.preventDefault();
            const base = path;
            const sep = (!base || base.endsWith("\\") || base.endsWith("/")) ? "" : "\\";
            const newPath = base ? (base + sep + e.name) : e.name;

            if (e.is_dir) {
                filePathInput.value = newPath;
                sendDc({ cmd: "list_dir", path: newPath });
            } else {
                sendDc({ cmd: "file_get", path: newPath });
            }
        };
        tdName.appendChild(link);

        // Type
        const tdType = document.createElement("td");
        tdType.textContent = e.is_dir ? "DIR" : "FILE";

        // Size
        const tdSize = document.createElement("td");
        tdSize.textContent = e.is_dir ? "" : String(e.size ?? "");

        // Actions (button Get)
        const tdActions = document.createElement("td");
        if (!e.is_dir) {
            const btn = document.createElement("button");
            btn.textContent = "Get";
            btn.onclick = (ev) => {
                ev.preventDefault();
                const base = path;
                const sep = (!base || base.endsWith("\\") || base.endsWith("/")) ? "" : "\\";
                const newPath = base ? (base + sep + e.name) : e.name;
                sendDc({ cmd: "file_get", path: newPath });
            };
            tdActions.appendChild(btn);
        }

        tr.appendChild(tdName);
        tr.appendChild(tdType);
        tr.appendChild(tdSize);
        tr.appendChild(tdActions);
        fileTableBody.appendChild(tr);
    }
}

function handleFileData(msg) {
    const ok    = !!msg.ok;
    const error = msg.error || "";

    if (!ok) {
        fileErrorEl.textContent = error ? "[File] " + error : "[File] error";
        return;
    }

    const name = msg.name || "remote-file.bin";
    const b64  = msg.content_base64 || "";

    if (!b64) {
        fileErrorEl.textContent = "[File] empty content";
        return;
    }

    try {
        const byteStr = atob(b64);
        const len = byteStr.length;
        const bytes = new Uint8Array(len);
        for (let i = 0; i < len; ++i) {
bytes[i] = byteStr.charCodeAt(i);
        }

        const blob = new Blob([bytes]);
        const url  = URL.createObjectURL(blob);

        const a = document.createElement("a");
        a.href = url;
        a.download = name;
        a.style.display = "none";
        document.body.appendChild(a);
        a.click();

        setTimeout(() => {
            URL.revokeObjectURL(url);
            document.body.removeChild(a);
        }, 0);

        fileErrorEl.textContent = "";
        log([File] download started: ${name} (${len} bytes));
    } catch (e) {
        fileErrorEl.textContent = "[File] decode error: " + e;
    }
}
body {
    font-family: Consolas, monospace;
    background: #111;
    color: #eee;
    padding: 16px;
}

#log {
    white-space: pre-wrap;
    background: #000;
    border: 1px solid #444;
    padding: 8px;
    height: 200px;
    overflow-y: auto;
    margin-top: 8px;
}

#remoteScreenImg {
    width: 640px;
    height: 360px;
    background: #222;
    margin-top: 12px;
    object-fit: contain;
}

button {
    padding: 6px 12px;
    margin-right: 8px;
    margin-top: 4px;
}

/* ============================= */
/* Remote Files panel [NEW]      */
/* ============================= */

#filePanel {
    margin-top: 16px;
    background: #000;
    border: 1px solid #444;
    padding: 8px;
}

#fileTable {
    width: 100%;
    border-collapse: collapse;
    margin-top: 6px;
    font-size: 13px;
}

    #fileTable th,
    #fileTable td {
        border-bottom: 1px solid #333;
        padding: 2px 4px;
    }

#fileError {
    color: #ff6666;
    margin-top: 4px;
    min-height: 18px;
}