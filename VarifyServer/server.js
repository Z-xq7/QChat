const grpc = require('@grpc/grpc-js');
const message_proto = require('./proto');
const emailModule = require('./email');
const const_module = require('./const');
const { v4: uuidv4 } = require('uuid');
const redisModule = require('./redis');

async function GetVarifyCode(call, callback) {
    console.log("email is ", call.request.email)
    try{
        let query_res = await redisModule.GetRedis(const_module.code_prefix + call.request.email);
        console.log("query res is ", query_res)
        let uniqueId = query_res;
        if(query_res ==null){
            uniqueId = uuidv4();
            if (uniqueId.length > 4) {
                uniqueId = uniqueId.substring(0, 4);
            } 
            let bres = await redisModule.SetRedisExpire(const_module.code_prefix+call.request.email, uniqueId,300)
            if(!bres){
                callback(null, { email:  call.request.email,
                    error:const_module.Errors.RedisErr
                });
                return;
            }
        }

        console.log("uniqueId is ", uniqueId)
        let text_str =  '您的验证码为'+ uniqueId +'请五分钟内完成注册'
        //发送邮件
        let mailOptions = {
            from: '2567037772@qq.com',
            to: call.request.email,
            subject: '验证码',
            text: text_str,
        };

        //发送邮件
        //await关键字会等待SendMail函数执行完成，并返回结果
        let send_res = await emailModule.SendMail(mailOptions);
        console.log("send res is ", send_res)

        if(!send_res){
            callback(null, { email:  call.request.email,
                error:const_module.Errors.RedisErr
            }); 
            return;
        }

        // 成功发送邮件后，按 proto 返回成功响应
        callback(null, {
            email: call.request.email,
            error: const_module.Errors.Success,
            code: uniqueId,
        });
        return;

    }catch(error){
        console.log("catch error is ", error)

        callback(null, { email:  call.request.email,
            error:const_module.Errors.Exception
        }); 
    }

}

function main() {
    var server = new grpc.Server()
    server.addService(message_proto.VarifyService.service, { GetVarifyCode: GetVarifyCode })
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
        server.start()
        console.log('grpc server started')      
    })
}

main()