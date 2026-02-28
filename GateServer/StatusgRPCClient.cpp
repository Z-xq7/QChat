#include "StatusgRPCClient.h"
#include "ConfigMgr.h"

StatusGrpcClient::StatusGrpcClient()
{
    auto& cfg = ConfigMgr::Instance();
    std::string host = cfg["StatusServer"]["host"];
    std::string port = cfg["StatusServer"]["port"];
    pool_.reset(new RPCConPool<StatusService>(5, host, port));
}

GetChatServerRsp StatusGrpcClient::GetChatServer(int uid)
{
    ClientContext context;
	//设置RPC调用的截止时间为3秒，超过这个时间后RPC调用将被取消
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));
    GetChatServerReq request;
    GetChatServerRsp response;
    request.set_uid(uid);

    auto stub = pool_->GetConnection();
    if (!stub)
    {
        response.set_error(ErrorCodes::RPCFailed);
        return response;
    }

    Status status = stub->GetChatServer(&context, request, &response);
    pool_->ReturnConnection(std::move(stub));

    if (!status.ok()) {
        response.set_error(ErrorCodes::RPCFailed);
    }
    return response;
}

//LoginRsp StatusGrpcClient::Login(int uid, std::string token)
//{
//    ClientContext context;
//    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));
//
//    LoginReq req;
//    LoginRsp rsp;
//    req.set_uid(uid);
//    req.set_token(token);
//
//    auto stub = pool_->GetConnection();
//    if (!stub)
//    {
//        rsp.set_error(ErrorCodes::RPCFailed);
//        return rsp;
//    }
//
//    Status status = stub->Login(&context, req, &rsp);
//    pool_->ReturnConnection(std::move(stub));
//
//    if (!status.ok()) {
//        rsp.set_error(ErrorCodes::RPCFailed);
//    }
//    return rsp;
//}
