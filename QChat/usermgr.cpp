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

    if (iter.value()->_transfer_state == TransferState::Uploading) {
        return true;
    }

    return false;
}



