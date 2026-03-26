#include <yangutil/yangtype.h>
#include <yangutil/yangavinfotype.h>
#include <yangutil/buffer/YangVideoBuffer.h>
#include <yangutil/buffer/YangAudioBuffer.h>
#include <yangutil/sys/YangThread2.h>
#include <yangcapture/YangVideoCapture.h>
#include <yangaudiodev/YangAudioCapture.h>
#include <yangstream/YangStreamManager.h>

// 简单的初始化函数，确保metartc库正确初始化
extern "C" {
    void yang_init_metartc_context(YangContext* context) {
        if (context) {
            // 确保AV信息已初始化
            yang_init_avinfo(&context->avinfo);

            // 根据需要初始化 RTC 相关默认值
            context->avinfo.rtc.enableHttpServerSdp = yangfalse;
            context->avinfo.rtc.sessionTimeout = 30;
            context->avinfo.rtc.iceCandidateType = 0; // 默认值，具体按你的部署调整
            context->avinfo.rtc.iceUsingLocalIp = yangfalse;
            context->avinfo.rtc.iceServerPort = 3478;
            context->avinfo.rtc.enableAudioBuffer = yangtrue;
            context->avinfo.rtc.rtcSocketProtocol = 0;   // UDP
            context->avinfo.rtc.turnSocketProtocol = 0;  // UDP
            context->avinfo.rtc.rtcPort = 0;             // 由系统分配
            context->avinfo.rtc.rtcLocalPort = 0;

            // 初始化流管理器等内部资源
            context->init();
        }
    }

    // 初始化流管理器（如果上层单独需要，可调用这个接口）
    void yang_init_streams(YangContext* context) {
        if (context && context->streams == nullptr) {
            // 这里不用手动 new，保持为空即可，metartc 内部在需要时会创建
            // 这个函数只是占位，避免上层误以为需要自己管理 streams
        }
    }
}