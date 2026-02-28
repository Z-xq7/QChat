const nodemailer = require('nodemailer');
const config_module = require("./config")

//创建发送邮件的代理
let transport = nodemailer.createTransport({
    host: 'smtp.qq.com',
    port: 465,  // SMTP服务器的端口号，通常为465(SSL)或587(TLS)
    secure: true,
    auth: {
        user: config_module.email_user, // 发送方邮箱地址
        pass: config_module.email_pass // 邮箱授权码或者密码
    }
});

/**
 * 发送邮件的函数
 * @param {*} mailOptions_ 发送邮件的参数
 * @returns 
 */
function SendMail(mailOptions_){
    //promise:将异步回调函数转换为同步函数，确保邮件发送完成后再继续执行后续代码
    return new Promise(function(resolve, reject){
        transport.sendMail(mailOptions_, function(error, info){
            if (error) {
                console.log(error);
                reject(error);
            } else {
                console.log('邮件已成功发送：' + info.response);
                resolve(info.response)
            }
        });
    })

}

module.exports.SendMail = SendMail