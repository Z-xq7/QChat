#include "VarifygRPCClient.h"
#include "ConfigMgr.h"

VarifygRPCClient::VarifygRPCClient()
{
    auto& gCfgMgr = ConfigMgr::Instance();
    std::string grpc_server_host = gCfgMgr["VarifyServer"]["host"];
    std::string grpc_server_port = gCfgMgr["VarifyServer"]["port"];
    _pool.reset(new RPCConPool<VarifyService>(5, grpc_server_host, grpc_server_port));
}

//获取验证码的RPC调用函数
GetVarifyRsp VarifygRPCClient::GetVarifyCode(std::string email)
{
    ClientContext context;
    // 设置 3 秒超时，避免阻塞
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));
    GetVarifyReq request;
    GetVarifyRsp response;
    request.set_email(email);
    //从连接池获取Stub连接
    auto stub = _pool->GetConnection();
    SetColor(GREEN);
    std::cout << "--- Obtained gRPC stub connection from pool ---" << std::endl;
    SetColor(RESET);
    //调用RPC方法获取验证码
    Status status = stub->GetVarifyCode(&context, request, &response);
    //检查RPC调用结果状态
    if (status.ok()) {
        SetColor(GREEN);
        std::cout << "--- Successfully called GetVarifyCode RPC method ---" << std::endl;
        SetColor(RESET);

        //回收Stub连接到连接池
        _pool->ReturnConnection(std::move(stub));
        SetColor(GREEN);
        std::cout << "--- Returned gRPC stub to pool ---" << std::endl;
        SetColor(RESET);
        return response;
    }
    else {
        SetColor(RED);
        std::cout << "--- Failed to call GetVarifyCode RPC method ---" << std::endl;
        SetColor(RESET);

        _pool->ReturnConnection(std::move(stub));
        SetColor(GREEN);
        std::cout << "--- Returned gRPC stub to pool ---" << std::endl;
        SetColor(RESET);
        // 处理错误
        response.set_error(ErrorCodes::RPCFailed);
        return response;
    }
}