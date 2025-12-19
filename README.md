# MMT WebSocket Controller

## Run locally (Windows)

### Start MySQL

Ensure MySQL is running and create the schema (default database: `mmt_remote`).

```powershell
mysql -u root -p -e "CREATE DATABASE IF NOT EXISTS mmt_remote;"
mysql -u root -p mmt_remote < db/schema.sql
```

Set environment variables if you use non-default credentials:

```powershell
$env:DB_HOST="127.0.0.1"
$env:DB_PORT="3306"
$env:DB_USER="root"
$env:DB_PASSWORD=""
$env:DB_NAME="mmt_remote"
```

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

### Start the UI

```powershell
cd client/web
npm install
npm run dev
```

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
