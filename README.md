# MMT WebSocket Controller

## Run locally (Windows)

### Build the C++ API (required for `run-backend.ps1`)

```powershell
cmake --preset mingw64
cmake --build build
```

> If your local toolchain uses a different preset or output directory, update the preset or pass `-Preset` to the script.

### Start the backend

```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-backend.ps1
```

The script starts the C++ API in the foreground and prints the health URL.

### Verify

```powershell
curl http://localhost:8080/health
```

### Optional: Node API (alternative)

```powershell
cd server
npm install
npm run dev:api
```
