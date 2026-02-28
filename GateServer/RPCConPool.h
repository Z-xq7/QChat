#pragma once
#include "const.h"
#include <grpcpp/grpcpp.h>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <typeinfo>

// Channel是gRPC中表示与服务器的连接的类，Stub是通过Channel创建的用于调用RPC方法的对象
using grpc::Channel;

// Generic RPC connection pool for any gRPC Service type that provides:
//   - nested type Service::Stub
//   - static std::unique_ptr<Service::Stub> NewStub(std::shared_ptr<Channel>)
// Usage: RPCConPool<message::VarifyService> or RPCConPool<message::StatusService>

template <class Service>
class RPCConPool {
public:
	//Stub是模版参数Service的嵌套类型，表示RPC方法调用的接口
    using Stub = typename Service::Stub;

    RPCConPool(size_t poolsize, std::string host, std::string port)
        : _b_stop(false), _poolSize(poolsize), _host(std::move(host)), _port(std::move(port)) {
        for (size_t i = 0; i < _poolSize; ++i) {
            std::shared_ptr<Channel> channel = grpc::CreateChannel(_host + ":" + _port,
                grpc::InsecureChannelCredentials());
            _connections.push(Service::NewStub(channel));
        }
        SetColor(GREEN);
        std::cout << "--- RPCConPool<" << typeid(Service).name() << "> created with " << _poolSize << " connections ---" << std::endl;
        SetColor(RESET);
    }

    ~RPCConPool() {
        std::lock_guard<std::mutex> lock(_mutex);
        Close();
        while (!_connections.empty()) {
            _connections.pop();
        }
        SetColor(BLUE);
        std::cout << "--- RPCConPool destruct ---" << std::endl;
        SetColor(RESET);
    }

	//获取一个可用的RPC stub连接，如果连接池已停止则返回nullptr
    std::unique_ptr<Stub> GetConnection() {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock, [this]() {
            return !_connections.empty() || _b_stop.load();
        });
        if (_b_stop.load()) {
            return nullptr;
        }
        auto connection = std::move(_connections.front());
        _connections.pop();
        return connection;
    }

	//返回一个stub连接到连接池，如果连接池已停止则直接丢弃该连接
    void ReturnConnection(std::unique_ptr<Stub> connection) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_b_stop.load()) {
            return;
        }
        _connections.push(std::move(connection));
        _cond.notify_one();
    }

    void Close() {
        _b_stop.store(true);
        _cond.notify_all();
    }

private:
    std::atomic<bool> _b_stop;
    size_t _poolSize;
    std::string _host;
    std::string _port;
    std::queue<std::unique_ptr<Stub>> _connections;
    std::mutex _mutex;
    std::condition_variable _cond;
};

