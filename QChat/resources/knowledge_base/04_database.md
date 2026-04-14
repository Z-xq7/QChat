# QChat 数据库设计

## 数据库概述

QChat 使用 MySQL 作为主数据库，存储用户信息、好友关系、聊天记录等持久化数据。

## 数据表清单

| 表名 | 说明 |
|------|------|
| user | 用户基本信息 |
| friend_apply | 好友申请表 |
| friend_relationship | 好友关系表 |
| chat_thread | 聊天会话表 |
| chat_message | 聊天消息表 |
| group_chat | 群聊信息表 |
| group_member | 群成员表 |

## 表结构详解

### 1. user（用户表）

```sql
CREATE TABLE user (
    id INT PRIMARY KEY AUTO_INCREMENT,
    uid INT UNIQUE NOT NULL,
    name VARCHAR(64) NOT NULL,
    nick VARCHAR(128),
    email VARCHAR(128) UNIQUE NOT NULL,
    pwd VARCHAR(256) NOT NULL,
    icon VARCHAR(256) DEFAULT '',
    desc TEXT,
    sex TINYINT DEFAULT 0 COMMENT '0-未知, 1-男, 2-女',
    status TINYINT DEFAULT 0 COMMENT '0-正常, 1-禁用',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_email (email),
    INDEX idx_uid (uid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

**字段说明**：
- `uid`: 用户唯一标识，业务层使用
- `name`: 用户账号名
- `nick`: 用户昵称
- `icon`: 头像文件名
- `desc`: 个人描述

### 2. friend_apply（好友申请表）

```sql
CREATE TABLE friend_apply (
    id INT PRIMARY KEY AUTO_INCREMENT,
    from_uid INT NOT NULL,
    to_uid INT NOT NULL,
    apply_content VARCHAR(256) DEFAULT '',
    apply_name VARCHAR(128),
    status TINYINT DEFAULT 0 COMMENT '0-待处理, 1-已同意, 2-已拒绝',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_apply (from_uid, to_uid),
    INDEX idx_to_uid (to_uid),
    FOREIGN KEY (from_uid) REFERENCES user(uid),
    FOREIGN KEY (to_uid) REFERENCES user(uid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 3. friend_relationship（好友关系表）

```sql
CREATE TABLE friend_relationship (
    id INT PRIMARY KEY AUTO_INCREMENT,
    uid INT NOT NULL,
    friend_uid INT NOT NULL,
    friend_name VARCHAR(128) COMMENT '备注名',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_friend (uid, friend_uid),
    INDEX idx_uid (uid),
    INDEX idx_friend_uid (friend_uid),
    FOREIGN KEY (uid) REFERENCES user(uid),
    FOREIGN KEY (friend_uid) REFERENCES user(uid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

**说明**：
- 双向好友关系会插入两条记录 (uid, friend_uid) 和 (friend_uid, uid)
- `friend_name` 是用户对好友的备注名

### 4. chat_thread（聊天会话表）

```sql
CREATE TABLE chat_thread (
    id INT PRIMARY KEY AUTO_INCREMENT,
    thread_id INT UNIQUE NOT NULL,
    thread_type TINYINT DEFAULT 0 COMMENT '0-私聊, 1-群聊',
    creator_uid INT NOT NULL,
    name VARCHAR(128) COMMENT '群聊名称',
    icon VARCHAR(256) DEFAULT '',
    notice TEXT COMMENT '群公告',
    member_count INT DEFAULT 2,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_creator (creator_uid),
    INDEX idx_thread_id (thread_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 5. chat_message（聊天消息表）

```sql
CREATE TABLE chat_message (
    id INT PRIMARY KEY AUTO_INCREMENT,
    msg_id BIGINT UNIQUE NOT NULL,
    thread_id INT NOT NULL,
    sender_uid INT NOT NULL,
    receiver_uid INT NOT NULL,
    msg_type TINYINT DEFAULT 0 COMMENT '0-文本, 1-图片, 2-视频, 3-文件',
    content TEXT,
    file_url VARCHAR(512),
    file_size BIGINT DEFAULT 0,
    file_name VARCHAR(256),
    status TINYINT DEFAULT 0 COMMENT '0-未读, 1-已读, 2-发送失败',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_thread_id (thread_id),
    INDEX idx_sender (sender_uid),
    INDEX idx_receiver (receiver_uid),
    INDEX idx_created_at (created_at),
    INDEX idx_thread_msg (thread_id, msg_id DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

**字段说明**：
- `msg_id`: 消息唯一 ID，使用雪花算法生成
- `content`: 文本消息内容，或图片/文件的描述
- `file_url`: 文件存储路径
- `status`: 消息状态，用于已读回执

**索引设计**：
- `idx_thread_msg`: 用于按会话加载历史消息（最常用查询）
- `idx_sender/receiver`: 用于统计和搜索

### 6. group_chat（群聊表）

```sql
CREATE TABLE group_chat (
    id INT PRIMARY KEY AUTO_INCREMENT,
    thread_id INT UNIQUE NOT NULL,
    group_name VARCHAR(128) NOT NULL,
    creator_uid INT NOT NULL,
    icon VARCHAR(256) DEFAULT '',
    notice TEXT,
    member_count INT DEFAULT 1,
    max_member INT DEFAULT 500,
    status TINYINT DEFAULT 0 COMMENT '0-正常, 1-解散',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_creator (creator_uid),
    INDEX idx_thread_id (thread_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 7. group_member（群成员表）

```sql
CREATE TABLE group_member (
    id INT PRIMARY KEY AUTO_INCREMENT,
    thread_id INT NOT NULL,
    uid INT NOT NULL,
    role TINYINT DEFAULT 0 COMMENT '0-成员, 1-管理员, 2-群主',
    group_name VARCHAR(128) COMMENT '群内昵称',
    join_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_ack_msg_id BIGINT DEFAULT 0 COMMENT '最后确认的消息ID',
    UNIQUE KEY uk_member (thread_id, uid),
    INDEX idx_thread (thread_id),
    INDEX idx_uid (uid),
    FOREIGN KEY (thread_id) REFERENCES group_chat(thread_id),
    FOREIGN KEY (uid) REFERENCES user(uid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

## 核心查询示例

### 1. 加载会话历史消息

```sql
-- 分页加载，从 last_msg_id 往前取 20 条
SELECT * FROM chat_message 
WHERE thread_id = ? AND msg_id < ? 
ORDER BY msg_id DESC 
LIMIT 20;
```

### 2. 获取用户的所有会话列表

```sql
-- 获取私聊会话
SELECT t.*, u.name, u.nick, u.icon 
FROM chat_thread t
JOIN friend_relationship f ON (f.friend_uid = t.creator_uid OR f.uid = t.creator_uid)
JOIN user u ON u.uid = CASE WHEN t.creator_uid = ? THEN f.friend_uid ELSE t.creator_uid END
WHERE f.uid = ? OR f.friend_uid = ?;

-- 获取群聊会话
SELECT g.* FROM group_chat g
JOIN group_member m ON g.thread_id = m.thread_id
WHERE m.uid = ?;
```

### 3. 获取未读消息数

```sql
-- 查询每个会话的未读消息数
SELECT thread_id, COUNT(*) as unread_count 
FROM chat_message 
WHERE receiver_uid = ? AND status = 0 
GROUP BY thread_id;
```

### 4. 标记消息已读

```sql
-- 将会话中发送给当前用户的所有未读消息标记为已读
UPDATE chat_message 
SET status = 1 
WHERE thread_id = ? AND receiver_uid = ? AND status = 0;
```

## Redis 缓存设计

### Key 命名规范

| Key 模式 | 说明 | 过期时间 |
|----------|------|----------|
| `verify:{email}` | 邮箱验证码 | 5 分钟 |
| `token:{uid}` | 用户登录 Token | 7 天 |
| `session:{uid}` | 用户会话信息 | 7 天 |
| `online:{uid}` | 用户在线状态 | - |
| `chat_server:{server_id}` | ChatServer 注册信息 | 30 秒 |

### 常用操作

```bash
# 存储验证码
SET verify:zhangsan@example.com 123456 EX 300

# 验证验证码
GET verify:zhangsan@example.com

# 存储 Token
SET token:1001 "jwt_token_string" EX 604800

# 检查 Token
GET token:1001

# 设置用户在线
SET online:1001 "chat_server_1"

# 获取用户在线状态
GET online:1001

# 删除用户在线状态（下线）
DEL online:1001
```

## 数据库优化建议

### 1. 分表策略

当 `chat_message` 表数据量过大时（超过 1 亿条），可按 `thread_id` 分表：

```sql
-- 分表示例：chat_message_0, chat_message_1, ..., chat_message_99
-- 根据 thread_id % 100 决定存储到哪个表
```

### 2. 归档策略

- 3 个月前的消息归档到历史表
- 1 年前的消息压缩存储到对象存储（OSS/S3）

### 3. 读写分离

- 写操作：主库
- 读操作：从库
- 消息发送写入主库，历史消息查询从从库读取

## 数据一致性保障

### 1. 好友关系一致性

添加好友时，需要同时写入：
1. `friend_apply` 表更新状态
2. `friend_relationship` 表插入双向记录

使用事务保证原子性。

### 2. 消息投递一致性

发送消息时：
1. 先写入 MySQL
2. 再推送给在线用户
3. 如果推送失败，用户登录后拉取

### 3. 群聊成员一致性

创建群聊时：
1. 插入 `group_chat` 记录
2. 插入 `group_member` 记录（包含创建者）
3. 插入 `chat_thread` 记录

使用事务保证原子性。
