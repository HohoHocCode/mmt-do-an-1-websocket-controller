CREATE TABLE IF NOT EXISTS users (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  username VARCHAR(128) NOT NULL UNIQUE,
  role ENUM('admin', 'user') NOT NULL DEFAULT 'user',
  password_hash VARCHAR(255) NULL,
  setup_token_hash VARCHAR(255) NULL,
  setup_token_created_at DATETIME NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS audit_log (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  user_id BIGINT NULL,
  username VARCHAR(128) NULL,
  action VARCHAR(64) NOT NULL,
  meta_json JSON NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_action_created_at (action, created_at),
  INDEX idx_username_created_at (username, created_at)
);
