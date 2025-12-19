# UX + Ops quick notes

## LAN discovery
- The web app calls `POST /api/discover`, which broadcasts `MMT_DISCOVER <nonce>` on `DISCOVERY_PORT` (default `41000`). Timeouts/retries are configurable with `DISCOVERY_TIMEOUT_MS` and `DISCOVERY_RETRIES`.
- The controller replies over UDP with JSON `{ nonce, wsPort, name, version, ip, timestamp }`. The responder runs inside the C++ agent when `DISCOVERY_ENABLED` is not set to `0/false`; override `DISCOVERY_PORT`, `AGENT_NAME`, `AGENT_VERSION` as needed.
- UI list actions:
  - **Use** — prefill host/port/name for manual add.
  - **Connect** — create a target immediately and start the WebSocket connection.

## Controller lifecycle (Node)
- Configure the child process with:
  - `CONTROLLER_CMD` (required to start), optional `CONTROLLER_ARGS`, `CONTROLLER_WORKDIR`, `CONTROLLER_GRACE_MS`.
  - `CONTROLLER_AUTO_START=1` will launch the child on API startup.
- Endpoints (auth required; restart/stop require admin):
  - `GET /api/controller/status`
  - `POST /api/controller/restart`
  - `POST /api/controller/stop`
- The UI shows controller status in the top bar and asks for confirmation before restart/stop.

## Hotkeys and safety
- Hotkeys are **off by default**; enable them in the Hotkeys card. Combos are captured only while their inputs are focused and ignored when typing in form fields.
- Default bindings: Connect (Ctrl+K), Reset (Ctrl+Shift+X), Stream (Ctrl+Shift+S), Processes (Ctrl+Shift+P). Stored in `localStorage`.
- “Reset / Cancel All” stops local stream state and sends `cancel_all` to the active/broadcast targets.

## Quick manual test loop
1) Start auth API: `cd server && npm install && npm run dev` (configure `.env` with JWT/DB secrets).
2) Start controller: build the C++ server, set `DISCOVERY_ENABLED=1 DISCOVERY_PORT=41000 AUTH_API_URL=http://localhost:5179`, run the binary (ws port 9002 by default).
3) Frontend: `cd client/web && npm install && npm run dev` (or `npm run build && npm run preview`).
4) Login, hit **Discover on LAN**, connect to a found agent, run a screen/camera action, then use **Reset / Cancel All** and **Restart controller** to confirm lifecycle handling.
