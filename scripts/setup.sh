#!/bin/bash

# Remote Desktop Control - Setup Script
# This script creates the project structure and sets up the environment

set -e  # Exit on error

echo "=========================================="
echo "  Remote Desktop Control - Setup"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored messages
print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_info() {
    echo -e "${NC}ℹ${NC} $1"
}

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        OS="linux"
        print_info "Detected OS: Linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
        print_info "Detected OS: macOS"
    elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
        OS="windows"
        print_info "Detected OS: Windows"
    else
        print_error "Unknown OS: $OSTYPE"
        exit 1
    fi
}

# Create directory structure
create_structure() {
    print_info "Creating project structure..."
    
    # Server directories
    mkdir -p server/{include,src}/{core,network,platform,utils}
    mkdir -p server/src/main
    mkdir -p server/tests/{unit,integration,fixtures}
    mkdir -p server/config
    mkdir -p server/deps
    
    # Client directories
    mkdir -p client/web/{public,src}
    mkdir -p client/web/src/{components,services,hooks,context,utils,styles}
    mkdir -p client/web/src/components/{Terminal,ProcessList,SystemInfo,Login,common}
    mkdir -p client/desktop
    
    # Shared directories
    mkdir -p shared/{protocols,schemas}
    
    # Root directories
    mkdir -p {docs,scripts,logs,data}
    mkdir -p data/{database,sessions,backup}
    mkdir -p docs/{api,guides,development,images}
    mkdir -p build/{bin,obj,lib}
    mkdir -p docker
    
    print_success "Directory structure created"
}

# Create .gitignore
create_gitignore() {
    print_info "Creating .gitignore..."
    
    cat > .gitignore << 'EOF'
# Build artifacts
build/
*.o
*.obj
*.exe
*.out
*.app

# Dependencies
node_modules/
server/deps/

# Logs
logs/*.log
*.log

# Database
data/database/*.db
data/sessions/*

# Security
certs/
keys/
*.key
*.pem
secrets.conf
.env
*.env.local

# IDE
.vscode/
.idea/
*.swp
*.swo
*~

# OS
.DS_Store
Thumbs.db

# Temporary
tmp/
temp/
*.tmp

# Compiled
dist/
*.so
*.dll
*.dylib
EOF

    print_success ".gitignore created"
}

# Install server dependencies
install_server_deps() {
    print_info "Installing server dependencies..."
    
    if [ "$OS" == "linux" ]; then
        print_info "Installing on Linux..."
        if command -v apt-get &> /dev/null; then
            sudo apt-get update
            sudo apt-get install -y build-essential g++ make cmake
            sudo apt-get install -y libssl-dev libsqlite3-dev
            print_success "Linux dependencies installed"
        elif command -v yum &> /dev/null; then
            sudo yum groupinstall -y "Development Tools"
            sudo yum install -y openssl-devel sqlite-devel
            print_success "Linux dependencies installed"
        else
            print_warning "Package manager not found. Please install manually:"
            echo "  - g++ / gcc"
            echo "  - make / cmake"
            echo "  - openssl-dev"
            echo "  - sqlite3-dev"
        fi
        
    elif [ "$OS" == "macos" ]; then
        print_info "Installing on macOS..."
        if ! command -v brew &> /dev/null; then
            print_error "Homebrew not found. Installing..."
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        brew install openssl sqlite3 cmake
        print_success "macOS dependencies installed"
        
    elif [ "$OS" == "windows" ]; then
        print_warning "Windows detected. Please install manually:"
        echo "  1. Install MinGW-w64 or MSVC"
        echo "  2. Install OpenSSL: https://slproweb.com/products/Win32OpenSSL.html"
        echo "  3. Install SQLite: https://www.sqlite.org/download.html"
    fi
}

# Install client dependencies
install_client_deps() {
    print_info "Installing client dependencies..."
    
    if ! command -v node &> /dev/null; then
        print_warning "Node.js not found. Please install from: https://nodejs.org/"
        return
    fi
    
    print_info "Node.js version: $(node --version)"
    print_info "npm version: $(npm --version)"
    
    cd client/web
    
    # Create package.json if not exists
    if [ ! -f package.json ]; then
        print_info "Creating package.json..."
        cat > package.json << 'EOF'
{
  "name": "remote-desktop-control-web",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "vite build",
    "preview": "vite preview"
  },
  "dependencies": {
    "react": "^18.2.0",
    "react-dom": "^18.2.0"
  },
  "devDependencies": {
    "@vitejs/plugin-react": "^4.0.0",
    "vite": "^4.3.9",
    "tailwindcss": "^3.3.2",
    "autoprefixer": "^10.4.14",
    "postcss": "^8.4.24"
  }
}
EOF
    fi
    
    npm install
    cd ../..
    
    print_success "Client dependencies installed"
}

# Create sample configuration files
create_config_files() {
    print_info "Creating configuration files..."
    
    # Server config
    cat > server/config/server.conf << 'EOF'
[Server]
port = 5555
protocol = tcp
max_connections = 10
timeout = 300

[WebSocket]
ws_port = 8080
ssl_enabled = false

[Logging]
log_level = info
log_file = logs/server.log
max_log_size = 10MB

[Security]
require_auth = true
max_failed_attempts = 3
session_timeout = 3600
EOF

    # Environment variables for client
    cat > client/web/.env.example << 'EOF'
VITE_WS_URL=ws://localhost:8080
VITE_API_URL=http://localhost:5555
VITE_APP_NAME=Remote Desktop Control
EOF

    cp client/web/.env.example client/web/.env
    
    print_success "Configuration files created"
}

# Create Makefile
create_makefile() {
    print_info "Creating Makefile..."
    
    cat > server/Makefile << 'EOF'
# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
LDFLAGS = -pthread

# Platform-specific settings
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lssl -lcrypto -lsqlite3
endif
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -lssl -lcrypto -lsqlite3
endif

# Directories
SRC_DIR = src
OBJ_DIR = ../build/obj
BIN_DIR = ../build/bin

# Source files
CORE_SOURCES = $(SRC_DIR)/core/Platform.cpp \
               $(SRC_DIR)/core/Network.cpp \
               $(SRC_DIR)/core/CommandHandler.cpp

NETWORK_SOURCES = $(SRC_DIR)/network/WebSocketServer.cpp

MAIN_SOURCES = $(SRC_DIR)/main/ServerMain.cpp \
               $(SRC_DIR)/main/WebSocketMain.cpp \
               $(SRC_DIR)/main/ClientMain.cpp

# Object files
CORE_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CORE_SOURCES))
NETWORK_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(NETWORK_SOURCES))

# Targets
.PHONY: all clean server websocket_server client

all: directories server websocket_server client

directories:
	@mkdir -p $(OBJ_DIR)/core
	@mkdir -p $(OBJ_DIR)/network
	@mkdir -p $(OBJ_DIR)/main
	@mkdir -p $(BIN_DIR)

# Compile object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Link server
server: $(CORE_OBJECTS) $(OBJ_DIR)/main/ServerMain.o
	@echo "Linking server..."
	@$(CXX) $^ -o $(BIN_DIR)/server $(LDFLAGS)
	@echo "✓ Server built: $(BIN_DIR)/server"

# Link WebSocket server
websocket_server: $(CORE_OBJECTS) $(NETWORK_OBJECTS) $(OBJ_DIR)/main/WebSocketMain.o
	@echo "Linking WebSocket server..."
	@$(CXX) $^ -o $(BIN_DIR)/websocket_server $(LDFLAGS)
	@echo "✓ WebSocket server built: $(BIN_DIR)/websocket_server"

# Link client
client: $(CORE_OBJECTS) $(OBJ_DIR)/main/ClientMain.o
	@echo "Linking client..."
	@$(CXX) $^ -o $(BIN_DIR)/client $(LDFLAGS)
	@echo "✓ Client built: $(BIN_DIR)/client"

clean:
	@echo "Cleaning..."
	@rm -rf $(OBJ_DIR)/* $(BIN_DIR)/*
	@echo "✓ Clean complete"

run-server: server
	@$(BIN_DIR)/server

run-websocket: websocket_server
	@$(BIN_DIR)/websocket_server
EOF

    print_success "Makefile created"
}

# Create README
create_readme() {
    print_info "Creating README.md..."
    
    cat > README.md << 'EOF'
# Remote Desktop Control System

A cross-platform remote desktop control system with WebSocket support.

## Features

- ✅ Cross-platform (Windows, Linux, macOS)
- ✅ WebSocket real-time communication
- ✅ Process management
- ✅ System monitoring
- ✅ File operations
- ✅ Command execution
- ✅ Web-based UI

## Quick Start

### Setup
```bash
chmod +x scripts/*.sh
./scripts/setup.sh
```

### Build Server
```bash
cd server
make all
```

### Run WebSocket Server
```bash
./build/bin/websocket_server
```

### Run Web Client
```bash
cd client/web
npm run dev
```

Open browser: http://localhost:5173

## Documentation

See `docs/` folder for detailed documentation.

## License

MIT License
EOF

    print_success "README.md created"
}

# Main execution
main() {
    echo ""
    detect_os
    echo ""
    
    create_structure
    create_gitignore
    create_config_files
    create_makefile
    create_readme
    
    echo ""
    print_info "Installing dependencies..."
    echo ""
    
    install_server_deps
    echo ""
    install_client_deps
    
    echo ""
    echo "=========================================="
    print_success "Setup complete!"
    echo "=========================================="
    echo ""
    echo "Next steps:"
    echo "  1. cd server && make all"
    echo "  2. ./build/bin/websocket_server"
    echo "  3. cd client/web && npm run dev"
    echo ""
    echo "Happy coding! 🚀"
    echo ""
}

# Run main function
main