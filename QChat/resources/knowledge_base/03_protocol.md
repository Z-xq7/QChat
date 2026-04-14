# QChat 网络协议文档

## 协议概述

QChat 使用自定义二进制协议进行客户端-服务器通信，协议 ID 范围为 1001-1094。

## 协议 ID 列表

### 认证相关 (1001-1004)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1001 | ID_GET_VARIFY_CODE | C->S | 获取验证码 |
| 1002 | ID_REG_USER | C->S | 注册用户 |
| 1003 | ID_RESET_PWD | C->S | 重置密码 |
| 1004 | ID_LOGIN_USER | C->S | 用户登录 |

### 聊天服务器登录 (1005-1006)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1005 | ID_CHAT_LOGIN | C->S | 登录聊天服务器 |
| 1006 | ID_CHAT_LOGIN_RSP | S->C | 登录聊天服务器回包 |

**ID_CHAT_LOGIN 请求格式**：
```json
{
    "uid": 1001,
    "token": "xxx"
}
```

**ID_CHAT_LOGIN_RSP 响应格式**：
```json
{
    "error": 0,
    "uid": 1001
}
```

### 用户搜索 (1007-1008)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1007 | ID_SEARCH_USER_REQ | C->S | 用户搜索请求 |
| 1008 | ID_SEARCH_USER_RSP | S->C | 搜索用户回包 |

**请求格式**：
```json
{
    "uid": 1002
}
```

**响应格式**：
```json
{
    "error": 0,
    "uid": 1002,
    "name": "张三",
    "nick": "张三昵称",
    "icon": "avatar.png",
    "desc": "个人描述"
}
```

### 好友添加 (1009-1011)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1009 | ID_ADD_FRIEND_REQ | C->S | 添加好友申请 |
| 1010 | ID_ADD_FRIEND_RSP | S->C | 申请添加好友回复 |
| 1011 | ID_NOTIFY_ADD_FRIEND_REQ | S->C | 通知用户添加好友申请 |

**ID_ADD_FRIEND_REQ 请求格式**：
```json
{
    "uid": 1001,
    "apply_uid": 1002,
    "apply_name": "我是张三",
    "apply_content": "你好，想加你好友"
}
```

### 好友认证 (1013-1015)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1013 | ID_AUTH_FRIEND_REQ | C->S | 认证好友请求（同意/拒绝） |
| 1014 | ID_AUTH_FRIEND_RSP | S->C | 认证好友回复 |
| 1015 | ID_NOTIFY_AUTH_FRIEND_REQ | S->C | 通知用户认证好友申请 |

**ID_AUTH_FRIEND_REQ 请求格式**：
```json
{
    "uid": 1001,
    "auth_uid": 1002,
    "auth_content": "同意",
    "is_auth": true
}
```

### 文本消息 (1017-1019)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1017 | ID_TEXT_CHAT_MSG_REQ | C->S | 文本聊天信息请求 |
| 1018 | ID_TEXT_CHAT_MSG_RSP | S->C | 文本聊天信息回复 |
| 1019 | ID_NOTIFY_TEXT_CHAT_MSG_REQ | S->C | 通知用户文本聊天信息 |

**ID_TEXT_CHAT_MSG_REQ 请求格式**：
```json
{
    "from_uid": 1001,
    "to_uid": 1002,
    "thread_id": 123,
    "content": "你好",
    "msg_id": 456
}
```

### 系统通知 (1021-1024)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1021 | ID_NOTIFY_OFF_LINE_REQ | S->C | 通知用户下线（异地登录） |
| 1023 | ID_HEART_BEAT_REQ | C->S | 心跳请求 |
| 1024 | ID_HEART_BEAT_RSP | S->C | 心跳回复 |

**心跳机制**：
- 客户端每 30 秒发送一次心跳
- 服务器 60 秒未收到心跳则断开连接

### 聊天线程管理 (1025-1028)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1025 | ID_LOAD_CHAT_THREAD_REQ | C->S | 加载聊天线程请求 |
| 1026 | ID_LOAD_CHAT_THREAD_RSP | S->C | 加载聊天线程回复 |
| 1027 | ID_CREATE_PRIVATE_CHAT_REQ | C->S | 创建私聊线程请求 |
| 1028 | ID_CREATE_PRIVATE_CHAT_RSP | S->C | 创建私聊线程回复 |

### 历史消息 (1029-1030)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1029 | ID_LOAD_CHAT_MSG_REQ | C->S | 加载聊天消息请求 |
| 1030 | ID_LOAD_CHAT_MSG_RSP | S->C | 加载聊天消息回复 |

**ID_LOAD_CHAT_MSG_REQ 请求格式**：
```json
{
    "thread_id": 123,
    "last_msg_id": 100,
    "count": 20
}
```

### 头像上传 (1031-1032)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1031 | ID_UPLOAD_HEAD_ICON_REQ | C->S | 上传头像请求 |
| 1032 | ID_UPLOAD_HEAD_ICON_RSP | S->C | 上传头像回复 |

### 图片消息 (1035-1039)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1035 | ID_IMG_CHAT_MSG_REQ | C->S | 图片聊天消息请求 |
| 1036 | ID_IMG_CHAT_MSG_RSP | S->C | 图片聊天消息回复 |
| 1037 | ID_IMG_CHAT_UPLOAD_REQ | C->S | 上传聊天图片资源 |
| 1038 | ID_IMG_CHAT_UPLOAD_RSP | S->C | 上传聊天图片资源回复 |
| 1039 | ID_NOTIFY_IMG_CHAT_MSG_REQ | S->C | 通知用户图片聊天信息 |

### 文件消息 (1041-1048)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1041 | ID_FILE_INFO_SYNC_REQ | C->S | 文件信息同步请求 |
| 1042 | ID_FILE_INFO_SYNC_RSP | S->C | 文件信息同步回复 |
| 1043 | ID_IMG_CHAT_CONTINUE_UPLOAD_REQ | C->S | 续传聊天图片资源请求 |
| 1044 | ID_IMG_CHAT_CONTINUE_UPLOAD_RSP | S->C | 续传聊天图片资源回复 |
| 1045 | ID_IMG_CHAT_DOWN_INFO_SYNC_REQ | C->S | 获取图片下载信息同步请求 |
| 1046 | ID_IMG_CHAT_DOWN_INFO_SYNC_RSP | S->C | 获取图片下载信息同步回复 |
| 1047 | ID_IMG_CHAT_DOWN_REQ | C->S | 聊天图片下载请求 |
| 1048 | ID_IMG_CHAT_DOWN_RSP | S->C | 聊天图片下载回复 |

### 视频通话 (1050-1061)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1050 | ID_CALL_INVITE_REQ | C->S | 视频通话邀请请求 |
| 1051 | ID_CALL_INVITE_RSP | S->C | 视频通话邀请响应 |
| 1052 | ID_CALL_INCOMING_NOTIFY | S->C | 来电通知 |
| 1053 | ID_CALL_ACCEPT_REQ | C->S | 接受通话请求 |
| 1054 | ID_CALL_ACCEPT_RSP | S->C | 接受通话响应 |
| 1055 | ID_CALL_ACCEPT_NOTIFY | S->C | 接受通话通知 |
| 1056 | ID_CALL_REJECT_REQ | C->S | 拒绝通话请求 |
| 1057 | ID_CALL_REJECT_RSP | S->C | 拒绝通话响应 |
| 1058 | ID_CALL_REJECT_NOTIFY | S->C | 拒绝通话通知 |
| 1059 | ID_CALL_HANGUP_REQ | C->S | 挂断通话请求 |
| 1060 | ID_CALL_HANGUP_RSP | S->C | 挂断通话响应 |
| 1061 | ID_CALL_HANGUP_NOTIFY | S->C | 挂断通话通知 |

**ID_CALL_INVITE_REQ 请求格式**：
```json
{
    "call_id": "uuid",
    "caller_uid": 1001,
    "callee_uid": 1002
}
```

**ID_CALL_ACCEPT_NOTIFY 格式**：
```json
{
    "call_id": "uuid",
    "room_id": "room_xxx",
    "turn_ws_url": "ws://turn.example.com:8080",
    "ice_servers": [
        {"urls": "turn:turn.example.com:3478", "username": "user", "credential": "pass"}
    ]
}
```

### 文件传输 (1062-1066)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1062 | ID_FILE_CHAT_MSG_REQ | C->S | 文件聊天消息请求 |
| 1063 | ID_FILE_CHAT_MSG_RSP | S->C | 文件聊天消息回复 |
| 1064 | ID_NOTIFY_FILE_CHAT_MSG_REQ | S->C | 通知用户文件聊天信息 |
| 1065 | ID_FILE_CHAT_DOWN_REQ | C->S | 聊天文件下载请求 |
| 1066 | ID_FILE_CHAT_DOWN_RSP | S->C | 聊天文件下载回复 |

### 群聊 (1070-1077)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1070 | ID_CREATE_GROUP_CHAT_REQ | C->S | 创建群聊请求 |
| 1071 | ID_CREATE_GROUP_CHAT_RSP | S->C | 创建群聊响应 |
| 1072 | ID_NOTIFY_GROUP_CHAT_CREATED | S->C | 通知用户被加入群聊 |
| 1073 | ID_GET_GROUP_MEMBERS_REQ | C->S | 获取群成员列表请求 |
| 1074 | ID_GET_GROUP_MEMBERS_RSP | S->C | 获取群成员列表响应 |
| 1075 | ID_UPDATE_GROUP_NOTICE_REQ | C->S | 更新群公告请求 |
| 1076 | ID_UPDATE_GROUP_NOTICE_RSP | S->C | 更新群公告响应 |
| 1077 | ID_NOTIFY_GROUP_NOTICE_UPDATE | S->C | 通知群成员群公告更新 |

### 在线状态 (1080-1083)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1080 | ID_GET_FRIEND_ONLINE_STATUS_REQ | C->S | 查询好友在线状态请求 |
| 1081 | ID_GET_FRIEND_ONLINE_STATUS_RSP | S->C | 查询好友在线状态响应 |
| 1082 | ID_NOTIFY_USER_ONLINE | S->C | 通知好友用户上线 |
| 1083 | ID_NOTIFY_USER_OFFLINE | S->C | 通知好友用户下线 |

### 已读回执 (1090-1094)

| 协议 ID | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 1090 | ID_GET_UNREAD_COUNTS_REQ | C->S | 获取未读消息数请求 |
| 1091 | ID_GET_UNREAD_COUNTS_RSP | S->C | 获取未读消息数响应 |
| 1092 | ID_MARK_MSG_READ_REQ | C->S | 标记消息已读请求 |
| 1093 | ID_MARK_MSG_READ_RSP | S->C | 标记消息已读响应 |
| 1094 | ID_NOTIFY_MSG_READ | S->C | 通知发送方消息已读 |

## 消息格式

### TCP 包头结构

```
+--------+--------+--------+--------+--------+--------+--------+
|  ID    |  ID    |  Len   |  Len   |  Len   |  Len   |  Data  |
| (High) | (Low)  | (Byte0)| (Byte1)| (Byte2)| (Byte3)|        |
+--------+--------+--------+--------+--------+--------+--------+
   2字节     2字节              4字节                    N字节
```

- **ID**: 协议 ID (2 字节，大端序)
- **Len**: 数据长度 (4 字节，大端序)
- **Data**: JSON 格式的消息体

### 文件传输包头

```cpp
#define FILE_UPLOAD_HEAD_LEN 6    // 包头长度
#define FILE_UPLOAD_ID_LEN 2      // ID 长度
#define FILE_UPLOAD_LEN_LEN 4     // 长度字段长度
#define MAX_FILE_LEN (1024*256)   // 最大分片大小 256KB
```

## 错误码

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0 | SUCCESS | 成功 |
| 1 | ERR_JSON | JSON 解析失败 |
| 2 | ERR_NETWORK | 网络请求失败 |
| 100 | ERR_USER_NOT_FOUND | 用户不存在 |
| 101 | ERR_WRONG_PASSWORD | 密码错误 |
| 102 | ERR_USER_EXISTS | 用户已存在 |
| 103 | ERR_INVALID_TOKEN | Token 无效 |
| 104 | ERR_SERVER_FULL | 服务器已满 |
