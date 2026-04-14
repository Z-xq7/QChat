#!/bin/bash
# ============================================
# QChat 服务器部署脚本 (Ubuntu 22.04)
# 用法: chmod +x deploy.sh && sudo ./deploy.sh
# ============================================

set -e

echo "=========================================="
echo "   QChat 服务器部署脚本"
echo "   Ubuntu 22.04 LTS"
echo "=========================================="

# ---- 1. 更新系统 & 安装基础工具 ----
echo ""
echo "[1/7] 更新系统并安装基础工具..."
apt update && apt upgrade -y
apt install -y build-essential cmake git wget curl software-properties-common

# ---- 2. 安装依赖库 ----
echo ""
echo "[2/7] 安装 C++ 依赖库..."

# Boost 1.74
apt install -y libboost-all-dev

# MySQL Connector/C++ 8.x
# Ubuntu 22.04 默认仓库提供 libmysqlcppconn-dev (基于 mysql-connector-c++ 8.x)
apt install -y libmysqlcppconn-dev

# Redis C 客户端 (hiredis)
apt install -y libhiredis-dev

# JSON 库
apt install -y libjsoncpp-dev

# OpenSSL
apt install -y libssl-dev

# pkg-config (用于查找依赖)
apt install -y pkg-config

# ---- 3. 安装 gRPC 和 protobuf ----
echo ""
echo "[3/7] 安装 gRPC 和 protobuf..."
# Ubuntu 22.04 仓库中的 gRPC 版本为 1.46+
apt install -y libgrpc++-dev protobuf-compiler-grpc protobuf-compiler libprotobuf-dev

# ---- 4. 验证安装 ----
echo ""
echo "[4/7] 验证依赖安装..."

echo -n "  cmake: "
cmake --version | head -1

echo -n "  g++: "
g++ --version | head -1

echo -n "  protoc: "
protoc --version

echo -n "  grpc_cpp_plugin: "
which grpc_cpp_plugin

echo -n "  pkg-config jsoncpp: "
pkg-config --modversion jsoncpp 2>/dev/null || echo "未找到 (将使用 find_library)"

echo -n "  hiredis.h: "
find /usr/include -name "hiredis.h" | head -1 || echo "未找到"

echo -n "  mysql_driver.h: "
find /usr/include -name "mysql_driver.h" | head -1 || echo "未找到"

echo ""
echo "[5/7] 所有依赖安装完成！"

# ---- 5. 编译服务 ----
echo ""
echo "[6/7] 开始编译各服务..."

PROJECT_DIR="/opt/QChat"
BUILD_DIR="${PROJECT_DIR}/build"

# 创建项目目录
mkdir -p "${PROJECT_DIR}"
mkdir -p "${BUILD_DIR}"

# 提示用户上传代码
echo ""
echo "=========================================="
echo "  请将项目代码上传到 ${PROJECT_DIR}/"
echo "  需要包含以下目录:"
echo "    - ChatServer/"
echo "    - GateServer/"
echo "    - StatusServer/"
echo "    - ResourceServer/"
echo "=========================================="
echo ""
echo "代码上传完成后，按 Enter 继续..."
read -p ""

# 编译每个服务
for SERVER in ChatServer GateServer StatusServer ResourceServer; do
    echo ""
    echo "--- 编译 ${SERVER} ---"
    mkdir -p "${BUILD_DIR}/${SERVER}"
    cd "${BUILD_DIR}/${SERVER}"
    cmake "${PROJECT_DIR}/${SERVER}" -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    
    # 复制可执行文件和配置到部署目录
    DEPLOY_DIR="${PROJECT_DIR}/deploy/${SERVER}"
    mkdir -p "${DEPLOY_DIR}"
    cp "${BUILD_DIR}/${SERVER}/${SERVER}" "${DEPLOY_DIR}/"
    cp "${PROJECT_DIR}/${SERVER}/config.ini" "${DEPLOY_DIR}/"
    
    echo "  ${SERVER} 编译成功 -> ${DEPLOY_DIR}/${SERVER}"
done

# ---- 6. 安装 systemd 服务 ----
echo ""
echo "[7/7] 配置 systemd 服务..."

for SERVER in ChatServer GateServer StatusServer ResourceServer; do
    SERVICE_FILE="/etc/systemd/system/qchat-${SERVER,,}.service"
    # 转换为小写
    SERVER_LOWER=$(echo "${SERVER}" | tr '[:upper:]' '[:lower:]')
    SERVICE_FILE="/etc/systemd/system/qchat-${SERVER_LOWER}.service"
    
    # 工作目录
    WORK_DIR="${PROJECT_DIR}/deploy/${SERVER}"
    
    cat > "${SERVICE_FILE}" <<EOF
[Unit]
Description=QChat ${SERVER}
After=network.target

[Service]
Type=simple
WorkingDirectory=${WORK_DIR}
ExecStart=${WORK_DIR}/${SERVER}
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

# 安全设置
NoNewPrivileges=true
ProtectSystem=strict
ReadWritePaths=${WORK_DIR}

[Install]
WantedBy=multi-user.target
EOF

    systemctl daemon-reload
    systemctl enable "qchat-${SERVER_LOWER}"
    echo "  已安装 qchat-${SERVER_LOWER}.service"
done

echo ""
echo "=========================================="
echo "   部署完成！"
echo "=========================================="
echo ""
echo "服务管理命令:"
echo "  启动所有:   sudo systemctl start qchat-*"
echo "  停止所有:   sudo systemctl stop qchat-*"
echo "  查看状态:   sudo systemctl status qchat-*"
echo "  查看日志:   journalctl -u qchat-gateserver -f"
echo ""
echo "部署目录: ${PROJECT_DIR}/deploy/"
echo ""
echo "启动前请确认 config.ini 中的配置正确！"
