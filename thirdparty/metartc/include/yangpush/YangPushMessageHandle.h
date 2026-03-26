#ifndef YANGPUSHMESSAGEHANDLE_H
#define YANGPUSHMESSAGEHANDLE_H

#include <yangutil/yangtype.h>
#include <yangutil/yangavinfotype.h>
#include <yangpush/YangPushHandle.h>
#include <yangpush/YangPushFactory.h>
#include <yangutil/sys/YangSysMessageHandle.h>

class YangPushMessageHandle {
public:
    YangPushMessageHandle();
    virtual ~YangPushMessageHandle();
    
    void init(YangContext* context, int32_t videoSrcType);
    YangPushHandle* getPushHandle();
    
    YangPushHandle* m_push;  // 用于直接访问push handle
    YangPushFactory m_factory; // 内嵌factory实例
    YangContext* m_context; // 保存context引用
    int32_t m_videoSrcType; // 保存视频源类型
};

#endif // YANGPUSHMESSAGEHANDLE_H