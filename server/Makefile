# # Compiler settings
# CXX = g++
# CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
# LDFLAGS = -pthread

# # Platform-specific settings
# UNAME_S := $(shell uname -s)
# ifeq ($(UNAME_S),Linux)
#     LDFLAGS += -lssl -lcrypto -lsqlite3
# endif
# ifeq ($(UNAME_S),Darwin)
#     LDFLAGS += -lssl -lcrypto -lsqlite3
# endif

# # Directories
# SRC_DIR = src
# OBJ_DIR = ../build/obj
# BIN_DIR = ../build/bin

# # Source files
# CORE_SOURCES = $(SRC_DIR)/core/Platform.cpp \
#                $(SRC_DIR)/core/Network.cpp \
#                $(SRC_DIR)/core/CommandHandler.cpp

# NETWORK_SOURCES = $(SRC_DIR)/network/WebSocketServer.cpp

# MAIN_SOURCES = $(SRC_DIR)/main/server_main.cpp \
#                $(SRC_DIR)/main/websocket_main.cpp \
#                $(SRC_DIR)/main/client_main.cpp

# # Object files
# CORE_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CORE_SOURCES))
# NETWORK_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(NETWORK_SOURCES))

# # Targets
# .PHONY: all clean server websocket_server client

# all: directories server websocket_server client

# directories:
# 	@mkdir -p $(OBJ_DIR)/core
# 	@mkdir -p $(OBJ_DIR)/network
# 	@mkdir -p $(OBJ_DIR)/main
# 	@mkdir -p $(BIN_DIR)

# # Compile object files
# $(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
# 	@echo "Compiling $<..."
# 	@$(CXX) $(CXXFLAGS) -c $< -o $@

# # Link server
# server: $(CORE_OBJECTS) $(OBJ_DIR)/main/server_main.o
# 	@echo "Linking server..."
# 	@$(CXX) $^ -o $(BIN_DIR)/server $(LDFLAGS)
# 	@echo "✓ Server built: $(BIN_DIR)/server"

# # Link WebSocket server
# websocket_server: $(CORE_OBJECTS) $(NETWORK_OBJECTS) $(OBJ_DIR)/main/websocket_main.o
# 	@echo "Linking WebSocket server..."
# 	@$(CXX) $^ -o $(BIN_DIR)/websocket_server $(LDFLAGS)
# 	@echo "✓ WebSocket server built: $(BIN_DIR)/websocket_server"

# # Link client
# client: $(CORE_OBJECTS) $(OBJ_DIR)/main/client_main.o
# 	@echo "Linking client..."
# 	@$(CXX) $^ -o $(BIN_DIR)/client $(LDFLAGS)
# 	@echo "✓ Client built: $(BIN_DIR)/client"

# clean:
# 	@echo "Cleaning..."
# 	@rm -rf $(OBJ_DIR)/* $(BIN_DIR)/*
# 	@echo "✓ Clean complete"

# run-server: server
# 	@$(BIN_DIR)/server

# run-websocket: websocket_server
# 	@$(BIN_DIR)/websocket_server


# Compiler settings
# Compiler settings


# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
LDFLAGS = -pthread

# === Platform detection ===
UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
OS := $(UNAME_S)

# Detect MinGW/MSYS (Windows)
ifneq (,$(findstring MINGW,$(UNAME_S)))
    OS := Windows
endif
ifneq (,$(findstring MSYS,$(UNAME_S)))
    OS := Windows
endif

# === Platform-specific libraries ===
ifeq ($(OS),Windows)
    # Windows (MinGW-w64): Winsock + OpenSSL
    LDFLAGS += -lws2_32 -lcrypto -lssl -lgdi32 -lcrypt32
    # Order matters: objects first, then libraries
else ifeq ($(OS),Linux)
    LDFLAGS += -lssl -lcrypto -lsqlite3
else ifeq ($(OS),Darwin)  # macOS
    LDFLAGS += -lssl -lcrypto -lsqlite3
endif

# Directories
SRC_DIR = src
OBJ_DIR = ../build/obj
BIN_DIR = ../build/bin

# Source files (using wildcard for core & network)
CORE_SOURCES    = $(wildcard $(SRC_DIR)/core/*.cpp)
NETWORK_SOURCES = $(wildcard $(SRC_DIR)/network/*.cpp)

MAIN_SOURCES = $(SRC_DIR)/main/server_main.cpp \
               $(SRC_DIR)/main/websocket_main.cpp \
               $(SRC_DIR)/main/client_main.cpp

# Object files
CORE_OBJECTS    = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CORE_SOURCES))
NETWORK_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(NETWORK_SOURCES))
MAIN_OBJECTS    = $(patsubst $(SRC_DIR)/main/%.cpp,$(OBJ_DIR)/main/%.o,$(MAIN_SOURCES))

# Target-specific object lists
SERVER_OBJS           = $(CORE_OBJECTS) $(OBJ_DIR)/main/server_main.o
WEBSOCKET_SERVER_OBJS = $(CORE_OBJECTS) $(NETWORK_OBJECTS) $(OBJ_DIR)/main/websocket_main.o
CLIENT_OBJS           = $(CORE_OBJECTS) $(OBJ_DIR)/main/client_main.o

# Targets
.PHONY: all clean server websocket_server client run-server run-websocket

all: directories server websocket_server client

directories:
	@mkdir -p $(OBJ_DIR)/core $(OBJ_DIR)/network $(OBJ_DIR)/main $(BIN_DIR)

# Compile rule
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Link targets
server: $(SERVER_OBJS)
	@echo "Linking server..."
	@$(CXX) $^ -o $(BIN_DIR)/server $(LDFLAGS)
	@echo "Server built: $(BIN_DIR)/server"

websocket_server: $(WEBSOCKET_SERVER_OBJS)
	@echo "Linking WebSocket server (OpenSSL + Winsock enabled)..."
	@$(CXX) $^ -o $(BIN_DIR)/websocket_server $(LDFLAGS)
	@echo "WebSocket server built: $(BIN_DIR)/websocket_server"

client: $(CLIENT_OBJS)
	@echo "Linking client..."
	@$(CXX) $^ -o $(BIN_DIR)/client $(LDFLAGS)
	@echo "Client built: $(BIN_DIR)/client"

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(OBJ_DIR)/* $(BIN_DIR)/*
	@echo "Clean complete"

# Run helpers
run-server: server
	@echo "Starting regular server..."
	@$(BIN_DIR)/server

run-websocket: websocket_server
	@echo "Starting WebSocket server on ws://0.0.0.0:8081 ..."
	@$(BIN_DIR)/websocket_server