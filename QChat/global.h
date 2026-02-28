/*******************************************************
* @file         global.h
* @brief        用来刷新qss
*
* @author       祁七七
* @date          2026/02/09
* @history
*******************************************************/

#ifndef GLOBAL_H
#define GLOBAL_H
#include <QWidget>
#include <functional>
#include "QStyle"
#include <QRegularExpression>
#include <memory>
#include <iostream>
#include <mutex>
#include <QByteArray>
#include <QNetworkReply>
#include <QJsonObject>
#include <QDir>
#include <QSettings>

//刷新
extern std::function<void(QWidget*)> repolish;
//加密
extern std::function<QString(QString)> xorString;

//请求id
enum ReqId{
    ID_GET_VARIFY_CODE = 1001,  //获取验证码
    ID_REG_USER = 1002,         //注册用户
    ID_RESET_PWD = 1003,        //重置密码
    ID_LOGIN_USER = 1004,       //用户登录
    ID_CHAT_LOGIN = 1005,       //登录聊天服务器
    ID_CHAT_LOGIN_RSP = 1006,   //登录聊天服务器回包
    ID_SEARCH_USER_REQ = 1007, //用户搜索请求
    ID_SEARCH_USER_RSP = 1008, //搜索用户回包
    ID_ADD_FRIEND_REQ = 1009,  //添加好友申请
    ID_ADD_FRIEND_RSP = 1010, //申请添加好友回复
    ID_NOTIFY_ADD_FRIEND_REQ = 1011,  //通知用户添加好友申请
    ID_AUTH_FRIEND_REQ = 1013,  //认证好友请求
    ID_AUTH_FRIEND_RSP = 1014,  //认证好友回复
    ID_NOTIFY_AUTH_FRIEND_REQ = 1015, //通知用户认证好友申请
    ID_TEXT_CHAT_MSG_REQ  = 1017,  //文本聊天信息请求
    ID_TEXT_CHAT_MSG_RSP  = 1018,  //文本聊天信息回复
    ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019, //通知用户文本聊天信息
    ID_NOTIFY_OFF_LINE_REQ = 1021, //通知用户下线
    ID_HEART_BEAT_REQ = 1023,      //心跳请求
    ID_HEART_BEAT_RSP = 1024,       //心跳回复
    ID_LOAD_CHAT_THREAD_REQ = 1025, //加载聊天线程请求
    ID_LOAD_CHAT_THREAD_RSP = 1026, //加载聊天线程回复
    ID_CREATE_PRIVATE_CHAT_REQ = 1027, //创建私聊线程请求
    ID_CREATE_PRIVATE_CHAT_RSP = 1028, //创建私聊线程回复
};

//模块
enum Modules{
    REGISTERMOD = 0,    //注册模块
    RESETMOD = 1,       //重置密码模块
    LOGINMOD = 2        //登录模块
};

//错误码
enum ErrorCodes{
    SUCCESS = 0,        //成功
    ERR_JSON = 1,       //JSON解析失败
    ERR_NETWORK = 2,    //网络请求失败

};

//输入错误提示
enum TipErr{
    TIP_SUCCESS = 0,
    TIP_EMAIL_ERR = 1,
    TIP_PWD_ERR = 2,
    TIP_CONFIRM_ERR = 3,
    TIP_PWD_CONFIRM = 4,
    TIP_VARIFY_ERR = 5,
    TIP_USER_ERR = 6
};

//鼠标点击的状态
enum ClickLbState{
    Normal = 0,
    Selected = 1
};

//登录请求服务器回包
struct ServerInfo{
    QString Host;
    QString Port;
    QString Token;
    int Uid;
};

//聊天界面的几种模式
enum ChatUIMode{
    SearchMode,     //搜索模式
    ChatMode,       //聊天模式
    ContactMode,    //联系模式
    SettingsMode,   //设置模式
};

//自定义的QListWidgetItem的几种类型
enum ListItemType{
    CHAT_USER_ITEM,     //聊天用户
    CONTACT_USER_ITEM,  //联系人用户
    SEARCH_USER_ITEM,   //搜索到的用户
    ADD_USER_TIP_ITEM,  //提示添加用户
    INVALID_ITEM,       //不可点击条目
    GROUP_TIP_ITEM,     //分组提示条目
    LINE_ITEM,          //分割线
    APPLY_FRIEND_ITEM,  //好友申请
};

//聊天角色
enum class ChatRole{
    Self,       //自己
    Other       //其他人
};

//聊天信息
struct MsgInfo{
    QString msgFlag;    //"text,image,file"
    QString content;    //表示文件和图像的url
    QPixmap pixmap;     //文件和图片的缩略图
};

enum MsgStatus{
    UN_READ = 0,        //对方未读
    SEND_FAILED = 1,    //发送失败
    READED = 2          //对方已读
};

//仅做测试用！
extern std::vector<QString> strs;
extern std::vector<QString> heads;
extern std::vector<QString> names;

//申请好友标签输入框最低长度
const int MIN_APPLY_LABEL_ED_LEN = 40;
//一次加载聊天列表/好友信息列表的长度
const int CHAT_COUNT_PER_PAGE = 13;
//添加标签的提示信息
const QString add_prefix = "添加标签";
//好友标签之间的间距
const int tip_offset = 5;

//http请求url
extern QString gate_url_prefix;

#endif // GLOBAL_H
