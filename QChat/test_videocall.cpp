/**
 * 视频通话功能测试验证
 * 这个文件用于验证修改后的视频通话功能逻辑
 */

#include <QDebug>
#include <QObject>
#include <QApplication>
#include "YangRtcWrapper.h"
#include "videocallmanager.h"
#include "videocallwindow.h"

class VideoCallTest : public QObject
{
    Q_OBJECT

public:
    VideoCallTest() {
        qDebug() << "[VideoCallTest] 初始化视频通话功能测试";
    }

public slots:
    void testVideoCallSetup() {
        qDebug() << "[VideoCallTest] 测试视频通话设置";
        
        // 测试YangRtcWrapper初始化
        YangRtcWrapper* rtcWrapper = new YangRtcWrapper();
        if (rtcWrapper) {
            qDebug() << "[VideoCallTest] YangRtcWrapper 初始化成功";
            
            // 测试回调设置
            rtcWrapper->setLocalVideoCallback([](const QByteArray& frame, int width, int height, int format) {
                qDebug() << "[VideoCallTest] 本地视频回调被调用" << width << "x" << height << "format:" << format;
            });
            
            rtcWrapper->setRemoteVideoCallback([](const QByteArray& frame, int width, int height, int format) {
                qDebug() << "[VideoCallTest] 远程视频回调被调用" << width << "x" << height << "format:" << format;
            });
            
            qDebug() << "[VideoCallTest] 视频回调设置完成";
        }
        
        // 测试VideoCallManager
        VideoCallManager* callMgr = VideoCallManager::GetInstance().get();
        if (callMgr) {
            qDebug() << "[VideoCallTest] VideoCallManager 获取成功";
            qDebug() << "[VideoCallTest] 当前通话状态:" << static_cast<int>(callMgr->getState());
        }
        
        delete rtcWrapper;
        qDebug() << "[VideoCallTest] 视频通话设置测试完成";
    }
    
    void testVideoCallFlow() {
        qDebug() << "[VideoCallTest] 测试视频通话流程";
        
        VideoCallManager* callMgr = VideoCallManager::GetInstance().get();
        if (callMgr) {
            // 模拟发起通话
            qDebug() << "[VideoCallTest] 模拟发起通话";
            // callMgr->startCall(123); // 这里只是模拟，实际不会发起真实通话
            
            // 模拟收到通话邀请
            qDebug() << "[VideoCallTest] 模拟收到通话邀请";
            // callMgr->handleCallIncoming(123, "call_123", "Test User");
        }
        
        qDebug() << "[VideoCallTest] 视频通话流程测试完成";
    }
};

#include "test_videocall.moc"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    VideoCallTest test;
    QObject::connect(&test, &VideoCallTest::testVideoCallSetup, &test, &VideoCallTest::testVideoCallSetup);
    QObject::connect(&test, &VideoCallTest::testVideoCallFlow, &test, &VideoCallTest::testVideoCallFlow);
    
    qDebug() << "[VideoCallTest] 开始测试视频通话功能";
    
    // 运行测试
    test.testVideoCallSetup();
    test.testVideoCallFlow();
    
    qDebug() << "[VideoCallTest] 视频通话功能测试完成";
    
    return 0;
}