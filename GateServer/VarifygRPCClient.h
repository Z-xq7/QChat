#pragma once
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "const.h"
#include "RPCConPool.h"

//ClientContext提供了调用RPC时的上下文信息
using grpc::ClientContext;
//Status表示RPC调用的结果状态
using grpc::Status;

//message命名空间内的GetVarifyReq和GetVarifyRsp分别表示RPC请求和应答消息体
using message::GetVarifyReq;
using message::GetVarifyRsp;
using message::VarifyService;

//gRPC客户端类，负责向gRPC服务器发送请求，接收响应
class VarifygRPCClient:public Singleton<VarifygRPCClient>
{
    friend class Singleton<VarifygRPCClient>;
public:
    //获取验证码的RPC调用函数
    GetVarifyRsp GetVarifyCode(std::string email);

private:
    VarifygRPCClient();
    //RPC连接池对象，管理到gRPC服务器的连接
    std::unique_ptr<RPCConPool<VarifyService>> _pool;
};

