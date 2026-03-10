#include "VarifygRPCClient.h"
#include "ConfigMgr.h"
#include "Logger.h"

VarifygRPCClient::VarifygRPCClient()
{
    auto& gCfgMgr = ConfigMgr::Instance();
    std::string grpc_server_host = gCfgMgr["VarifyServer"]["host"];
    std::string grpc_server_port = gCfgMgr["VarifyServer"]["port"];
    _pool.reset(new RPCConPool<VarifyService>(5, grpc_server_host, grpc_server_port));
}

//获取验证码RPC调用函数
GetVarifyRsp VarifygRPCClient::GetVarifyCode(std::string email)
{
    ClientContext context;
    // 设置 3 秒超时（建议根据网络延迟自行调整）
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));
    GetVarifyReq request;
    GetVarifyRsp response;
    request.set_email(email);
    //从连接池获取Stub连接
    auto stub = _pool->GetConnection();
    LOG_DEBUG("Obtained gRPC stub connection from pool");
    //调用RPC方法获取验证码
    Status status = stub->GetVarifyCode(&context, request, &response);
    //检测RPC调用结果状态
    if (status.ok()) {
        LOG_INFO("Successfully called GetVarifyCode RPC method");
        //归还Stub连接到连接池
        _pool->ReturnConnection(std::move(stub));
        LOG_DEBUG("Returned gRPC stub to pool");
        return response;
    }
    else {
        LOG_ERROR("Failed to call GetVarifyCode RPC method");
        _pool->ReturnConnection(std::move(stub));
        LOG_DEBUG("Returned gRPC stub to pool");
        // 返回错误
        response.set_error(ErrorCodes::RPCFailed);
        return response;
    }
}