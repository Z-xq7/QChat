#pragma once
#include <memory>
#include <mutex>
#include <iostream>

// 单例模式：全局仅一个实例，并提供全局访问点
template <typename T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator=(const Singleton<T>&) = delete;

public:
    static std::shared_ptr<T> GetInstance() {
        static std::once_flag s_flag;
        static std::shared_ptr<T> instance;
        std::call_once(s_flag, [&]() {
            instance = std::shared_ptr<T>(new T);
        });
        return instance;
    }
    void PrintAddress() {
        std::cout << GetInstance().get() << std::endl;
    }
    ~Singleton() {
        std::cout << "this is singleton destruct" << std::endl;
    }
};