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
