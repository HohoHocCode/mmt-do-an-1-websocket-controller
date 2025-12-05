# =========================
# Cross-platform Makefile - FIXED & FINAL
# =========================

# Compiler
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -Iserver/include -Iserver/include/core -Iserver/include/network

# Linker flags (only when linking)
LDFLAGS  = -pthread -mconsole

# ---------- Platform detection ----------
ifeq ($(OS),Windows_NT)
    PLATFORM = Windows
    EXT      = .exe
    LDFLAGS += -lws2_32 -lwsock32 -lpsapi -lssl -lcrypto
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        PLATFORM = Linux
        EXT      =
        LDFLAGS += -lssl -lcrypto
    endif
    ifeq ($(UNAME_S),Darwin)
        PLATFORM = macOS
        EXT      =
        LDFLAGS += -lssl -lcrypto
    endif
endif

# ---------- Directories ----------
OBJ_DIR = build/obj
BIN_DIR = build/bin
SRC_DIR = server/src

# ---------- Source files ----------
COMMON_SOURCES = \
    $(SRC_DIR)/core/Platform.cpp \
    $(SRC_DIR)/core/Network.cpp \
    $(SRC_DIR)/core/CommandHandler.cpp

NETWORK_SOURCES = \
    $(SRC_DIR)/network/WebSocketServer.cpp \
    $(SRC_DIR)/network/web_server.cpp

SERVER_SOURCES      = $(SRC_DIR)/main/server_main.cpp
CLIENT_SOURCES      = $(SRC_DIR)/main/client_main.cpp
WEB_MAIN_SOURCES    = $(SRC_DIR)/main/web_main.cpp
WEBSOCKET_SOURCES   = $(SRC_DIR)/main/websocket_main.cpp

# ---------- Object files (full sub-dirs) ----------
COMMON_OBJECTS    = $(OBJ_DIR)/core/Platform.o \
                    $(OBJ_DIR)/core/Network.o \
                    $(OBJ_DIR)/core/CommandHandler.o

NETWORK_OBJECTS   = $(OBJ_DIR)/network/WebSocketServer.o \
                    $(OBJ_DIR)/network/web_server.o

SERVER_OBJECTS      = $(OBJ_DIR)/main/server_main.o
CLIENT_OBJECTS      = $(OBJ_DIR)/main/client_main.o
WEB_MAIN_OBJECTS    = $(OBJ_DIR)/main/web_main.o
WEBSOCKET_OBJECTS   = $(OBJ_DIR)/main/websocket_main.o

# ---------- Executables ----------
SERVER_TARGET      = $(BIN_DIR)/server$(EXT)
CLIENT_TARGET      = $(BIN_DIR)/client$(EXT)
WEB_SERVER_TARGET  = $(BIN_DIR)/web_server$(EXT)
WEBSOCKET_TARGET   = $(BIN_DIR)/websocket_server$(EXT)

# ---------- Default target ----------
all: info directories $(SERVER_TARGET) $(CLIENT_TARGET) $(WEB_SERVER_TARGET) $(WEBSOCKET_TARGET)

info:
	@echo "Building for: $(PLATFORM)"
	@echo ""

# ---------- Create required folders ----------
directories:
	@echo "Creating build directories..."
	@mkdir -p $(OBJ_DIR)/core $(OBJ_DIR)/network $(OBJ_DIR)/main $(BIN_DIR) data/logs

# ---------- Compile rules (explicit sub-dirs) ----------
$(OBJ_DIR)/core/%.o: $(SRC_DIR)/core/%.cpp
	@echo "Compiling $< ..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/network/%.o: $(SRC_DIR)/network/%.cpp
	@echo "Compiling $< ..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/main/%.o: $(SRC_DIR)/main/%.cpp
	@echo "Compiling $< ..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# ---------- Link targets ----------
$(SERVER_TARGET): $(COMMON_OBJECTS) $(SERVER_OBJECTS)
	@echo "Linking server ..."
	@$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "Server built: $@"

$(CLIENT_TARGET): $(COMMON_OBJECTS) $(CLIENT_OBJECTS)
	@echo "Linking client ..."
	@$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "Client built: $@"

$(WEB_SERVER_TARGET): $(COMMON_OBJECTS) $(NETWORK_OBJECTS) $(WEB_MAIN_OBJECTS)
	@echo "Linking web server ..."
	@$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "Web server built: $@"

$(WEBSOCKET_TARGET): $(COMMON_OBJECTS) $(NETWORK_OBJECTS) $(WEBSOCKET_OBJECTS)
	@echo "Linking WebSocket server ..."
	@$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "WebSocket server built: $@"

	# === UI: React Web Interface ===
ui: 
	@echo "Starting Remote Control UI..."
	@cd client/web && npm start

ui-install:
	@echo "Installing UI dependencies..."
	@cd client/web && npm install

ui-build:
	@echo "Building UI for production..."
	@cd client/web && npm run build

# ---------- Convenience targets ----------
server: directories $(SERVER_TARGET)
client: directories $(CLIENT_TARGET)
web-server: directories $(WEB_SERVER_TARGET)
websocket: directories $(WEBSOCKET_TARGET)

# ---------- Run commands ----------
run-server: $(SERVER_TARGET)
	@$(SERVER_TARGET)

run-client: $(CLIENT_TARGET)
	@if [ -z "$(SERVER_IP)" ]; then \
		echo "Usage: make run-client SERVER_IP=127.0.0.1"; \
		exit 1; \
	fi; \
	$(CLIENT_TARGET) $(SERVER_IP)

run-web-server: $(WEB_SERVER_TARGET)
	@echo "Starting web server: http://localhost:8080"
	@$(WEB_SERVER_TARGET)

run-websocket: $(WEBSOCKET_TARGET)
	@echo "Starting WebSocket server: ws://localhost:8081"
	@$(WEBSOCKET_TARGET)

# ---------- Clean ----------
clean:
	@echo "Cleaning..."
	-@rm -rf build/obj build/bin
	@echo "Done."

rebuild: clean all

.PHONY: all info directories server client web-server websocket \
        clean rebuild run-server run-client run-web-server run-websocket