# Run & build guide

## Build (CMake + vcpkg)

```bash
cmake --preset codex-linux
cmake --build --preset codex-linux
```

> The presets expect the vcpkg toolchain at `cmake/toolchains/vcpkg_optional.cmake`.  
> Update `DB_*` environment variables as needed for the API.

## Run the API service

```bash
./build/codex-linux/mmt_api
# Environment (optional):
#   API_PORT=8080
#   DB_HOST=127.0.0.1
#   DB_USER=root
#   DB_PASSWORD=secret
#   DB_NAME=mmt_remote
#   DB_PORT=3306
```

The API listens on HTTP port `8080` by default and exposes `/api/*` routes.
Import `db/schema.sql` into your MySQL instance before running the service.

## Run the WebSocket controller

```bash
./build/codex-linux/server
```

The controller keeps the existing WebSocket stream implementation and discovery responder.

## Frontend (Vite + React)

```bash
cd client/web
npm install
npm run dev
```

The Vite dev server proxies `/api` to `http://localhost:8080` and `/ws` to `ws://localhost:8080`.
