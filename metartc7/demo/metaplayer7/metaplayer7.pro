QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets


CONFIG += c++11


# Copyright (c) 2019-2022 yanggaofeng

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
#DEFINES += QT_WIN_MSC
HOME_BASE=../../
INCLUDEPATH += $$HOME_BASE/libmetartc7/src
macx{

    INCLUDEPATH += $$HOME_BASE/include
    CONFIG(debug, debug|release) {
        LIBS += -L$$HOME_BASE/bin/lib_debug
        DESTDIR += $$HOME_BASE/bin/app_debug
    }else{
        LIBS += -L$$HOME_BASE/bin/lib_release
        DESTDIR += $$HOME_BASE/bin/app_release
    }
 LIBS +=  -L$$HOME_BASE/thirdparty/lib

 LIBS += -lmetartc7  -lmetartccore7 -lyuv -lspeexdsp -lopus -lyangh264decoder -lusrsctp -lpthread  -ldl

 LIBS += -framework CoreAudio

    #openssl
 LIBS += -lssl2 -lcrypto2 -lsrtp2
}
unix:!macx{

    INCLUDEPATH += $$HOME_BASE/include
    CONFIG(debug, debug|release) {
        LIBS += -L$$HOME_BASE/bin/lib_debug
        DESTDIR += $$HOME_BASE/bin/app_debug
    }else{
        LIBS += -L$$HOME_BASE/bin/lib_release
        DESTDIR += $$HOME_BASE/bin/app_release
    }
 LIBS +=  -L$$HOME_BASE/thirdparty/lib

 LIBS += -lmetartc7  -lmetartccore7 -lyuv -lspeexdsp -lopus -lyangh264decoder -lusrsctp -lpthread  -ldl

#linux
LIBS += -lasound
    #openssl
 LIBS += -lssl2 -lcrypto2 -lsrtp2

#mbtls
 #LIBS += -lmbedtls -lmbedx509 -lmbedcrypto -lsrtp2_mbed
    #gmssl
 #LIBS += -lssl_gm -lcrypto_gm -lmetasrtp3
}

win32{
    DEFINES += __WIN32__
    DEFINES +=_AMD64_

    INCLUDEPATH += $$HOME_BASE\include
    CONFIG(debug, debug|release) {
        LIBS += -L$$HOME_BASE/bin/lib_win_debug
        DESTDIR += $$HOME_BASE/bin/app_win_debug
    }else{
        LIBS += -L$$HOME_BASE/bin/lib_win_release
        DESTDIR += $$HOME_BASE/bin/app_win_release
    }
    LIBS += -lmetartc7  -lmetartccore7 -lyuv -lspeexdsp -lopenh264 -lopus -lusrsctp -lavutil -lavcodec -lwinmm -ldmoguids -lole32 -lStrmiids
    YANG_LIB= -L$$HOME_BASE/thirdparty/lib/win -lsrtp2 -lssl  -lcrypto
    msvc{
        QMAKE_CFLAGS += /utf-8
        QMAKE_CXXFLAGS += /utf-8
        QMAKE_LFLAGS    += /ignore:4099
        DEFINES +=HAVE_STRUCT_TIMESPEC
        DEFINES +=WIN32_LEAN_AND_MEAN
        INCLUDEPATH += $$HOME_BASE\thirdparty\include\win\include   #vc

        YANG_LIB=  -L$$HOME_BASE/thirdparty/lib/win/msvc -lavrt  -lpthreadVC2  -luser32 -lAdvapi32
        #openssl
        YANG_LIB+= -lsrtp2  -llibcrypto -llibssl
        #gmssl
        #YANG_LIB+= -lmetasrtp3  -llibcrypto_gm -llibssl_gm
    }
    LIBS +=  $$YANG_LIB
    LIBS +=   -lCrypt32 -lws2_32
}
# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    video/yangrecordthread.cpp \
    yangplayer/YangPlayFactory.cpp \
    yangplayer/YangPlayWidget.cpp \
    yangplayer/YangPlayerBase.cpp \
    yangplayer/YangPlayerDecoder.cpp \
    yangplayer/YangPlayerHandleImpl.cpp \
    yangplayer/YangPlayerPlay.cpp \
    yangplayer/YangRtcReceive.cpp \
    yangplayer/YangYuvPlayWidget.cpp


HEADERS += \
    mainwindow.h \
    video/yangrecordthread.h \
    yangplayer/YangPlayWidget.h \
    yangplayer/YangPlayerHandleImpl.h \
    yangplayer/YangRtcReceive.h \
    yangplayer/YangYuvPlayWidget.h


FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


# 添加对metartc库的兼容性设置
win32-g++:DEFINES += _GLIBCXX_USE_C99=0

# 添加Windows网络库链接，解决inet_ntoa等网络函数的链接错误
win32:LIBS += -lws2_32 -lwsock32 -liphlpapi -lcrypt32 -ladvapi32 -luser32 -lgdi32 -lole32 -loleaut32 -luuid -lcomdlg32 -lcomctl32

# 添加IP Helper库链接，解决GetAdaptersAddresses等函数的链接错误
win32:LIBS += -liphlpapi

# 添加openssl库路径
INCLUDEPATH += "E:/VS_QChat/thirdparty/openssl/include"

# 添加OpenSSL库链接，解决SSL_get_ex_data、EVP_sha512等加密函数的链接错误
 win32:LIBS += -L"E:/VS_QChat/thirdparty/openssl/bin/" -lssl -lcrypto

# 添加srtp库路径
INCLUDEPATH += "E:/VS_QChat/thirdparty/srtp/include"

# 添加SRTP库链接，解决srtp_create等函数的链接错误
 win32:LIBS += -L"E:/VS_QChat/thirdparty/srtp/bin/" -lsrtp2

# 添加usrsctp库路径
INCLUDEPATH += "E:/VS_QChat/thirdparty/usrsctp/include"

# 添加SCTP库链接，解决usrsctp_setsockopt等函数的链接错误
 win32:LIBS += -L"E:/VS_QChat/thirdparty/usrsctp/bin/" -lusrsctp

# 添加metartc库路径（使用绝对路径）
INCLUDEPATH += "E:/VS_QChat/thirdparty/metartc/include"

# 将所有metartc相关的库放在一起，按正确的依赖顺序链接
win32: LIBS += -L"E:/VS_QChat/thirdparty/metartc/bin/" -lmetartc7d -lyangwincodec7 -lmetartccore7 -lssl -lcrypto -lsrtp2 -lusrsctp -lws2_32 -lwsock32 -liphlpapi -lcrypt32 -ladvapi32 -luser32 -lgdi32 -lole32 -loleaut32 -luuid -lcomdlg32 -lcomctl32
