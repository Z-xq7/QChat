const path = require('path');
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');

// 定义proto文件的路径
const PROTO_PATH = path.join(__dirname, 'message.proto');
// 加载proto文件并生成对应的JavaScript对象
const packageDefinition = protoLoader.loadSync(PROTO_PATH,{
    keepCase: true, //保持原有字段的大小写
    longs: String, //将long类型转换为字符串
    enums: String, //将enum类型转换为字符串
    defaults: true, //设置默认值
    oneofs: true //支持oneof语法
});

 //加载生成的JavaScript对象
const protoDescriptor = grpc.loadPackageDefinition(packageDefinition);
// 获取message包中的message服务对象
const message_proto = protoDescriptor.message;
// 导出message_proto对象，供其他模块使用
module.exports = message_proto;