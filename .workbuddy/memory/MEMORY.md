# QChat 项目长期记忆

## 项目概要
- Qt 6 + MinGW + metartc7 (C API YangPeerConnection) + Node.js 信令服务 + coturn TURN 服务器
- P2P 视频通话应用，使用 WebSocket 信令交换 SDP
- 服务器：ChatServer (C++ TCP)、GateServer (WebSocket网关)、TurnServer (Node.js信令)、ResourceServer、StatusServer、VarifyServer

## metartc7 YangPeerConnection C API 关键知识（2026-04-03 确认）

### 正确的 P2P 初始化顺序
1. `memset(&peerconn, 0)` 清零
2. 设置 `peer.avinfo = &avinfo` 和 `peer.streamconfig` 全部字段（direction/isControlled/recvCallback/rtcCallback/localPort/remoteIp/remotePort）
3. **配置 ICE 服务器**（设置 `avinfo->rtc.iceCandidateType = YangIceTurn` 等）
4. `yang_create_peerConnection(&peerconn)` — **内部自动调用 `yang_pc_init()`**，读取 avinfo/streamconfig 创建 YangRtcConnection
   - ⚠️ **必须**在设置 iceCandidateType 之后调用！`yang_create_ice` 会**复制**（非引用）`avinfo->rtc.iceCandidateType`，之后再改 avinfo 无效
5. `addAudioTrack` / `addVideoTrack` / `addTransceiver`
6. SDP 交换：Offerer: createOffer → 发送 → 收到 answer → setRemoteDescription → setLocalDescription；Answerer: 收到 offer → setRemoteDescription → createAnswer → 发送 → setLocalDescription
7. 轮询 `isConnected(peer)` 等待 ICE/DTLS 完成

### 关键 API 行为
- `yang_create_peerConnection` 内部最后一行自动调用 `yang_pc_init()`（创建 YangRtcConnection/session/context/ice/srtp/dtls）
- `yang_pc_init()` 有 `if(peer->conn!=NULL) return` 保护，重复调用无效
- `connectSfuServer` 只在 SRS/ZLM 模式下有实现，P2P 模式是 `yang_srs_connectRtcServer`（不是 P2P 专用）
- P2P 模式：ICE 由 metartc 内部在 `setRemoteDescription` 解析 ICE candidates 后自动启动
- `addTransceiver(direction)` 需要在 addTrack 之后调用（demo 中确认）
- P2P SDP 行尾：Offerer 用 `\n`，Answerer 用 `\r\n`

### 常见错误
- **绝不能** 在 `yang_create_peerConnection` 之后才设 avinfo（会导致内部 init 用 NULL avinfo）
- **绝不能** 在 `yang_create_peerConnection` 之后 memset streamconfig（会清除内部填充的 sslCallback 等指针）
- **绝不能** 在 P2P 模式调 connectSfuServer（函数指针指向 SRS 逻辑，会导致错误）
- **绝不能** 在 `configureIceServers` 之前调 `yang_create_peerConnection`（yang_create_ice 复制 iceCandidateType，之后改 avinfo 不影响 ICE session）
- `createAnswer()` 内部强制 `isControlled = yangtrue`，`createOffer()` 不强制（保持 streamconfig 中的值）

## 分页加载 / 虚拟滚动（2026-04-15 完成 v2 架构重构）

### 核心架构（正确的分页方案）
- **登录时**：只加载最新消息（单次请求，不链式加载全部历史）
- **滚动到顶部**：请求更早消息（反向分页），追加到 `_msg_map` 并头插到 UI
- **`_msg_map` 存储**：初始消息 ASC 存储，历史消息 DESC 存储（服务器返回 DESC）
- **`SetChatData` 重建**：`m_history_loaded` 标志控制，保留 `_msg_map` 中的所有消息

### 服务器改动
- `ChatServer/const.h`：新增 `ID_LOAD_CHAT_HISTORY_REQ=1095`、`ID_LOAD_CHAT_HISTORY_RSP=1096`
- `ChatServer/MysqlDao.cpp::LoadChatMsg`：
  - `last_message_id==0`（初始加载）：`WHERE message_id>0 ORDER BY message_id DESC LIMIT 20` → 返回最新消息
  - `last_message_id>0`（历史加载）：`WHERE message_id<last_id ORDER BY message_id DESC LIMIT 20` → 返回更早消息
  - `next_cursor = messages.back().message_id`（最旧消息ID，作为下次分页游标）
- `ChatServer/MysqlDao.cpp::LoadChatHistory`：调用 `LoadChatMsg(thread_id, cursor, page_size)`（共享逻辑）
- `ChatServer/LogicSystem.cpp`：新增 `LoadChatHistory` 处理函数和注册

### 客户端改动
- `QChat/global.h`：新增 `ID_LOAD_CHAT_HISTORY_REQ=1095`、`ID_LOAD_CHAT_HISTORY_RSP=1096`
- `QChat/tcpmgr.h/cpp`：
  - 新增 `sig_load_chat_history` 信号
  - 新增 `ID_LOAD_CHAT_HISTORY_RSP` 处理器（解析 `first_message_id`/`has_more`/chat_datas）
- `QChat/chatdialog.h/cpp`：
  - 新增 `slot_load_chat_history` 处理函数（存入 `_msg_map` 并 emit `sig_prepend_chat_msg`）
  - `slot_load_chat_msg`（路径B）：**删除链式加载**，仅单次请求
  - `slot_request_load_history`：改用 `ID_LOAD_CHAT_HISTORY_REQ`，参数 `cursor=oldest_rendered_msg_id`
  - `SetSelectChatPage()`/`slot_item_clicked()`：调用 `SetChatData` 前设置 `ui->chat_page->m_history_loaded = false`
- `QChat/chatpage.h/cpp`：
  - 新增 `m_history_loaded` 标志
  - `slot_load_more_history_rsp`：设置 `m_history_loaded=true`，反向迭代 `PrependChatMsg` 保证顺序
  - `SetChatData`：新增 `m_history_loaded` 分支 — 从 `_msg_map` 重建列表（`_msg_map` 含初始+历史）

### PrependChatMsg 顺序要点
- `PrependChatMsg` 插入到位置 0（列表顶部）
- 头插 `[msg_51, msg_52, ..., msg_60]` → 最终列表中：`msg_60` 在顶（新加载中最旧）、`msg_51` 在底（新加载中最旧）
- **反向迭代**：`for (auto it = chat_datas.rbegin(); it != chat_datas.rend(); ++it)` 保证 oldest 在底部
- `_msg_map` 中历史消息 DESC 存储 → `SetChatData` 排序 ASC 后：`H_newest` 在顶、`H_oldest` 在 `I_oldest` 之上 → 正确视觉顺序



