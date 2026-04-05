#ifndef USERMGR_H
#define USERMGR_H
#include <QObject>
#include <memory>
#include <singleton.h>
#include "userdata.h"
#include <vector>
#include <mutex>
#include <QLabel>
#include "global.h"

class UserMgr:public QObject,public Singleton<UserMgr>,
        public std::enable_shared_from_this<UserMgr>
{
    Q_OBJECT
public:
    friend class Singleton<UserMgr>;
    ~ UserMgr();

    void SetToken(QString token);
    QString GetToken();
    int GetUid();
    QString GetName();
    QString GetNick();
    void SetNick(QString nick);
    QString GetIcon();
    void SetIcon(QString name);
    QString GetDesc();
    void SetDesc(QString desc);

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
    std::shared_ptr<UserInfo> GetFriendById(int uid);
    //获取所有好友列表
    std::vector<std::shared_ptr<UserInfo>> GetAllFriends();
    //获取所有会话列表
    std::vector<std::shared_ptr<ChatThreadData>> GetAllChatThreadData();
    //根据每一页加载好友列表（聊天列表）
    std::vector<std::shared_ptr<UserInfo>> GetChatListPerPage();
    //更新已经加载的好友数量（聊天列表）
    void UpdateChatLoadedCount();
    //判断好友列表是否全部加载完（聊天列表）
    bool IsLoadChatFin();
    //根据每一页加载好友信息列表（好友信息列表）
    std::vector<std::shared_ptr<UserInfo>> GetConListPerPage();
    //更新已经加载的好友信息数量（好友信息列表）
    void UpdateContactLoadedCount();
    //判断好友信息列表是否全部加载完（好友信息列表）
    bool IsLoadConFin();
    // //缓存和好友的消息内容
    // void AppendFriendChatMsg(int friend_id,std::vector<std::shared_ptr<TextChatData>>);
    //添加聊天信息
    void AddChatThreadData(std::shared_ptr<ChatThreadData>chat_thread_data,int other_uid);
    //设置最后加载的thread聊天id
    void SetLastChatThreadId(int last_chat_thread_id);
    //获取最后加载的thread聊天id
    int GetLastChatThreadId();
    //根据uid获取thread_id
    int GetThreadIdByUid(int uid);
    //先从_uid_to_thread_id获取thread_id，再从_chat_map中根据thread_id获取会话数据
    std::shared_ptr<ChatThreadData> GetChatThreadByUid(int uid);
   //从_chat_map中根据thread_id获取会话数据
    std::shared_ptr<ChatThreadData> GetChatThreadByThreadId(int thread_id);
    //将已发送的消息，还未收到回应的消息加到map中管理
    void AddMsgUnRsp(std::shared_ptr<TextChatData> msg);
    //加载当前聊天对话的消息
    std::shared_ptr<ChatThreadData> GetCurLoadData();
    //加载当前的下一个聊天对话的消息
    std::shared_ptr<ChatThreadData> GetNextLoadData();
    //将md5(文件唯一名称字符串)和上传的文件信息关联起来
    void AddUploadFile(QString name, std::shared_ptr<QFileInfo> file_info);
    //移除上传的文件信息
    void RmvUploadFile(QString name);
    //获取上传信息
    std::shared_ptr<QFileInfo> GetUploadInfoByName(QString name);
    //判断文件是否在上传
    bool IsDownLoading(QString name);
    //将文件加到下载map中
    void AddDownloadFile(QString name, std::shared_ptr<DownloadInfo> file_info);
    //将文件从下载map中移除
    void RmvDownloadFile(QString name);
    //获取下载文件的信息
    std::shared_ptr<DownloadInfo> GetDownloadInfo(QString name);
    //添加资源路径到将要重置的Label集合
    void AddLabelToReset(QString path, QLabel* label);

    // 获取缓存的头像
    QPixmap GetCachedIcon(QString path);
    // 缓存头像
    void CacheIcon(QString path, QPixmap pixmap);
    //重置用户label头像
    void ResetLabelIcon(QString path);
    //添加传输中的文件
    void AddTransFile(QString name, std::shared_ptr<MsgInfo> msg_info);
    //通过名字查找传输中的文件
    std::shared_ptr<MsgInfo> GetTransFileByName(QString name);
    //通过名字删除传输的文件
    void RmvTransFileByName(QString name);
    //获取要传输的文件
    std::shared_ptr<MsgInfo> GetFreeUploadFile();
    //获取要下载的文件
    std::shared_ptr<MsgInfo> GetFreeDownloadFile();
    //暂停正在传输的文件
    void PauseTransFileByName(QString name);
    //暂停后继续上传或下载文件
    void ResumeTransFileByName(QString name);
    //判断文件是否正在上传
    bool TransFileIsUploading(QString name);
    //获取所有传输中的文件
    std::vector<std::shared_ptr<MsgInfo>> GetAllTransFiles();

    // 群聊相关方法
    // 添加群聊到管理
    void AddGroupChat(int thread_id, std::shared_ptr<ChatThreadData> group_data);
    // 根据thread_id获取群聊数据
    std::shared_ptr<ChatThreadData> GetGroupChat(int thread_id);
    // 检查是否是群聊
    bool IsGroupChat(int thread_id);
    // 获取所有群聊列表
    std::vector<std::shared_ptr<ChatThreadData>> GetAllGroupChats();
    // 登录时批量追加群聊基本信息
    void AppendGroupList(QJsonArray array);

signals:
    // 群聊创建成功信号，通知UI更新聊天列表
    void sig_group_chat_created(int thread_id, const QString& group_name);

public slots:
    // 处理创建群聊响应
    void SlotCreateGroupChatRsp(int error, int thread_id, const QString& group_name);
    // 处理被加入群聊通知
    void SlotGroupChatCreated(int thread_id, const QString& group_name, int creator_uid,
                              const std::vector<std::shared_ptr<GroupMemberInfo>>& members);
    // 处理群公告更新通知（来自服务器推送）
    void SlotUpdateGroupNotice(int thread_id, const QString& notice);

private:
    UserMgr();

    std::mutex _mtx;
    QString _token;
    //存储申请列表
    std::vector<std::shared_ptr<ApplyInfo>> _apply_list;
    //存储好友信息
    std::vector<std::shared_ptr<UserInfo>> _friend_list;
    //存储个人信息
    std::shared_ptr<UserInfo> _user_info;
    //存储好友信息
    QMap<int,std::shared_ptr<UserInfo>> _friend_map;
    //聊天列表加载位置
    int _chat_loaded;
    //好友信息列表加载位置
    int _contact_loaded;
    //建立会话thread_id到数据的映射关系
    QMap<int,std::shared_ptr<ChatThreadData>> _chat_map;
    //聊天会话id列表
    std::vector<int> _chat_thread_ids;
    //记录已经加载聊天列表的会话索引
    int _cur_load_chat_index;
    //建立将对方uid和会话id关联
    QMap<int,int> _uid_to_thread_id;
    //记录最后加载的thread聊天id
    int _last_chat_thread_id;
    //已发送的消息，还未收到回应的。
    QMap<QString, std::shared_ptr<TextChatData>> _msg_unrsp_map;
    //上传文件md5（文件唯一名称）和文件信息关联 映射
    QMap<QString, std::shared_ptr<QFileInfo> > _name_to_upload_info;
    std::mutex _down_load_mtx;
    //名字关联下载信息
    QMap<QString, std::shared_ptr<DownloadInfo> > _name_to_download_info;
    QHash<QString, QList<QLabel*>> _path_to_reset_labels;
    //名字关联发送的信息
    QHash<QString, std::shared_ptr<MsgInfo> > _name_to_msg_info;
    //传输锁
    std::mutex _trans_mtx;
    // 头像缓存
    QMap<QString, QPixmap> _icon_cache;
    // 群聊数据映射 thread_id -> ChatThreadData
    QMap<int, std::shared_ptr<ChatThreadData>> _group_chat_map;

public slots:
    void SlotAddFriendRsp(std::shared_ptr<AuthRsp> rsp);
    void SlotAddFriendAuth(std::shared_ptr<AuthInfo> auth);
};

#endif // USERMGR_H
