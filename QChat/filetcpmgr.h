#ifndef FILETCPMGR_H
#define FILETCPMGR_H

#include <QTcpSocket>
#include "singleton.h"
#include "global.h"
#include <functional>
#include <QObject>
#include "userdata.h"
#include <QJsonArray>
#include <memory>
#include <QThread>
#include <QQueue>
#include <memory>

//文件tcp传输线程
class FileTcpThread: public std::enable_shared_from_this<FileTcpThread>{
public:
    FileTcpThread();
    ~FileTcpThread();
private:
    QThread * _file_tcp_thread;

};

class FileTcpMgr : public QObject, public Singleton<FileTcpMgr>,
                   public std::enable_shared_from_this<FileTcpMgr>
{
    Q_OBJECT
public:
    friend class Singleton<FileTcpMgr>;

    ~FileTcpMgr();

    void SendData(ReqId reqId, QByteArray data);
    void CloseConnection();
    void SendDownloadInfo(std::shared_ptr<DownloadInfo> download,QString req_type);
    //拥塞窗口上传数据
    void BatchSend(std::shared_ptr<MsgInfo> msg_info, int sender, int receiver);
    //暂停后继续上传图片文件
    void ContinueUploadFile(QString unique_name);
    //暂停后继续下载图片文件
    void ContinueDownloadFile(QString unique_name);
    //拷贝上传的文件
    void CopyFile(QString src_path, QString dst_path, QString dst_dir);

private:
    explicit FileTcpMgr(QObject *parent = nullptr);
    void registerMetaType();
    void initHandlers();
    void handleMsg(ReqId id, int len, QByteArray data);

    QTcpSocket _socket;
    QString _host;
    uint16_t _port;
    QByteArray _buffer;
    bool _b_recv_pending;
    quint16 _message_id;
    quint32 _message_len;
    QMap<ReqId, std::function<void(ReqId id, int len, QByteArray data)>> _handlers;
    //发送队列
    QQueue<QByteArray> _send_queue;
    //正在发送的包
    QByteArray  _current_block;
    //当前已发送的字节数
    qint64        _bytes_sent;
    //是否正在发送
    bool _pending;
    //发送的拥塞窗口，控制发送数量
    int _cwnd_size;

signals:
    void sig_close();
    void sig_send_data(ReqId reqId, QByteArray data);
    void sig_con_success(bool bsuccess);
    void sig_connection_closed();
    //重新加载label头像
    void sig_reset_label_icon(QString path);
    //重新加载侧边栏用户头像
    void sig_reset_head();
    void sig_update_upload_progress(std::shared_ptr<MsgInfo>);
    void sig_continue_upload_file(QString unique_name);
    void sig_continue_download_file(QString unique_name);
    void sig_update_download_progress(std::shared_ptr<MsgInfo>);
    void sig_download_finish(std::shared_ptr<MsgInfo>,QString file_path);
    void sig_file_transfer_failed(std::shared_ptr<MsgInfo>);

public slots:
    void slot_send_data(ReqId reqId, QByteArray data);
    void slot_tcp_connect(std::shared_ptr<ServerInfo> si);
    void slot_tcp_close();
    void slot_continue_upload_file(QString unique_name);
    void slot_continue_download_file(QString unique_name);
};

#endif // FILETCPMGR_H
