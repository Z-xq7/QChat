#include "usermgr.h"
#include <QJsonArray>
#include "tcpmgr.h"

UserMgr::~UserMgr()
{

}

UserMgr::UserMgr():_user_info(nullptr),_chat_loaded(0),_contact_loaded(0),_cur_load_chat_index(0){

}

void UserMgr::SlotAddFriendRsp(std::shared_ptr<AuthRsp> rsp)
{
    AddFriend(rsp);
}

void UserMgr::SlotAddFriendAuth(std::shared_ptr<AuthInfo> auth)
{
    AddFriend(auth);
}

void UserMgr::SetToken(QString token)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _token = token;
}

QString UserMgr::GetToken()
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _token;
}

int UserMgr::GetUid()
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _user_info->_uid;
}

QString UserMgr::GetName()
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _user_info->_name;
}

QString UserMgr::GetNick()
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _user_info->_nick;
}

void UserMgr::SetNick(QString nick)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _user_info->_nick = nick;
}

QString UserMgr::GetIcon()
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _user_info->_icon;
}

void UserMgr::SetIcon(QString name)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _user_info->_icon = name;
}

QString UserMgr::GetDesc()
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _user_info->_desc;
}

void UserMgr::SetDesc(QString desc)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _user_info->_desc = desc;
}

std::vector<std::shared_ptr<ApplyInfo> > UserMgr::GetApplyList()
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _apply_list;
}

bool UserMgr::AlreadyApply(int uid)
{
    std::lock_guard<std::mutex> lock(_mtx);
    for(auto& apply: _apply_list){
        if(apply->_uid == uid){
            return true;
        }
    }
    return false;
}

void UserMgr::AddApplyList(std::shared_ptr<ApplyInfo> app)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _apply_list.push_back(app);
}

void UserMgr::SetUserInfo(std::shared_ptr<UserInfo> user_info)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _user_info = user_info;
}

std::shared_ptr<UserInfo> UserMgr::GetUserInfo()
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _user_info;
}

void UserMgr::AppendApplyList(QJsonArray array)
{
    // 遍历 QJsonArray 并输出每个元素
    for (const QJsonValue &value : array) {
        auto name = value["name"].toString();
        auto desc = value["desc"].toString();
        auto icon = value["icon"].toString();
        auto nick = value["nick"].toString();
        auto sex = value["sex"].toInt();
        auto uid = value["uid"].toInt();
        auto status = value["status"].toInt();
        auto info = std::make_shared<ApplyInfo>(uid, name,
            desc, icon, nick, sex, status);

        std::lock_guard<std::mutex> lock(_mtx);
        _apply_list.push_back(info);
    }
}

void UserMgr::AppendFriendList(QJsonArray array) {
    // 遍历 QJsonArray 并输出每个元素
    for (const QJsonValue& value : array) {
        auto name = value["name"].toString();
        auto desc = value["desc"].toString();
        auto icon = value["icon"].toString();
        auto nick = value["nick"].toString();
        auto sex = value["sex"].toInt();
        auto uid = value["uid"].toInt();
        auto back = value["back"].toString();

        auto info = std::make_shared<UserInfo>(uid, name,
            nick, icon, sex, desc, back);

        std::lock_guard<std::mutex> lock(_mtx);
        _friend_list.push_back(info);
        _friend_map.insert(uid, info);
    }
}

bool UserMgr::CheckFriendById(int uid)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto iter = _friend_map.find(uid);
    if(iter == _friend_map.end()){
        return false;
    }
    return true;
}

void UserMgr::AddFriend(std::shared_ptr<AuthRsp> auth_rsp)
{
    auto friend_info = std::make_shared<UserInfo>(auth_rsp);
    std::lock_guard<std::mutex> lock(_mtx);
    _friend_map[friend_info->_uid] = friend_info;

    // （ai修改）处理问候语消息，设置状态为已读
    for(auto& chat_data : auth_rsp->_chat_datas) {
        // 修改消息状态为已读
        chat_data->SetStatus(MsgStatus::READED); // 设置为已读状态
    }
}

void UserMgr::AddFriend(std::shared_ptr<AuthInfo> auth_info)
{
    auto friend_info = std::make_shared<UserInfo>(auth_info);
    std::lock_guard<std::mutex> lock(_mtx);
    _friend_map[friend_info->_uid] = friend_info;

    // （ai修改）处理问候语消息，设置状态为已读
    for(auto& chat_data : auth_info->_chat_datas) {
        // 修改消息状态为已读
        chat_data->SetStatus(MsgStatus::READED); // 设置为已读状态
    }
}

std::shared_ptr<UserInfo> UserMgr::GetFriendById(int uid)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto find_it = _friend_map.find(uid);
    if(find_it == _friend_map.end()){
        return nullptr;
    }
    return *find_it;
}

std::vector<std::shared_ptr<UserInfo>> UserMgr::GetAllFriends()
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _friend_list;
}

std::vector<std::shared_ptr<ChatThreadData>> UserMgr::GetAllChatThreadData()
{
    std::lock_guard<std::mutex> lock(_mtx);
    std::vector<std::shared_ptr<ChatThreadData>> chat_list;
    for(auto& chat : _chat_map){
        chat_list.push_back(chat);
    }
    return chat_list;
}

std::vector<std::shared_ptr<UserInfo>> UserMgr::GetChatListPerPage() {
    std::lock_guard<std::mutex> lock(_mtx);
    //用来存储这次要加载的聊天列表信息
    std::vector<std::shared_ptr<UserInfo>> friend_list;
    //加载的起始位置
    int begin = _chat_loaded;
    //一次加载的末尾
    int end = begin + CHAT_COUNT_PER_PAGE;

    if (begin >= _friend_list.size()) {
        return friend_list;
    }

    if (end > _friend_list.size()) {
        friend_list = std::vector<std::shared_ptr<UserInfo>>(_friend_list.begin() + begin, _friend_list.end());
        return friend_list;
    }

    friend_list = std::vector<std::shared_ptr<UserInfo>>(_friend_list.begin() + begin, _friend_list.begin()+ end);
    return friend_list;
}

void UserMgr::UpdateChatLoadedCount() {
    std::lock_guard<std::mutex> lock(_mtx);
    int begin = _chat_loaded;
    int end = begin + CHAT_COUNT_PER_PAGE;

    if (begin >= _friend_list.size()) {
        return ;
    }

    if (end > _friend_list.size()) {
        _chat_loaded = _friend_list.size();
        return ;
    }

    _chat_loaded = end;
}

bool UserMgr::IsLoadChatFin() {
    std::lock_guard<std::mutex> lock(_mtx);
    if (_chat_loaded >= _friend_list.size()) {
        return true;
    }

    return false;
}

std::vector<std::shared_ptr<UserInfo>> UserMgr::GetConListPerPage() {
    std::lock_guard<std::mutex> lock(_mtx);
    std::vector<std::shared_ptr<UserInfo>> friend_list;
    int begin = _contact_loaded;
    int end = begin + CHAT_COUNT_PER_PAGE;

    if (begin >= _friend_list.size()) {
        return friend_list;
    }

    if (end > _friend_list.size()) {
        qDebug() << "[UserMgr]: -------------- friend_list: end >= _friend_list.size() -------------";
        friend_list = std::vector<std::shared_ptr<UserInfo>>(_friend_list.begin() + begin, _friend_list.end());
        if(friend_list.empty() == false) qDebug() << "[UserMgr]: -------- friend_list size is " << friend_list.size() << " ---------";
        return friend_list;
    }

    friend_list = std::vector<std::shared_ptr<UserInfo>>(_friend_list.begin() + begin, _friend_list.begin() + end);
    return friend_list;
}

void UserMgr::UpdateContactLoadedCount() {
    std::lock_guard<std::mutex> lock(_mtx);
    int begin = _contact_loaded;
    int end = begin + CHAT_COUNT_PER_PAGE;

    if (begin >= _friend_list.size()) {
        return;
    }

    if (end > _friend_list.size()) {
        _contact_loaded = _friend_list.size();
        return;
    }

    _contact_loaded = end;
}

bool UserMgr::IsLoadConFin()
{
    std::lock_guard<std::mutex> lock(_mtx);
    if (_contact_loaded >= _friend_list.size()) {
        return true;
    }

    return false;
}

// void UserMgr::AppendFriendChatMsg(int friend_id, std::vector<std::shared_ptr<TextChatData>> msgs)
// {
//     auto find_iter = _friend_map.find(friend_id);
//     if(find_iter == _friend_map.end()){
//         qDebug()<<"append friend uid  " << friend_id << " not found";
//         return;
//     }

//     find_iter.value()->AppendChatMsgs(msgs);
// }

void UserMgr::AddChatThreadData(std::shared_ptr<ChatThreadData> chat_thread_data, int other_uid)
{
    std::lock_guard<std::mutex> lock(_mtx);
    //建立会话id到数据的映射关系
    _chat_map[chat_thread_data->GetThreadId()] = chat_thread_data;
    //存储会话列表
    _chat_thread_ids.push_back(chat_thread_data->GetThreadId());
    if(other_uid){
        //将对方uid和会话id关联
        _uid_to_thread_id[other_uid] = chat_thread_data->GetThreadId();
    }
}

void UserMgr::SetLastChatThreadId(int id)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _last_chat_thread_id = id;
}

int UserMgr::GetLastChatThreadId()
{
    std::lock_guard<std::mutex> lock(_mtx);
    return _last_chat_thread_id;
}

int UserMgr::GetThreadIdByUid(int uid)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto iter = _uid_to_thread_id.find(uid);
    if(iter == _uid_to_thread_id.end()){
        return -1;
    }
    return iter.value();
}

std::shared_ptr<ChatThreadData> UserMgr::GetChatThreadByUid(int uid)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto iter = _uid_to_thread_id.find(uid);
    if(iter == _uid_to_thread_id.end()){
        return nullptr;
    }

    auto find_iter = _chat_map.find(iter.value());
    if (find_iter != _chat_map.end()) {
        return find_iter.value();
    }
    return nullptr;
}

std::shared_ptr<ChatThreadData> UserMgr::GetChatThreadByThreadId(int thread_id)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto find_iter = _chat_map.find(thread_id);
    if (find_iter != _chat_map.end()) {
        return find_iter.value();
    }
    // 同时查找群聊 map
    auto group_iter = _group_chat_map.find(thread_id);
    if (group_iter != _group_chat_map.end()) {
        return group_iter.value();
    }
    return nullptr;
}

void UserMgr::AddMsgUnRsp(std::shared_ptr<TextChatData> msg)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _msg_unrsp_map.insert(msg->GetUniqueId(), msg);
}

std::shared_ptr<ChatThreadData> UserMgr::GetCurLoadData()
{
    std::lock_guard<std::mutex> lock(_mtx);
    if(_cur_load_chat_index >= _chat_thread_ids.size()){
        return nullptr;
    }

    auto iter = _chat_map.find(_chat_thread_ids[_cur_load_chat_index]);
    if(iter == _chat_map.end()){
        return nullptr;
    }

    return iter.value();
}

std::shared_ptr<ChatThreadData> UserMgr::GetNextLoadData()
{
    std::lock_guard<std::mutex> lock(_mtx);
    _cur_load_chat_index++;
    if(_cur_load_chat_index >= _chat_thread_ids.size()){
        return nullptr;
    }

    auto iter = _chat_map.find(_chat_thread_ids[_cur_load_chat_index]);
    if(iter == _chat_map.end()){
        return nullptr;
    }

    return iter.value();
}

void UserMgr::AddUploadFile(QString name, std::shared_ptr<QFileInfo> file_info)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _name_to_upload_info.insert(name, file_info);
}

void UserMgr::RmvUploadFile(QString name)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _name_to_upload_info.remove(name);
}

std::shared_ptr<QFileInfo> UserMgr::GetUploadInfoByName(QString name)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto iter = _name_to_upload_info.find(name);
    if(iter == _name_to_upload_info.end()){
        return nullptr;
    }

    return iter.value();
}

bool UserMgr::IsDownLoading(QString name) {
    std::lock_guard<std::mutex> lock(_down_load_mtx);
    auto iter = _name_to_download_info.find(name);
    if (iter == _name_to_download_info.end()) {
        return false;
    }

    return true;
}

void UserMgr::AddDownloadFile(QString name, std::shared_ptr<DownloadInfo> file_info) {
    std::lock_guard<std::mutex> lock(_down_load_mtx);
    _name_to_download_info[name] = file_info;
}

void UserMgr::RmvDownloadFile(QString name) {
    std::lock_guard<std::mutex> lock(_down_load_mtx);
    _name_to_download_info.remove(name);
}

std::shared_ptr<DownloadInfo> UserMgr::GetDownloadInfo(QString name)
{
    std::lock_guard<std::mutex> lock(_down_load_mtx);
    auto iter = _name_to_download_info.find(name);
    if (iter == _name_to_download_info.end()) {
        return nullptr;
    }

    return iter.value();
}

void UserMgr::AddLabelToReset(QString path, QLabel* label)
{
    auto iter = _path_to_reset_labels.find(path);
    if (iter == _path_to_reset_labels.end()) {
        QList<QLabel*> list;
        list.append(label);
        _path_to_reset_labels.insert(path, list);
        return;
    }

    iter->append(label);
}

QPixmap UserMgr::GetCachedIcon(QString path)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto iter = _icon_cache.find(path);
    if(iter != _icon_cache.end()){
        return iter.value();
    }
    return QPixmap();
}

void UserMgr::CacheIcon(QString path, QPixmap pixmap)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _icon_cache[path] = pixmap;
}

void UserMgr::ResetLabelIcon(QString path)
{
    auto iter = _path_to_reset_labels.find(path);
    if (iter == _path_to_reset_labels.end()) {
        return;
    }

    for (auto ele_iter = iter.value().begin(); ele_iter != iter.value().end(); ele_iter++) {
        QPixmap pixmap(path); // 加载上传的头像图片
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled((*ele_iter)->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            (*ele_iter)->setPixmap(scaledPixmap);
            (*ele_iter)->setScaledContents(true);
        }
        else {
            qWarning() << "无法加载上传的头像：" << path;
        }
    }

    _path_to_reset_labels.erase(iter);
}

void UserMgr::AddTransFile(QString name, std::shared_ptr<MsgInfo> msg_info)
{
    std::lock_guard<std::mutex> mtx(_trans_mtx);
    _name_to_msg_info[name] = msg_info;
}

std::shared_ptr<MsgInfo> UserMgr::GetTransFileByName(QString name) {
    std::lock_guard<std::mutex> mtx(_trans_mtx);
    auto iter = _name_to_msg_info.find(name);
    if (iter == _name_to_msg_info.end()) {
        return nullptr;
    }

    return *iter;
}

void UserMgr::RmvTransFileByName(QString name) {
    std::lock_guard<std::mutex> mtx(_trans_mtx);
    auto iter = _name_to_msg_info.find(name);
    if (iter == _name_to_msg_info.end()) {
        return ;
    }

    _name_to_msg_info.erase(iter);
}

std::shared_ptr<MsgInfo> UserMgr::GetFreeUploadFile() {
    std::lock_guard<std::mutex> mtx(_trans_mtx);
    if (_name_to_msg_info.isEmpty()) {
        return nullptr;
    }

    for (auto iter = _name_to_msg_info.begin(); iter != _name_to_msg_info.end(); iter++) {
        //只要传输状态不是暂停则返回一个可用的待传输文件
        if ((iter.value()->_transfer_state != TransferState::Paused) &&
            (iter.value()->_transfer_type == TransferType::Upload)) {
            return iter.value();
        }
    }

    //没有找到等待传输的文件则返回空
    return nullptr;
}

std::shared_ptr<MsgInfo> UserMgr::GetFreeDownloadFile() {
    std::lock_guard<std::mutex> mtx(_trans_mtx);
    if (_name_to_msg_info.isEmpty()) {
        return nullptr;
    }

    for (auto iter = _name_to_msg_info.begin(); iter != _name_to_msg_info.end(); iter++) {
        //只要传输状态不是暂停则返回一个可用的待传输文件
        if ((iter.value()->_transfer_state != TransferState::Paused) &&
            (iter.value()->_transfer_type == TransferType::Download)) {
            return iter.value();
        }
    }

    //没有找到等待传输的文件则返回空
    return nullptr;
}

void UserMgr::PauseTransFileByName(QString name) {
    std::lock_guard<std::mutex> mtx(_trans_mtx);
    auto iter = _name_to_msg_info.find(name);
    if (iter == _name_to_msg_info.end()) {
        return;
    }

    iter.value()->_transfer_state = TransferState::Paused;
}

void UserMgr::ResumeTransFileByName(QString name)
{
    std::lock_guard<std::mutex> mtx(_trans_mtx);
    auto iter = _name_to_msg_info.find(name);
    if (iter == _name_to_msg_info.end()) {
        return;
    }

    if (iter.value()->_transfer_type == TransferType::Download) {
        iter.value()->_transfer_state = TransferState::Downloading;
        return;
    }

    if (iter.value()->_transfer_type == TransferType::Upload) {
        iter.value()->_transfer_state = TransferState::Uploading;
        return;
    }
}

bool UserMgr::TransFileIsUploading(QString name) {
    std::lock_guard<std::mutex> mtx(_trans_mtx);
    auto iter = _name_to_msg_info.find(name);
    if (iter == _name_to_msg_info.end()) {
        return false;
    }

    return iter.value()->_transfer_state == TransferState::Uploading;
}

std::vector<std::shared_ptr<MsgInfo>> UserMgr::GetAllTransFiles()
{
    std::lock_guard<std::mutex> mtx(_trans_mtx);
    std::vector<std::shared_ptr<MsgInfo>> trans_files;
    for (auto iter = _name_to_msg_info.begin(); iter != _name_to_msg_info.end(); iter++) {
        trans_files.push_back(iter.value());
    }
    return trans_files;
}

// 群聊相关方法实现
void UserMgr::AddGroupChat(int thread_id, std::shared_ptr<ChatThreadData> group_data)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _group_chat_map[thread_id] = group_data;
    // 同时添加到聊天列表中
    _chat_map[thread_id] = group_data;
    _chat_thread_ids.push_back(thread_id);
}

std::shared_ptr<ChatThreadData> UserMgr::GetGroupChat(int thread_id)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto iter = _group_chat_map.find(thread_id);
    if (iter != _group_chat_map.end()) {
        return iter.value();
    }
    return nullptr;
}

bool UserMgr::IsGroupChat(int thread_id)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto iter = _group_chat_map.find(thread_id);
    return iter != _group_chat_map.end();
}

std::vector<std::shared_ptr<ChatThreadData>> UserMgr::GetAllGroupChats()
{
    std::lock_guard<std::mutex> lock(_mtx);
    std::vector<std::shared_ptr<ChatThreadData>> group_list;
    for (auto iter = _group_chat_map.begin(); iter != _group_chat_map.end(); ++iter) {
        group_list.push_back(iter.value());
    }
    return group_list;
}

void UserMgr::SlotCreateGroupChatRsp(int error, int thread_id, const QString& group_name)
{
    if (error != 0) {
        qDebug() << "[UserMgr]: 创建群聊失败, error=" << error;
        return;
    }

    qDebug() << "[UserMgr]: 创建群聊成功, thread_id=" << thread_id << ", group_name=" << group_name;

    // 创建群聊数据
    auto group_data = std::make_shared<ChatThreadData>();
    group_data->SetThreadId(thread_id);
    group_data->SetGroupName(group_name);
    group_data->SetIsGroup(true);
    group_data->SetOtherId(0);  // 群聊没有特定的对方id

    // 添加到群聊管理
    AddGroupChat(thread_id, group_data);

    // 通知UI更新聊天列表
    emit sig_group_chat_created(thread_id, group_name);
}

void UserMgr::SlotGroupChatCreated(int thread_id, const QString& group_name, int creator_uid,
                                   const std::vector<std::shared_ptr<GroupMemberInfo>>& members)
{
    qDebug() << "[UserMgr]: 收到被加入群聊通知, thread_id=" << thread_id
             << ", group_name=" << group_name << ", creator_uid=" << creator_uid;

    // 检查是否已存在
    if (IsGroupChat(thread_id)) {
        qDebug() << "[UserMgr]: 群聊已存在, 跳过添加";
        return;
    }

    // 创建群聊数据
    auto group_data = std::make_shared<ChatThreadData>();
    group_data->SetThreadId(thread_id);
    group_data->SetGroupName(group_name);
    group_data->SetIsGroup(true);
    group_data->SetOtherId(0);  // 群聊没有特定的对方id
    group_data->SetGroupMembers(members);

    // 添加到群聊管理
    AddGroupChat(thread_id, group_data);

    // 通知UI更新聊天列表
    emit sig_group_chat_created(thread_id, group_name);
}

void UserMgr::AppendGroupList(QJsonArray array)
{
    for (const QJsonValue& value : array) {
        int thread_id    = value["thread_id"].toInt();
        QString name     = value["name"].toString();
        QString icon     = value["icon"].toString();
        QString notice   = value["notice"].toString();
        QString desc     = value["desc"].toString();
        int owner_id     = value["owner_id"].toInt();
        int member_count = value["member_count"].toInt();

        auto group_data = std::make_shared<ChatThreadData>();
        group_data->SetThreadId(thread_id);
        group_data->SetGroupName(name);
        group_data->SetGroupIcon(icon);
        group_data->SetNotice(notice);
        group_data->SetGroupDesc(desc);
        group_data->SetOwnerId(owner_id);
        group_data->SetMemberCount(member_count);
        group_data->SetIsGroup(true);
        group_data->SetOtherId(0);

        std::lock_guard<std::mutex> lock(_mtx);
        _group_chat_map.insert(thread_id, group_data);
        // 同时加入 _chat_map 和 _chat_thread_ids，使 loadChatMsg 能遍历到群聊去请求历史消息
        _chat_map.insert(thread_id, group_data);
        _chat_thread_ids.push_back(thread_id);
    }
    qDebug() << "[UserMgr]: AppendGroupList, loaded" << array.size() << "groups";
}

void UserMgr::SlotUpdateGroupNotice(int thread_id, const QString& notice)
{
    // 更新内存中群聊数据的公告字段
    auto group_data = GetGroupChat(thread_id);
    if (group_data) {
        group_data->SetNotice(notice);
        qDebug() << "[UserMgr]: Updated group notice, thread_id=" << thread_id;
    }
}

bool UserMgr::IsFriendOnline(int uid)
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto iter = _friend_online_status.find(uid);
    if (iter == _friend_online_status.end()) {
        return false; // 默认离线
    }
    return iter.value();
}

void UserMgr::SetFriendOnlineStatus(int uid, bool online)
{
    {
        std::lock_guard<std::mutex> lock(_mtx);
        bool old_status = _friend_online_status.value(uid, false);
        if (old_status == online) {
            return; // 状态未变化
        }
        _friend_online_status[uid] = online;
    }
    qDebug() << "[UserMgr]: Friend status changed, uid=" << uid << ", online=" << online;
    emit sig_friend_status_changed(uid, online);
}

void UserMgr::SetFriendOnlineStatusBatch(const QJsonObject& online_map)
{
    // 先清除所有旧状态（默认全部离线），然后设置在线的好友
    {
        std::lock_guard<std::mutex> lock(_mtx);
        for (auto it = _friend_online_status.begin(); it != _friend_online_status.end(); ++it) {
            it.value() = false;
        }
        // 设置在线的好友
        for (auto it = online_map.begin(); it != online_map.end(); ++it) {
            int uid = it.key().toInt();
            _friend_online_status[uid] = true;
        }
    }
    qDebug() << "[UserMgr]: Batch updated online status, online_count=" << online_map.size();
    // 一次性发出信号，由接收方统一刷新 UI（避免逐个 emit 导致重复遍历列表）
    emit sig_friend_online_status_batch();
}

void UserMgr::RequestFriendOnlineStatus()
{
    QJsonObject json;
    json["uid"] = GetUid();
    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    TcpMgr::GetInstance()->SendData(ReqId::ID_GET_FRIEND_ONLINE_STATUS_REQ, data);
    qDebug() << "[UserMgr]: Sent get friend online status request";
}

void UserMgr::SlotFriendOnline(int uid)
{
    SetFriendOnlineStatus(uid, true);
}

void UserMgr::SlotFriendOffline(int uid)
{
    qDebug() << "[UserMgr]: SlotFriendOffline called, uid=" << uid;
    SetFriendOnlineStatus(uid, false);
}
