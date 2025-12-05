#!/bin/bash

# Build All Components Script

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_info() {
    echo -e "${NC}➜${NC} $1"
}

# Build C++ Server
build_server() {
    print_header "Building C++ Server"
    
    cd server
    
    if [ -f Makefile ]; then
        print_info "Using Makefile..."
        make clean
        make all
    elif [ -f CMakeLists.txt ]; then
        print_info "Using CMake..."
        mkdir -p build
        cd build
        cmake ..
        make
        cd ..
    else
        print_error "No build configuration found (Makefile or CMakeLists.txt)"
        exit 1
    fi
    
    cd ..
    print_success "Server build complete"
}

# Build Web Client
build_web_client() {
    print_header "Building Web Client"
    
    cd client/web
    
    if [ ! -d node_modules ]; then
        print_info "Installing dependencies..."
        npm install
    fi
    
    print_info "Building production bundle..."
    npm run build
    
    cd ../..
    print_success "Web client build complete"
}

# Create distribution package
create_dist() {
    print_header "Creating Distribution Package"
    
    DIST_DIR="dist"
    rm -rf $DIST_DIR
    mkdir -p $DIST_DIR/{server,client}
    
    # Copy server binaries
    print_info "Copying server binaries..."
    cp -r build/bin/* $DIST_DIR/server/
    cp -r server/config $DIST_DIR/server/
    
    # Copy web client
    print_info "Copying web client..."
    if [ -d client/web/dist ]; then
        cp -r client/web/dist/* $DIST_DIR/client/
    else
        print_error "Web client dist folder not found"
    fi
    
    # Copy scripts
    print_info "Copying scripts..."
    mkdir -p $DIST_DIR/scripts
    cat > $DIST_DIR/scripts/start.sh << 'EOF'
#!/bin/bash
cd "$(dirname "$0")/.."
echo "Starting Remote Desktop Control Server..."
./server/websocket_server &
echo "Server started on ws://localhost:8080"
echo "Open client/index.html in your browser"
EOF
    chmod +x $DIST_DIR/scripts/start.sh
    
    # Create README
    cat > $DIST_DIR/README.txt << 'EOF'
Remote Desktop Control System
=============================

To run:
1. Execute: ./scripts/start.sh
2. Open client/index.html in browser
3. Connect using username

Server runs on: ws://localhost:8080

For more info, see documentation.
EOF
    
    print_success "Distribution package created in dist/"
}

# Run tests
run_tests() {
    print_header "Running Tests"
    
    # Server tests
    if [ -d server/tests ]; then
        print_info "Running server tests..."
        cd server
        make test 2>/dev/null || print_info "No server tests configured"
        cd ..
    fi
    
    # Client tests
    if [ -f client/web/package.json ]; then
        print_info "Running client tests..."
        cd client/web
        npm test 2>/dev/null || print_info "No client tests configured"
        cd ../..
    fi
    
    print_success "Tests complete"
}

# Show build info
show_info() {
    print_header "Build Information"
    
    echo "Executables:"
    if [ -f build/bin/server ]; then
        echo "  ✓ build/bin/server"
    fi
    if [ -f build/bin/websocket_server ]; then
        echo "  ✓ build/bin/websocket_server"
    fi
    if [ -f build/bin/client ]; then
        echo "  ✓ build/bin/client"
    fi
    
    echo ""
    echo "Web Client:"
    if [ -d client/web/dist ]; then
        echo "  ✓ client/web/dist/"
        du -sh client/web/dist 2>/dev/null || echo "  Size: unknown"
    fi
    
    echo ""
    echo "Distribution:"
    if [ -d dist ]; then
        echo "  ✓ dist/"
        du -sh dist 2>/dev/null || echo "  Size: unknown"
    fi
}

# Main menu
show_menu() {
    echo ""
    echo "Build Options:"
    echo "  1) Build Server Only"
    echo "  2) Build Client Only"
    echo "  3) Build All"
    echo "  4) Create Distribution"
    echo "  5) Run Tests"
    echo "  6) Clean All"
    echo "  0) Exit"
    echo ""
    read -p "Select option: " choice
    
    case $choice in
        1) build_server ;;
        2) build_web_client ;;
        3) build_server && build_web_client ;;
        4) build_server && build_web_client && create_dist ;;
        5) run_tests ;;
        6) 
            print_info "Cleaning..."
            cd server && make clean && cd ..
            rm -rf build/* dist/* client/web/dist
            print_success "Clean complete"
            ;;
        0) exit 0 ;;
        *) print_error "Invalid option" ;;
    esac
}

# Parse command line arguments
if [ $# -eq 0 ]; then
    show_menu
else
    case $1 in
        server) build_server ;;
        client) build_web_client ;;
        all) build_server && build_web_client ;;
        dist) build_server && build_web_client && create_dist ;;
        test) run_tests ;;
        clean)
            cd server && make clean && cd ..
            rm -rf build/* dist/* client/web/dist
            ;;
        info) show_info ;;
        *)
            echo "Usage: $0 [server|client|all|dist|test|clean|info]"
            exit 1
            ;;
    esac
fi

echo ""
show_info
echo ""
print_success "Build script complete!"
echo ""