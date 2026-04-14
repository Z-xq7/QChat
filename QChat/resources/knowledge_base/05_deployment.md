# QChat 部署指南

## 环境要求

### 服务器端

| 组件 | 版本要求 | 说明 |
|------|----------|------|
| C++ 编译器 | GCC 7+ / MSVC 2019+ | 支持 C++17 |
| Qt | 5.15+ / 6.x | 客户端需要 |
| Node.js | 14+ | VarifyServer 需要 |
| MySQL | 5.7+ / 8.0 | 主数据库 |
| Redis | 5.0+ | 缓存、会话 |
| Boost | 1.70+ | C++ 网络库 |
| gRPC | 1.30+ | RPC 框架 |
| cmake | 3.16+ | 构建工具 |

### 客户端

| 组件 | 版本要求 |
|------|----------|
| Qt | 6.x |
| 编译器 | MSVC 2019+ (Windows) / GCC 9+ (Linux) |

## 目录结构

```
/opt/qchat/                    # 部署根目录
├── bin/                       # 可执行文件
│   ├── GateServer
│   ├── ChatServer
│   ├── ChatServer2
│   ├── StatusServer
│   └── ...
├── config/                    # 配置文件
│   ├── gate_config.ini
│   ├── chat_config.ini
│   └── ...
├── logs/                      # 日志目录
├── data/                      # 数据文件
└── scripts/                   # 启动脚本
    ├── start_all.sh
    └── stop_all.sh
```

## 数据库初始化

### 1. 创建数据库

```sql
-- 登录 MySQL
mysql -u root -p

-- 创建数据库
CREATE DATABASE qchat CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- 创建用户
CREATE USER 'qchat_user'@'%' IDENTIFIED BY 'your_password';
GRANT ALL PRIVILEGES ON qchat.* TO 'qchat_user'@'%';
FLUSH PRIVILEGES;
```

### 2. 导入表结构

```bash
# 执行建表脚本
mysql -u qchat_user -p qchat < sql/create_tables.sql
```

### 3. Redis 配置

```bash
# 安装 Redis
sudo apt-get install redis-server

# 配置密码（可选，建议生产环境启用）
sudo vim /etc/redis/redis.conf
# 修改: requirepass your_redis_password

# 启动 Redis
sudo systemctl start redis-server
sudo systemctl enable redis-server
```

## 服务器编译部署

### 1. 安装依赖（Ubuntu 22.04）

```bash
# 基础工具
sudo apt-get update
sudo apt-get install -y build-essential cmake git

# Boost
sudo apt-get install -y libboost-all-dev

# gRPC
sudo apt-get install -y protobuf-compiler libprotobuf-dev
sudo apt-get install -y libgrpc++-dev libgrpc-dev

# MySQL 客户端库
sudo apt-get install -y libmysqlclient-dev

# Redis 客户端库
sudo apt-get install -y libhiredis-dev
```

### 2. 编译项目

```bash
# 克隆代码
git clone https://github.com/yourusername/qchat.git
cd qchat

# 创建构建目录
mkdir build && cd build

# 配置
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译
make -j$(nproc)
```

### 3. 部署可执行文件

```bash
# 创建部署目录
sudo mkdir -p /opt/qchat/bin
sudo mkdir -p /opt/qchat/config
sudo mkdir -p /opt/qchat/logs

# 复制可执行文件
sudo cp build/GateServer/GateServer /opt/qchat/bin/
sudo cp build/ChatServer/ChatServer /opt/qchat/bin/
sudo cp build/ChatServer2/ChatServer2 /opt/qchat/bin/
sudo cp build/StatusServer/StatusServer /opt/qchat/bin/

# 复制配置文件
sudo cp config/*.ini /opt/qchat/config/
```

## 配置文件说明

### GateServer 配置 (gate_config.ini)

```ini
[Server]
Host = 0.0.0.0
Port = 13419

[StatusServer]
Host = 127.0.0.1
Port = 50052

[MySQL]
Host = 127.0.0.1
Port = 3306
User = qchat_user
Password = your_password
Database = qchat

[Redis]
Host = 127.0.0.1
Port = 6379
Password = 
```

### ChatServer 配置 (chat_config.ini)

```ini
[Server]
Host = 0.0.0.0
Port = 8090
GrpcPort = 50055
ServerId = chat_server_1

[StatusServer]
Host = 127.0.0.1
Port = 50052

[MySQL]
Host = 127.0.0.1
Port = 3306
User = qchat_user
Password = your_password
Database = qchat

[Redis]
Host = 127.0.0.1
Port = 6379
Password =
```

### StatusServer 配置 (status_config.ini)

```ini
[Server]
GrpcPort = 50052

[Redis]
Host = 127.0.0.1
Port = 6379
Password =
```

## 启动服务

### 启动脚本 (start_all.sh)

```bash
#!/bin/bash

DEPLOY_DIR=/opt/qchat
LOG_DIR=$DEPLOY_DIR/logs

# 创建日志目录
mkdir -p $LOG_DIR

# 启动 StatusServer
echo "Starting StatusServer..."
nohup $DEPLOY_DIR/bin/StatusServer \
    --config=$DEPLOY_DIR/config/status_config.ini \
    > $LOG_DIR/status_server.log 2>&1 &
sleep 2

# 启动 ChatServer
echo "Starting ChatServer..."
nohup $DEPLOY_DIR/bin/ChatServer \
    --config=$DEPLOY_DIR/config/chat_config.ini \
    > $LOG_DIR/chat_server.log 2>&1 &
sleep 2

# 启动 ChatServer2
echo "Starting ChatServer2..."
nohup $DEPLOY_DIR/bin/ChatServer2 \
    --config=$DEPLOY_DIR/config/chat_config2.ini \
    > $LOG_DIR/chat_server2.log 2>&1 &
sleep 2

# 启动 GateServer
echo "Starting GateServer..."
nohup $DEPLOY_DIR/bin/GateServer \
    --config=$DEPLOY_DIR/config/gate_config.ini \
    > $LOG_DIR/gate_server.log 2>&1 &

echo "All servers started!"
```

### 停止脚本 (stop_all.sh)

```bash
#!/bin/bash

echo "Stopping all servers..."

pkill -f GateServer
pkill -f ChatServer
pkill -f ChatServer2
pkill -f StatusServer

echo "All servers stopped!"
```

### 赋予执行权限

```bash
chmod +x /opt/qchat/scripts/start_all.sh
chmod +x /opt/qchat/scripts/stop_all.sh
```

## VarifyServer 部署

```bash
cd /opt/qchat/VarifyServer

# 安装依赖
npm install

# 配置环境变量
export MYSQL_HOST=127.0.0.1
export MYSQL_PORT=3306
export MYSQL_USER=qchat_user
export MYSQL_PASSWORD=your_password
export MYSQL_DATABASE=qchat
export REDIS_HOST=127.0.0.1
export REDIS_PORT=6379
export SMTP_HOST=smtp.example.com
export SMTP_PORT=587
export SMTP_USER=noreply@example.com
export SMTP_PASS=your_smtp_password

# 启动
nohup node server.js > /opt/qchat/logs/varify_server.log 2>&1 &
```

## TURN 服务器部署

视频通话需要 TURN 服务器，使用 coturn：

```bash
# 安装 coturn
sudo apt-get install coturn

# 配置
sudo vim /etc/turnserver.conf
```

```ini
# turnserver.conf
listening-port=3478
listening-ip=your_server_ip
relay-ip=your_server_ip
external-ip=your_server_ip
realm=your_domain.com
server-name=turnserver
fingerprint
lt-cred-mech
user=qchat_user:qchat_password
no-stdout-log
log-file=/var/log/turnserver.log
```

```bash
# 启动 TURN 服务
sudo systemctl enable coturn
sudo systemctl start coturn
```

## 客户端编译

### Windows (MSVC)

```bash
# 使用 Qt Creator 打开 QChat.pro
# 或命令行编译
qmake QChat.pro
nmake release
```

### Linux

```bash
cd QChat
qmake QChat.pro
make -j$(nproc)
```

## 验证部署

### 1. 检查服务状态

```bash
# 查看进程
ps aux | grep -E "GateServer|ChatServer|StatusServer"

# 查看端口监听
netstat -tlnp | grep -E "13419|8090|8091|50052"
```

### 2. 测试 API

```bash
# 测试 GateServer
curl -X POST http://localhost:13419/get_verify_code \
  -H "Content-Type: application/json" \
  -d '{"email": "test@example.com"}'
```

### 3. 查看日志

```bash
# 实时查看日志
tail -f /opt/qchat/logs/gate_server.log
tail -f /opt/qchat/logs/chat_server.log
```

## 常见问题

### 1. 数据库连接失败

**现象**：服务器启动时报数据库连接错误

**解决**：
- 检查 MySQL 服务是否启动
- 检查用户名密码是否正确
- 检查防火墙是否放行 3306 端口

### 2. 端口被占用

**现象**：启动时报 `bind failed`

**解决**：
```bash
# 查找占用端口的进程
sudo lsof -i :8090

# 结束进程
sudo kill -9 <PID>
```

### 3. 客户端无法连接

**检查项**：
- 服务器防火墙是否放行端口
- 云服务器安全组规则
- 客户端配置的 IP/端口是否正确

```bash
# 检查防火墙
sudo ufw status
sudo ufw allow 13419/tcp
sudo ufw allow 8090/tcp
sudo ufw allow 8091/tcp
```

## 生产环境建议

### 1. 使用 systemd 管理服务

创建 `/etc/systemd/system/qchat-gate.service`：

```ini
[Unit]
Description=QChat GateServer
After=network.target mysql.service redis.service

[Service]
Type=simple
User=qchat
WorkingDirectory=/opt/qchat
ExecStart=/opt/qchat/bin/GateServer --config=/opt/qchat/config/gate_config.ini
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable qchat-gate
sudo systemctl start qchat-gate
```

### 2. 使用 Nginx 反向代理

```nginx
server {
    listen 80;
    server_name api.qchat.example.com;
    
    location / {
        proxy_pass http://127.0.0.1:13419;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

### 3. 日志轮转

```bash
sudo vim /etc/logrotate.d/qchat
```

```
/opt/qchat/logs/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 0644 qchat qchat
    sharedscripts
    postrotate
        /bin/kill -HUP $(cat /var/run/syslogd.pid 2> /dev/null) 2> /dev/null || true
    endscript
}
```

### 4. 监控告警

建议使用 Prometheus + Grafana 监控：
- 服务器 CPU、内存、磁盘
- 服务进程状态
- 网络连接数
- 消息吞吐量
