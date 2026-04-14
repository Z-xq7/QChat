#include "MysqlMgr.h"


MysqlMgr::~MysqlMgr() {

}

int MysqlMgr::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
	return _dao.RegUser(name, email, pwd);
}

bool MysqlMgr::CheckEmail(const std::string& name, const std::string& email) {
	return _dao.CheckEmail(name, email);
}

bool MysqlMgr::UpdatePwd(const std::string& name, const std::string& pwd) {
	return _dao.UpdatePwd(name, pwd);
}

MysqlMgr::MysqlMgr() {
}

bool MysqlMgr::CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo) {
	return _dao.CheckPwd(name, pwd, userInfo);
}

bool MysqlMgr::AddFriendApply(const int& from, const int& to, const std::string& desc, const std::string& bakname)
{
	return _dao.AddFriendApply(from, to, desc, bakname);
}

//（已废弃）
bool MysqlMgr::AuthFriendApply(const int& from, const int& to) {
	return _dao.AuthFriendApply(from, to);
}

bool MysqlMgr::AddFriend(const int& from, const int& to, std::string back_name, std::vector<std::shared_ptr<AddFriendMsg>>& msg_list) {
	return _dao.AddFriend(from, to, back_name, msg_list);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(int uid)
{
	return _dao.GetUser(uid);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(std::string name)
{
	return _dao.GetUser(name);
}

bool MysqlMgr::GetApplyList(int touid, 
	std::vector<std::shared_ptr<ApplyInfo>>& applyList, int begin, int limit) {

	return _dao.GetApplyList(touid, applyList, begin, limit);
}

bool MysqlMgr::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo> >& user_info) {
	return _dao.GetFriendList(self_id, user_info);
}

bool MysqlMgr::GetUserThreads(int64_t userId, int64_t last_id, int page_size,
	std::vector<std::shared_ptr<ChatThreadInfo>>& threads, bool& load_more, int& next_last_id)
{
	return _dao.GetUserThreads(userId, last_id, page_size, threads, load_more, next_last_id);
}

bool MysqlMgr::CreatePrivateChat(int user1_id, int user2_id, int thread_id)
{
	return _dao.CreatePrivateChat(user1_id, user2_id, thread_id);
}

std::shared_ptr<PageResult> MysqlMgr::LoadChatMsg(int threadId, int lastId, int pageSize)
{
	return _dao.LoadChatMsg(threadId,lastId,pageSize);
}

bool MysqlMgr::AddChatMsg(std::vector<std::shared_ptr<ChatMessage>>& chat_datas)
{
	return _dao.AddChatMsg(chat_datas);
}

bool MysqlMgr::AddChatMsg(std::shared_ptr<ChatMessage>& chat_datas)
{
	return _dao.AddChatMsg(chat_datas);
}

std::shared_ptr<ChatMessage> MysqlMgr::GetChatMsg(int message_id)
{
	return _dao.GetChatMsg(message_id);
}

bool MysqlMgr::CreateGroupChat(int creator_uid, const std::string& group_name,
	const std::vector<int>& member_uids, int& thread_id)
{
	return _dao.CreateGroupChat(creator_uid, group_name, member_uids, thread_id);
}

bool MysqlMgr::GetGroupMembers(int thread_id, std::vector<std::shared_ptr<GroupMemberInfo>>& members)
{
	return _dao.GetGroupMembers(thread_id, members);
}

bool MysqlMgr::GetUserGroupChats(int user_id, std::vector<int>& thread_ids)
{
	return _dao.GetUserGroupChats(user_id, thread_ids);
}

bool MysqlMgr::GetGroupInfo(int thread_id, GroupInfo& group_info)
{
	return _dao.GetGroupInfo(thread_id, group_info);
}

bool MysqlMgr::UpdateGroupNotice(int thread_id, const std::string& notice)
{
	return _dao.UpdateGroupNotice(thread_id, notice);
}

bool MysqlMgr::UpdateGroupMemberSetting(int thread_id, int user_id, const std::string& group_nick, int role, int is_disturb, int is_top)
{
	return _dao.UpdateGroupMemberSetting(thread_id, user_id, group_nick, role, is_disturb, is_top);
}

bool MysqlMgr::GetUnreadCounts(int user_id, std::vector<std::pair<int, int>>& unread_counts)
{
	return _dao.GetUnreadCounts(user_id, unread_counts);
}

bool MysqlMgr::MarkMsgRead(int thread_id, int reader_uid, std::string update_time)
{
	return _dao.MarkMsgRead(thread_id, reader_uid, update_time);
}

bool MysqlMgr::GetPrivateChatPeer(int thread_id, int self_uid, int& peer_uid)
{
	return _dao.GetPrivateChatPeer(thread_id, self_uid, peer_uid);
}



