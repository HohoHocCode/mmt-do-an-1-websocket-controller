# AGENTS.md — Guide for Codex/Agents working in this repository

## 0) Overall goals
As an Agent, you must deliver work across **three** main areas:

1) **Project-wide bug hunting & fixes**
- Investigate compile/build issues, runtime errors, logic bugs, crashes, memory leaks, race conditions (if any).
- Priority order: **crash / cannot run / cannot connect WebSocket** > logic bugs > UX issues.

2) **Improve Web UI/UX (modern, usable, clear state)**
- Make the UI look **modern**, clean, consistent, and responsive.
- Improve experience: connection status, errors/notifications, loading states, empty states, layout, typography.

3) **Thoroughly handle the files within the scope list below**
- Read carefully, refactor reasonably, fix bugs, improve structure, add types/validation, reduce code smells.
- If you must edit files outside the scope to make build/test pass, you may do so — but keep changes **minimal**.

---

## 1) Required file scope
### Root
- `list.h`, `list.cpp`
- `start.h`, `start.cpp`
- `end.h`, `end.cpp`
- `main.cpp`
- `keylogger.h`, `keylogger.cpp`
- `CMakeLists.txt`
- `CMakePresets.json`
- `.vscode/c_cpp_properties.json`, `.vscode/settings.json`

### C++ (include/)
- `include/utils/json.hpp`
- `include/utils/base64.hpp`
- `include/modules/system_control.hpp`
- `include/modules/screen.hpp`
- `include/modules/process.hpp`
- `include/modules/consent.hpp`
- `include/modules/camera.hpp`
- `include/core/command.hpp`
- `include/core/dispatcher.hpp`
- `include/network/ws_server.hpp`
- `include/network/ws_client.hpp`

### C++ (src/)
- `src/server/main.cpp`
- `src/client/main.cpp`
- `src/modules/system_control.cpp`
- `src/modules/consent.cpp`
- `src/modules/screen.cpp`
- `src/modules/process.cpp`
- `src/modules/camera.cpp`
- `src/utils/base64.cpp`
- `src/network/ws_client.cpp`
- `src/network/ws_server.cpp`
- `src/core/dispatcher.cpp`

### Web client (client/web)
- `client/web/src/vite-env.d.ts`
- `client/web/src/main.tsx`
- `client/web/src/types.ts`
- `client/web/src/styles/index.css`
- `client/web/src/web_interface.tsx`
- `client/web/src/App.tsx`
- `client/web/.env`, `client/web/.env.example`
- `client/web/postcss.config.js`
- `client/web/index.html`
- `client/web/vite.config.js`, `client/web/vite.config.ts`
- `client/web/tailwind.config.js`
- `client/web/tsconfig.json`, `client/web/tsconfig.node.json`
- `client/web/package.json`
- `client/web/.gitignore`

---

## 2) Safety & ethics constraints (MANDATORY)
This repository contains `keylogger.*`. The Agent **MUST NOT**:
- Expand or create functionality intended for unauthorized sensitive data collection, stealth behavior, persistence, or hidden exfiltration.
- Add stealth/persistence mechanisms or process-hiding.

The Agent **MAY**:
- Refactor for clarity, safety, and maintainability.
- **Disable sensitive behavior by default**.
- Require **explicit, informed consent** (via `consent.*`) before enabling any sensitive capability.
- Add rate limits, input validation, transparent logging, safety toggles, and opt-out mechanisms.

If you find a high-risk flow, prefer:
- Safer alternatives (e.g., “disable feature”, “require explicit opt-in UI + server-side check”, “remove dangerous defaults”).

---

## 3) Standard workflow (recommended)
### Step A — Survey & plan
1) Understand architecture: C++ server/client, web client, WebSocket message flow, command dispatcher.
2) List likely problems:
   - Build/CMake/presets/toolchains, include paths, standard flags, warnings-as-errors.
   - WebSocket: parsing, concurrency, disconnect handling, ping/pong, reconnection strategy.
   - Modules: system/screen/process/camera permissions, OS-specific failures.
3) Record “Top issues” by severity + reproduction steps.

### Step B — Make it run
- Prioritize getting **build stable** (C++ and web).
- Add short run/build notes (README or PR description) if helpful.

### Step C — Fix bugs in small iterations
For each bugfix:
1) Provide clear reproduction (or logs/trace).
2) Apply minimal, targeted fix (avoid rewrites).
3) Add tests if a test framework exists.
4) If no tests exist: add validation/logging/sanity checks to reduce regressions.

### Step D — UI/UX facelift (structured)
- Improve React structure: componentization, clean types, lean state handling.
- Improve connection + action feedback (status, errors, notifications).

---

## 4) Build / Run / Test rules
### C++ (CMake)
- Prefer `CMakePresets.json` if valid presets exist.
- If unclear:
  - Read `CMakeLists.txt` and `CMakePresets.json` to infer build steps,
  - Then document “Build notes” in the PR.

Mandatory when changing C++:
- Keep warnings reasonable without breaking builds.
- Reduce UB and improve error handling paths.
- Standardize error reporting: structured codes/messages when possible.

**Codex/Linux environment note:**
- Codex cloud usually runs on Linux. Windows-only presets (e.g., MinGW) may fail.
- If needed, add a Linux preset (e.g., `codex-linux`) while keeping Windows presets intact.

### Web (Vite + React + Tailwind)
- Read `client/web/package.json` for scripts.
- Ensure:
  - `npm ci` (or `npm install`) works,
  - `npm run dev` works (when possible),
  - `npm run build` has no TypeScript errors,
  - `npm run lint` passes if present.

---

## 5) C++ code quality standards
- Do not use `using namespace std;` in headers.
- Use include guards or `#pragma once` consistently.
- Avoid unnecessary includes; reduce coupling.
- Avoid magic strings for commands; centralize constants (enum/string constants).
- All WebSocket messages must have validation:
  - minimal schema (`type`, `payload`),
  - size limits,
  - handle missing fields / wrong types,
  - reject dangerous or unexpected input.
- Prefer RAII, avoid raw owning pointers, close resources deterministically.

---

## 6) Web code quality standards
- TypeScript strict-friendly (avoid `any`).
- Keep `types.ts` clean: message types, API contracts, shared domain types.
- UI must include:
  - Connection status (Connected / Disconnected / Connecting).
  - Clear error messaging (toast/alert).
  - Empty-state + loading-state.
  - Responsive layout (mobile/desktop).
  - Consistent theme/typography (Tailwind spacing + tokens).
  - Basic accessibility: focus ring, aria-labels for key buttons.

Modern UI/UX direction:
- Layout: Sidebar (actions) + Main panel (logs/results) OR topbar + cards.
- Use “cards”, “badges/chips”, “toast”, “confirm modal” (especially for risky actions).
- Optional: dark mode + persist preference.

---

## 7) WebSocket & command dispatcher rules
Recommended message format:
- `type`: string
- `requestId`: string (map request/response)
- `payload`: object
- `timestamp`: number (optional)
- `error`: `{ code, message }` (optional)

Dispatcher requirements:
- Must not crash on unknown commands.
- Handlers return structured errors.
- Add request timeout when applicable.
- Logging levels: info/warn/error.

Consent gate:
- Sensitive commands (system_control/screen/camera/process/keylogger if present) must verify consent before execution.

---

## 8) Branch, PR, and change management (IMPORTANT)
- Work on a dedicated branch (e.g., `codex/...`).
- Keep PRs small and focused:
  - One PR = one logical fix (build fix, merge fix, preset fix, UI improvement, etc.)
- Do not mix unrelated refactors with build portability changes.
- Never “fix” TypeScript build by weakening strictness. Fix the code.

---

## 9) Definition of Done checklist
### Bugfix
- [ ] Project builds (at least one valid configuration).
- [ ] Clear bugs within scoped files fixed.
- [ ] No obvious crashes on WebSocket connect/disconnect.
- [ ] Improved message validation.
- [ ] Clear logging/error handling.

### UI/UX
- [ ] UI is more modern, with better spacing/typography, responsive.
- [ ] Connection status + error notifications exist.
- [ ] Flows are clear and consistent.

### Safety
- [ ] Sensitive features disabled by default OR require explicit consent.
- [ ] No stealth/unauthorized data collection added.

---

## 10) Reporting format
When done, summarize:
1) Bugs found + reproduction + fix.
2) UI/UX changes (screenshots if possible).
3) Safety/consent changes.
4) Verified build/run commands.

End of file.
