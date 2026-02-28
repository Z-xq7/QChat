# QChat 系统架构文档

QChat是一个基于C++和Node.js的分布式聊天系统，包含五个核心服务器组件：GateServer、ChatServer、ChatServer2、StatusServer和VarifyServer。系统使用gRPC进行服务间通信，Redis作为缓存和会话管理，MySQL存储用户数据。

## 1. GateServer（网关服务器）

### 文件结构
- `GateServer.cpp` - 主程序入口
- `CServer.cpp/h` - 服务器核心类
- `HttpConnection.cpp/h` - HTTP连接处理
- `LogicSystem.cpp/h` - 业务逻辑处理
- `config.ini` - 配置文件
- 各种通用组件文件（Redis、MySQL、配置管理等）

### 核心功能
- 作为系统的HTTP入口点，接收客户端的HTTP请求
- 提供用户注册、登录、密码重置和验证码获取等API接口
- 与VarifyServer和StatusServer通过gRPC通信
- 处理GET和POST请求的路由

### 详细实现
**GateServer.cpp**: 程序入口，创建io_context并启动服务器，监听指定端口（默认13419），处理SIGINT和SIGTERM信号

**CServer.cpp**: 继承自shared_from_this，创建并接受HTTP连接，使用AsioIOServicePool管理IO服务

**HttpConnection.cpp**: 处理HTTP请求和响应，包含URL编码/解码功能，异步读取客户端请求，解析GET参数，处理POST请求体，发送HTTP响应

**LogicSystem.cpp**: 核心业务逻辑处理器，注册并处理以下API：
- `/get_test`: 测试接口
- `/get_code`: 获取邮箱验证码，通过gRPC调用VarifyServer发送邮件
- `/user_register`: 用户注册，验证邮箱验证码，将用户信息存入MySQL
- `/reset_pwd`: 重置密码，验证邮箱和验证码后更新用户密码
- `/user_login`: 用户登录，通过gRPC调用StatusServer获取聊天服务器信息

## 2. ChatServer（聊天服务器1）

### 文件结构
- `ChatServer.cpp` - 主程序入口
- `CServer.cpp/h` - 服务器核心类
- `CSession.cpp/h` - 客户端会话管理
- `LogicSystem.cpp/h` - 消息处理逻辑
- `ChatServiceImpl.cpp/h` - gRPC服务实现
- `UserMgr.cpp/h` - 用户管理
- `config.ini` - 配置文件
- 各种通用组件文件

### 核心功能
- 接收客户端TCP连接，处理聊天消息
- 实现心跳机制，管理用户会话
- 与StatusServer通信，管理在线用户状态
- 实现用户登录、好友管理、消息传递等功能

### 详细实现
**ChatServer.cpp**: 程序入口，创建ASIO IO服务池，启动gRPC服务，创建TCP服务器监听客户端连接，设置信号处理机制

**CServer.cpp**: TCP服务器实现，使用Boost.Asio异步接受客户端连接，管理所有客户端会话，包含心跳检测机制每60秒更新在线用户数到Redis

**CSession.cpp**: 客户端会话管理，处理消息头和消息体的异步读取，实现TCP粘包处理，包含发送队列管理，处理心跳超时检测（20秒无心跳则断开连接）

**LogicSystem.cpp**: 消息处理核心，使用队列和工作线程异步处理客户端消息，注册消息回调处理函数：
- `MSG_CHAT_LOGIN`: 处理用户登录请求，验证Token，设置用户会话状态
- `ID_HEART_BEAT_REQ`: 处理心跳请求，保持连接活跃

**UserMgr.cpp**: 用户管理模块，管理用户ID与会话的映射关系，提供用户会话的增删改查功能

## 3. ChatServer2（聊天服务器2）

### 文件结构
与ChatServer完全相同，只是配置不同，用于负载均衡

### 核心功能
功能与ChatServer相同，但通过不同端口运行（端口8091，gRPC端口50056），实现分布式聊天服务

### 配置差异
- 服务器名称: chatserver2
- 监听端口: 8091
- gRPC端口: 50056
- 负责处理chatserver1的对等服务器通信

## 4. StatusServer（状态服务器）

### 文件结构
- `StatusServer.cpp` - 主程序入口
- `StatusServiceImpl.cpp/h` - gRPC服务实现
- `config.ini` - 配置文件
- 各种通用组件文件

### 核心功能
- 为客户端提供聊天服务器负载均衡
- 根据当前负载选择最空闲的聊天服务器
- 管理用户Token，验证登录状态
- 与Redis通信获取实时连接数

### 详细实现
**StatusServer.cpp**: gRPC服务器入口，监听端口50052，注册StatusServiceImpl服务，处理SIGINT信号

**StatusServiceImpl.cpp**: 
- `GetChatServer`: 根据负载选择最空闲的聊天服务器（通过Redis中存储的连接数判断），生成唯一Token并存储到Redis
- `getChatServer`: 内部方法，遍历配置的聊天服务器列表，选择当前连接数最少的服务器
- `Login`: 验证用户Token的有效性
- `insertToken`: 将用户Token存储到Redis进行会话管理

## 5. VarifyServer（验证服务器）

### 文件结构
- `server.js` - 主程序入口
- `email.js` - 邮件发送模块
- `redis.js` - Redis操作模块
- `config.js` - 配置模块
- `const.js` - 常量定义
- `proto.js` - gRPC协议定义
- `config.json` - JSON配置文件
- `package.json` - Node.js依赖配置

### 核心功能
- 通过gRPC服务提供验证码生成和发送功能
- 发送邮件验证码到用户邮箱
- 与Redis通信存储和验证验证码
- 使用QQ邮箱SMTP服务发送邮件

### 详细实现
**server.js**: 
- gRPC服务器入口，监听端口50051
- `GetVarifyCode`方法：接收邮箱地址，生成4位验证码，存储到Redis（5分钟过期），通过邮件发送给用户
- 使用UUID生成随机验证码，通过Redis检查是否已存在未过期的验证码

**email.js**: 
- 使用nodemailer创建SMTP连接（QQ邮箱SMTP）
- `SendMail`函数：异步发送邮件，返回Promise确保发送完成

**redis.js**:
- 使用ioredis连接Redis数据库
- 提供`GetRedis`, `SetRedisExpire`, `QueryRedis`等方法
- 支持键值存储和过期时间设置

## 系统架构关系

1. **客户端 -> GateServer**: HTTP请求进行注册、登录等操作
2. **GateServer -> VarifyServer**: gRPC调用获取验证码
3. **GateServer -> StatusServer**: gRPC调用获取聊天服务器信息
4. **客户端 -> ChatServer/ChatServer2**: TCP连接进行聊天
5. **ChatServer/ChatServer2 -> StatusServer**: gRPC通信管理用户状态
6. **所有服务器 -> Redis**: 存储会话、用户信息、连接数等
7. **所有服务器 -> MySQL**: 存储用户数据

## 技术栈

- **后端语言**: C++ (聊天服务器、网关服务器、状态服务器), JavaScript/Node.js (验证服务器)
- **通信协议**: gRPC, HTTP, TCP
- **数据库**: MySQL, Redis
- **异步框架**: Boost.Asio (C++), ioredis (Node.js)
- **其他**: JSON, Protobuf, SMTP

## 部署说明

1. 启动Redis和MySQL数据库
2. 启动VarifyServer (端口50051)
3. 启动StatusServer (端口50052)
4. 启动ChatServer (端口50055)
5. 启动ChatServer2 (端口50056)
6. 启动GateServer (端口13419)

整个系统采用分布式架构设计，支持多服务器负载均衡，具有良好的扩展性和稳定性。