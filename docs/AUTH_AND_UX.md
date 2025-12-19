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

## Login flow (UI)
1. User enters username; the client calls `GET /auth/status`.
2. If the account exists but has no password, the “Set password” view asks for the setup token and a new password (one-time, 24h token TTL).
3. Otherwise, enter password and login → receives JWT + role.
4. Tokens are stored in `localStorage` for this dev build; lock or logout clears them.

## Console UX (client/web)
- **Auth handshake:** After connecting to an agent, the web app sends `{"cmd":"auth","token":"<JWT>"}` over WebSocket.
- **Power menu:** Restart / Shutdown (admin only, requires typing `RESTART`/`SHUTDOWN`), Reset tasks (all users).
- **Lock session:** Clears token and returns to the login screen; actions are blocked while locked.
- **Reset tasks:** Sends `cancel_all` to the agent to stop running streams and clears local task state.
- **LAN discovery:** Calls `GET /devices/discover` to broadcast a UDP message on port 41000 (no port scanning). Results can prefill the target form; empty responses are expected until agents implement the UDP listener.
- **Targets:** Manual add remains available (host + port, defaults to 9002).

## Agent/backend enforcement (C++)
- WebSocket server now handles:
  - `auth` → verifies JWT via `AUTH_API_URL` (default `http://localhost:5179`) and caches the user.
  - `shutdown` / `restart` → require verified admin token; otherwise return an error. (Current `SystemControl` is a stub; actions are acknowledged only if implemented.)
  - `cancel_all` → stops active streams and acks immediately.
- Set `AUTH_API_URL` in the agent environment to point at the Node auth service.

## Limitations / Notes
- Restart/Shutdown handlers are gated but rely on stubbed `SystemControl`; OS-level execution must be completed separately.
- UDP discovery replies are not yet implemented on the agent; expect an empty list until added.
- Tokens are stored in `localStorage` for dev convenience; move to a more secure store for production.

## Acceptance checklist
- `GET /auth/status` returns existence + password state.
- First-time password setup requires a valid, unexpired setup token.
- Login returns JWT + role; `/auth/verify` validates signatures.
- UI switches correctly between “not found / set password / login”.
- Restart/Shutdown are hidden/disabled for non-admin and rejected server-side without admin tokens.
- Reset tasks clears running streams without needing a reconnect.
- No keylogger or OS input capture added; sensitive actions are explicit and audited.
