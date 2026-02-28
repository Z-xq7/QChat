#ifndef USERMGR_H
#define USERMGR_H
#include <QObject>
#include <memory>
#include <singleton.h>
#include "userdata.h"
#include <vector>

class UserMgr:public QObject,public Singleton<UserMgr>,
        public std::enable_shared_from_this<UserMgr>
{
    Q_OBJECT
public:
    friend class Singleton<UserMgr>;
    ~ UserMgr();

    void SetToken(QString token);
    int GetUid();
    QString GetName();

    //获取申请列表
    std::vector<std::shared_ptr<ApplyInfo>> GetApplyList();
    //判断对方是否已经申请过添加好友
    bool AlreadyApply(int uid);
    //添加申请信息
    void AddApplyList(std::shared_ptr<ApplyInfo> app);
    //设置个人信息
    void SetUserInfo(std::shared_ptr<UserInfo>user_info);
    //获取用户信息
    std::shared_ptr<UserInfo> GetUserInfo();
    //添加好友申请列表
    void AppendApplyList(QJsonArray array);
    //添加好友列表
    void AppendFriendList(QJsonArray array);
    //判断好友是否已经在好友列表中了
    bool CheckFriendById(int uid);
    //同意对方为好友后添加好友
    void AddFriend(std::shared_ptr<AuthRsp> auth_rsp);
    //被同意为好友后添加好友
    void AddFriend(std::shared_ptr<AuthInfo> auth_info);
    //通过uid搜索好友
    std::shared_ptr<FriendInfo> GetFriendById(int uid);
    //根据每一页加载好友列表（聊天列表）
    std::vector<std::shared_ptr<FriendInfo>> GetChatListPerPage();
    //更新已经加载的好友数量（聊天列表）
    void UpdateChatLoadedCount();
    //判断好友列表是否全部加载完（聊天列表）
    bool IsLoadChatFin();
    //根据每一页加载好友信息列表（好友信息列表）
    std::vector<std::shared_ptr<FriendInfo>> GetConListPerPage();
    //更新已经加载的好友信息数量（好友信息列表）
    void UpdateContactLoadedCount();
    //判断好友信息列表是否全部加载完（好友信息列表）
    bool IsLoadConFin();
    //缓存和好友的消息内容
    void AppendFriendChatMsg(int friend_id,std::vector<std::shared_ptr<TextChatData>>);

private:
    UserMgr();

    QString _token;
    //存储申请列表
    std::vector<std::shared_ptr<ApplyInfo>> _apply_list;
    //存储好友信息
    std::vector<std::shared_ptr<FriendInfo>> _friend_list;
    //存储个人信息
    std::shared_ptr<UserInfo> _user_info;
    //存储好友信息
    QMap<int,std::shared_ptr<FriendInfo>> _friend_map;
    //聊天列表加载位置
    int _chat_loaded;
    //好友信息列表加载位置
    int _contact_loaded;
};

#endif // USERMGR_H
