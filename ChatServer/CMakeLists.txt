cmake_minimum_required(VERSION 3.16)
project(ChatServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================
# 查找依赖库
# ============================================

# Boost (ASIO, filesystem, property_tree, system, thread)
find_package(Boost REQUIRED COMPONENTS system thread filesystem)
message(STATUS "Boost found: ${Boost_INCLUDE_DIRS}")

# OpenSSL
find_package(OpenSSL REQUIRED)
message(STATUS "OpenSSL found: ${OpenSSL_INCLUDE_DIRS}")

# gRPC + protobuf (使用 CONFIG 模式从 apt 安装中查找)
find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# jsoncpp
find_package(PkgConfig QUIET)
pkg_check_modules(JSONCPP QUIET jsoncpp)
if(NOT JSONCPP_FOUND)
    find_path(JSONCPP_INCLUDE_DIRS json/json.h)
    find_library(JSONCPP_LIBRARIES jsoncpp)
endif()

# hiredis
find_path(HIREDIS_INCLUDE_DIRS hiredis.h)
find_library(HIREDIS_LIBRARIES hiredis)

# MySQL Connector/C++
find_path(MYSQLCPPCONN_INCLUDE_DIRS jdbc/mysql_driver.h
    PATHS /usr/include /usr/local/include
    PATH_SUFFIXES mysqlcppconn)
find_library(MYSQLCPPCONN_LIBRARIES mysqlcppconn)

# ============================================
# Protobuf 代码生成
# ============================================

set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/message.proto")
set(PROTO_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto")

# 获取 protoc 和 grpc_cpp_plugin 的路径
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
set(_GRPC_CPP_PLUGIN $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

# 生成 protobuf 和 gRPC C++ 源文件
add_custom_command(
    OUTPUT "${PROTO_SRC_DIR}/message.pb.cc"
           "${PROTO_SRC_DIR}/message.grpc.pb.cc"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROTO_SRC_DIR}"
    COMMAND protobuf::protoc
        --proto_path="${CMAKE_CURRENT_SOURCE_DIR}"
        --cpp_out="${PROTO_SRC_DIR}"
        --grpc_out="${PROTO_SRC_DIR}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN}"
        "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
    COMMENT "Generating protobuf/gRPC C++ code from message.proto"
)

# ============================================
# 源文件
# ============================================

set(SOURCES
    AsioIOServicePool.cpp
    ChatGrpcClient.cpp
    ChatServer.cpp
    ChatServiceImpl.cpp
    ConfigMgr.cpp
    CServer.cpp
    CSession.cpp
    DistLock.cpp
    Logger.cpp
    LogicSystem.cpp
    MsgNode.cpp
    MysqlDao.cpp
    MysqlMgr.cpp
    RedisMgr.cpp
    StatusGrpcClient.cpp
    UserMgr.cpp
)

set(HEADERS
    AsioIOServicePool.h
    ChatGrpcClient.h
    ChatServiceImpl.h
    ConfigMgr.h
    const.h
    CServer.h
    CSession.h
    data.h
    DistLock.h
    Logger.h
    LogicSystem.h
    MsgNode.h
    MysqlDao.h
    MysqlMgr.h
    RedisMgr.h
    Singleton.h
    StatusGrpcClient.h
    UserMgr.h
)

# ============================================
# 构建可执行文件
# ============================================

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    "${PROTO_SRC_DIR}/message.pb.cc"
    "${PROTO_SRC_DIR}/message.grpc.pb.cc"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTO_SRC_DIR}
    ${Boost_INCLUDE_DIRS}
    ${OpenSSL_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${MYSQLCPPCONN_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Boost::system
    Boost::thread
    Boost::filesystem
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc
    ${JSONCPP_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    ${MYSQLCPPCONN_LIBRARIES}
    pthread
    dl
)

# 编译时忽略已生成的 protobuf 文件的警告
set_source_files_properties(
    "${PROTO_SRC_DIR}/message.pb.cc"
    "${PROTO_SRC_DIR}/message.grpc.pb.cc"
    PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter"
)
