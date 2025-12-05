#!/bin/bash

# Deployment Script for Remote Desktop Control System

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Default configuration
DEPLOY_DIR="/opt/remote-desktop"
SERVICE_NAME="remote-desktop"
WEB_PORT=8080

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

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_info() {
    echo -e "${NC}➜${NC} $1"
}

# Check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then 
        print_error "Please run as root (use sudo)"
        exit 1
    fi
}

# Check if dist exists
check_dist() {
    if [ ! -d "dist" ]; then
        print_error "Distribution not found. Run: ./scripts/build_all.sh dist"
        exit 1
    fi
}

# Stop existing service
stop_service() {
    print_info "Stopping existing service..."
    
    if systemctl is-active --quiet $SERVICE_NAME; then
        systemctl stop $SERVICE_NAME
        print_success "Service stopped"
    else
        print_info "Service not running"
    fi
}

# Create deployment directory
create_deploy_dir() {
    print_info "Creating deployment directory..."
    
    mkdir -p $DEPLOY_DIR/{bin,config,logs,data,www}
    
    # Set permissions
    chmod 755 $DEPLOY_DIR
    chmod 755 $DEPLOY_DIR/{bin,config,logs,data,www}
    
    print_success "Directory created: $DEPLOY_DIR"
}

# Copy files
copy_files() {
    print_info "Copying files..."
    
    # Server binaries
    cp dist/server/* $DEPLOY_DIR/bin/
    chmod +x $DEPLOY_DIR/bin/*
    
    # Configuration
    cp -r server/config/* $DEPLOY_DIR/config/ 2>/dev/null || true
    
    # Web client
    cp -r dist/client/* $DEPLOY_DIR/www/
    
    # Create necessary subdirectories
    mkdir -p $DEPLOY_DIR/data/{database,sessions,backup}
    
    print_success "Files copied"
}

# Create systemd service
create_systemd_service() {
    print_info "Creating systemd service..."
    
    cat > /etc/systemd/system/$SERVICE_NAME.service << EOF
[Unit]
Description=Remote Desktop Control Server
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=$DEPLOY_DIR
ExecStart=$DEPLOY_DIR/bin/websocket_server
Restart=always
RestartSec=10

# Security settings
NoNewPrivileges=true
PrivateTmp=true

# Logging
StandardOutput=append:$DEPLOY_DIR/logs/server.log
StandardError=append:$DEPLOY_DIR/logs/error.log

[Install]
WantedBy=multi-user.target
EOF

    systemctl daemon-reload
    print_success "Systemd service created"
}

# Configure nginx (optional)
configure_nginx() {
    if ! command -v nginx &> /dev/null; then
        print_warning "Nginx not found. Skipping web server configuration."
        return
    fi
    
    print_info "Configuring Nginx..."
    
    cat > /etc/nginx/sites-available/remote-desktop << EOF
server {
    listen 80;
    server_name _;
    
    root $DEPLOY_DIR/www;
    index index.html;
    
    location / {
        try_files \$uri \$uri/ /index.html;
    }
    
    # WebSocket proxy
    location /ws {
        proxy_pass http://localhost:$WEB_PORT;
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
        proxy_read_timeout 86400;
    }
    
    # Security headers
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
}
EOF

    # Enable site
    ln -sf /etc/nginx/sites-available/remote-desktop /etc/nginx/sites-enabled/
    
    # Test and reload
    nginx -t && systemctl reload nginx
    
    print_success "Nginx configured"
}

# Configure firewall
configure_firewall() {
    print_info "Configuring firewall..."
    
    if command -v ufw &> /dev/null; then
        ufw allow $WEB_PORT/tcp
        ufw allow 80/tcp
        print_success "UFW firewall configured"
    elif command -v firewall-cmd &> /dev/null; then
        firewall-cmd --permanent --add-port=$WEB_PORT/tcp
        firewall-cmd --permanent --add-port=80/tcp
        firewall-cmd --reload
        print_success "Firewalld configured"
    else
        print_warning "No firewall detected. Please open ports manually:"
        echo "  - Port $WEB_PORT (WebSocket)"
        echo "  - Port 80 (HTTP)"
    fi
}

# Create startup scripts
create_scripts() {
    print_info "Creating management scripts..."
    
    # Start script
    cat > $DEPLOY_DIR/start.sh << 'EOF'
#!/bin/bash
systemctl start remote-desktop
echo "Remote Desktop Control started"
echo "Access at: http://$(hostname -I | awk '{print $1}')"
EOF
    
    # Stop script
    cat > $DEPLOY_DIR/stop.sh << 'EOF'
#!/bin/bash
systemctl stop remote-desktop
echo "Remote Desktop Control stopped"
EOF
    
    # Status script
    cat > $DEPLOY_DIR/status.sh << 'EOF'
#!/bin/bash
systemctl status remote-desktop
EOF
    
    # Restart script
    cat > $DEPLOY_DIR/restart.sh << 'EOF'
#!/bin/bash
systemctl restart remote-desktop
echo "Remote Desktop Control restarted"
EOF
    
    # Logs script
    cat > $DEPLOY_DIR/logs.sh << 'EOF'
#!/bin/bash
tail -f logs/server.log
EOF
    
    chmod +x $DEPLOY_DIR/*.sh
    
    print_success "Management scripts created"
}

# Create backup script
create_backup_script() {
    cat > $DEPLOY_DIR/backup.sh << 'EOF'
#!/bin/bash
BACKUP_DIR="data/backup"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

mkdir -p $BACKUP_DIR

# Backup database
if [ -f data/database/remote_access.db ]; then
    cp data/database/remote_access.db $BACKUP_DIR/db_$TIMESTAMP.db
    echo "Database backed up to $BACKUP_DIR/db_$TIMESTAMP.db"
fi

# Backup logs
tar -czf $BACKUP_DIR/logs_$TIMESTAMP.tar.gz logs/*.log
echo "Logs backed up to $BACKUP_DIR/logs_$TIMESTAMP.tar.gz"

# Keep only last 10 backups
ls -t $BACKUP_DIR/db_*.db | tail -n +11 | xargs -r rm
ls -t $BACKUP_DIR/logs_*.tar.gz | tail -n +11 | xargs -r rm

echo "Backup complete"
EOF
    
    chmod +x $DEPLOY_DIR/backup.sh
    print_success "Backup script created"
}

# Start service
start_service() {
    print_info "Starting service..."
    
    systemctl enable $SERVICE_NAME
    systemctl start $SERVICE_NAME
    
    sleep 2
    
    if systemctl is-active --quiet $SERVICE_NAME; then
        print_success "Service started successfully"
    else
        print_error "Service failed to start. Check logs:"
        echo "  journalctl -u $SERVICE_NAME -n 50"
        exit 1
    fi
}

# Show deployment info
show_info() {
    print_header "Deployment Complete!"
    
    echo "Installation Directory: $DEPLOY_DIR"
    echo ""
    echo "Service Management:"
    echo "  Start:   systemctl start $SERVICE_NAME"
    echo "  Stop:    systemctl stop $SERVICE_NAME"
    echo "  Status:  systemctl status $SERVICE_NAME"
    echo "  Logs:    journalctl -u $SERVICE_NAME -f"
    echo ""
    echo "Quick Scripts:"
    echo "  cd $DEPLOY_DIR"
    echo "  ./start.sh    - Start service"
    echo "  ./stop.sh     - Stop service"
    echo "  ./status.sh   - Check status"
    echo "  ./restart.sh  - Restart service"
    echo "  ./logs.sh     - View logs"
    echo "  ./backup.sh   - Backup data"
    echo ""
    echo "Access:"
    LOCAL_IP=$(hostname -I | awk '{print $1}')
    echo "  WebSocket: ws://$LOCAL_IP:$WEB_PORT"
    echo "  Web UI:    http://$LOCAL_IP"
    echo ""
    echo "Configuration:"
    echo "  Server config: $DEPLOY_DIR/config/server.conf"
    echo "  Logs:         $DEPLOY_DIR/logs/"
    echo "  Database:     $DEPLOY_DIR/data/database/"
    echo ""
}

# Uninstall
uninstall() {
    print_warning "Uninstalling Remote Desktop Control..."
    
    # Stop service
    systemctl stop $SERVICE_NAME 2>/dev/null || true
    systemctl disable $SERVICE_NAME 2>/dev/null || true
    
    # Remove systemd service
    rm -f /etc/systemd/system/$SERVICE_NAME.service
    systemctl daemon-reload
    
    # Remove nginx config
    rm -f /etc/nginx/sites-enabled/remote-desktop
    rm -f /etc/nginx/sites-available/remote-desktop
    
    # Remove installation directory
    read -p "Remove installation directory $DEPLOY_DIR? (y/N): " confirm
    if [ "$confirm" == "y" ]; then
        rm -rf $DEPLOY_DIR
        print_success "Installation directory removed"
    fi
    
    print_success "Uninstall complete"
}

# Main deployment process
deploy() {
    print_header "Remote Desktop Control - Deployment"
    
    check_root
    check_dist
    
    stop_service
    create_deploy_dir
    copy_files
    create_systemd_service
    create_scripts
    create_backup_script
    
    # Optional components
    read -p "Configure Nginx? (y/N): " nginx_choice
    if [ "$nginx_choice" == "y" ]; then
        configure_nginx
    fi
    
    read -p "Configure firewall? (y/N): " fw_choice
    if [ "$fw_choice" == "y" ]; then
        configure_firewall
    fi
    
    start_service
    show_info
}

# Parse arguments
case "${1:-deploy}" in
    deploy)
        deploy
        ;;
    uninstall)
        uninstall
        ;;
    update)
        print_info "Updating installation..."
        check_dist
        stop_service
        copy_files
        start_service
        print_success "Update complete"
        ;;
    info)
        show_info
        ;;
    *)
        echo "Usage: $0 [deploy|uninstall|update|info]"
        exit 1
        ;;
esac