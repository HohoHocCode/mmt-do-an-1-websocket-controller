# Auth & UX Guide

## MySQL (dev)
1. Copy `.env.example` to `.env` at repo root and adjust secrets if needed.
2. Start MySQL 8 with the provided docker compose:
   ```bash
   docker compose up -d mysql
   ```
   - Host: `localhost`, Port: `3306`
   - DB: `mmt_controller`, User: `mmt`, Password: `mmtpass`
3. If you already have MySQL running elsewhere, point the env vars to that instance instead.

## NodeJS auth API
Located in `server/`.

1. Install dependencies (registry access required):
   ```bash
   cd server
   npm install
   ```
2. Apply migrations and seed dev users:
   ```bash
   npm run migrate
   npm run seed
   ```
   - Seeds `admin` (role admin) and `demo` (role user). A setup token is printed unless a seed password is provided via `ADMIN_SEED_PASSWORD` / `DEMO_SEED_PASSWORD`.
3. Run the API:
   ```bash
   npm run dev
   ```
   - Default port: `5179` (`BACKEND_PORT` env).
4. Create additional users with one-time setup tokens:
   ```bash
   npm run create-user -- --username alice --role user
   ```

### Env keys (server)
- `MYSQL_HOST`, `MYSQL_PORT`, `MYSQL_USER`, `MYSQL_PASSWORD`, `MYSQL_DATABASE`
- `JWT_SECRET`, `SETUP_TOKEN_SECRET`, `AUTH_TOKEN_TTL_SECONDS`
- `BACKEND_PORT` (default 5179)
- `DISCOVERY_PORT` (default 41000, UDP broadcast)
- `DISCOVERY_TIMEOUT_MS` (per-attempt wait, default 1800ms), `DISCOVERY_RETRIES` (default 2)
- `CONTROLLER_CMD`, `CONTROLLER_ARGS`, `CONTROLLER_WORKDIR`, `CONTROLLER_GRACE_MS` (process management)
- `CONTROLLER_AUTO_START` (set to `1/true` to launch controller child process on boot)

## Login flow (UI)
1. User enters username; the client calls `GET /auth/status`.
2. If the account exists but has no password, the “Set password” view asks for the setup token and a new password (one-time, 24h token TTL).
3. Otherwise, enter password and login → receives JWT + role.
4. Tokens are stored in `localStorage` for this dev build; lock or logout clears them.

## Console UX (client/web)
- **Auth handshake:** After connecting to an agent, the web app sends `{"cmd":"auth","token":"<JWT>"}` over WebSocket.
- **Controller menu:** Restart / stop the controller child process (admin only, confirmation required); refresh status; Reset tasks (all users).
- **Lock session:** Clears token and returns to the login screen; actions are blocked while locked.
- **Reset tasks:** Sends `cancel_all` to the agent to stop running streams and clears local task state.
- **LAN discovery:** Calls `POST /api/discover` to broadcast a UDP message on `DISCOVERY_PORT` (no port scanning). Results can prefill the target form or connect immediately.
- **Hotkeys:** Disabled by default; can be enabled per-session, captured only while the hotkey input is focused, and ignored when typing elsewhere.
- **Targets:** Manual add remains available (host + port, defaults to 9002).

## Agent/backend enforcement (C++)
- WebSocket server now handles:
  - `auth` → verifies JWT via `AUTH_API_URL` (default `http://localhost:5179`) and caches the user.
  - `shutdown` / `restart` → require verified admin token; otherwise return an error. (Current `SystemControl` is a stub; actions are acknowledged only if implemented.)
  - `cancel_all` → stops active streams and acks immediately.
- Set `AUTH_API_URL` in the agent environment to point at the Node auth service.

## Limitations / Notes
- Restart/Shutdown handlers are gated but rely on stubbed `SystemControl`; OS-level execution must be completed separately.
- Tokens are stored in `localStorage` for dev convenience; move to a more secure store for production.

## Acceptance checklist
- `GET /auth/status` returns existence + password state.
- First-time password setup requires a valid, unexpired setup token.
- Login returns JWT + role; `/auth/verify` validates signatures.
- UI switches correctly between “not found / set password / login”.
- Restart/Shutdown are hidden/disabled for non-admin and rejected server-side without admin tokens.
- Reset tasks clears running streams without needing a reconnect.
- No keylogger or OS input capture added; sensitive actions are explicit and audited.

## LAN discovery + controller lifecycle (new)
- UI “Discover on LAN” calls `POST /api/discover` → the Node service broadcasts `MMT_DISCOVER <nonce>` on `DISCOVERY_PORT` (UDP) with timeouts/retries.
- Agents reply with metadata (`name`, `version`, `wsPort`, `nonce`) via UDP. Responses populate the discovery list, allow “Use” (prefill form) or “Connect” (add + connect immediately).
- Agent-side UDP responder can be toggled with `DISCOVERY_ENABLED` (default on) and uses `DISCOVERY_PORT`. It responds without blocking the WebSocket server.
- Controller process management:
  - `POST /api/controller/restart|stop` (admin only) restarts/stops the child process defined by `CONTROLLER_CMD` (+ optional args/working dir).
  - Status endpoint `GET /api/controller/status` is available to authenticated users for UI display.
- UI hotkeys: disabled by default, explicitly enabled via the Hotkeys card. Combos are only captured while their inputs are focused; global hotkeys ignore text fields and execute Connect/Reset/Stream/Process refresh actions when enabled.
