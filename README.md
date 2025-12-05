# Remote Desktop Control System

Cross-platform remote administration tool with web interface.

## Quick Start

### Build Server

```bash
cd server
make all
```

### Run on One Computer

```bash
# Terminal 1: Start WebSocket server
./build/bin/websocket_server

# Terminal 2: Start web client
cd client/web
npm install
npm run dev

# Browser: http://localhost:5173
```

### Run on Two Computers

**Server (Computer 1):**

```bash
./build/bin/websocket_server
# Opens port 8080
```

**Client (Computer 2):**

```bash
# Browser: http://<server-ip>:8080
```

## Documentation

See `docs/README.md` for full documentation.
