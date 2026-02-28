QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

DESTDIR = ./bin

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    adduseritem.cpp \
    applyfriend.cpp \
    applyfrienditem.cpp \
    applyfriendlist.cpp \
    applyfriendpage.cpp \
    authenfriend.cpp \
    bubbleframe.cpp \
    chatdialog.cpp \
    chatitembase.cpp \
    chatpage.cpp \
    chatuserlist.cpp \
    chatuserwid.cpp \
    chatview.cpp \
    clickedbtn.cpp \
    clickedlabel.cpp \
    clickedoncelabel.cpp \
    contactuserlist.cpp \
    conuseritem.cpp \
    customizeedit.cpp \
    findfaildlg.cpp \
    findsuccessdialog.cpp \
    friendinfopage.cpp \
    friendlabel.cpp \
    global.cpp \
    grouptipitem.cpp \
    httpmgr.cpp \
    imagecropperlabel.cpp \
    listitembase.cpp \
    loadingdialog.cpp \
    logindialog.cpp \
    main.cpp \
    mainwindow.cpp \
    messagetextedit.cpp \
    picturebubble.cpp \
    registerdialog.cpp \
    resetdialog.cpp \
    searchlist.cpp \
    statewidget.cpp \
    tcpmgr.cpp \
    textbubble.cpp \
    timerbtn.cpp \
    userdata.cpp \
    userinfopage.cpp \
    usermgr.cpp

HEADERS += \
    adduseritem.h \
    applyfriend.h \
    applyfrienditem.h \
    applyfriendlist.h \
    applyfriendpage.h \
    authenfriend.h \
    bubbleframe.h \
    chatdialog.h \
    chatitembase.h \
    chatpage.h \
    chatuserlist.h \
    chatuserwid.h \
    chatview.h \
    clickedbtn.h \
    clickedlabel.h \
    clickedoncelabel.h \
    contactuserlist.h \
    conuseritem.h \
    customizeedit.h \
    findfaildlg.h \
    findsuccessdialog.h \
    friendinfopage.h \
    friendlabel.h \
    global.h \
    grouptipitem.h \
    httpmgr.h \
    imagecropperdialog.h \
    imagecropperlabel.h \
    listitembase.h \
    loadingdialog.h \
    logindialog.h \
    mainwindow.h \
    messagetextedit.h \
    picturebubble.h \
    registerdialog.h \
    resetdialog.h \
    searchlist.h \
    singleton.h \
    statewidget.h \
    tcpmgr.h \
    textbubble.h \
    timerbtn.h \
    userdata.h \
    userinfopage.h \
    usermgr.h

FORMS += \
    adduseritem.ui \
    applyfriend.ui \
    applyfrienditem.ui \
    applyfriendpage.ui \
    authenfriend.ui \
    chatdialog.ui \
    chatpage.ui \
    chatuserwid.ui \
    conuseritem.ui \
    findfaildlg.ui \
    findsuccessdialog.ui \
    friendinfopage.ui \
    friendlabel.ui \
    grouptipitem.ui \
    loadingdialog.ui \
    logindialog.ui \
    mainwindow.ui \
    registerdialog.ui \
    resetdialog.ui \
    userinfopage.ui

TRANSLATIONS += \
    QChat_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    config.ini \
    favicon.ico

RESOURCES += \
    resources.qrc

# CONFIG(debug, debug | release){
#     #指定要拷贝的文件目录为工程目录下release目录下的所有dll、lib文件，例如工程目录在D:\QT\Test
#     #PWD就为D:/QT/Test，DllFile = D:/QT/Test/release/*.dll
#     TargetConfig = $${PWD}/config.ini
#     #将输入目录中的"/"替换为"\"
#     TargetConfig = $$replace(TargetConfig, /, \\)
#     #将输出目录中的"/"替换为"\"
#     OutputDir =  $${OUT_PWD}/$${DESTDIR}
#     OutputDir = $$replace(OutputDir, /, \\)
#     #执行copy命令
#     #QMAKE_POST_LINK += copy /Y \"$$TargetConfig\" \"$$OutputDir\"
#     QMAKE_POST_LINK += cp \"$$TargetConfig\" \"$$OutputDir\" &

#     # 首先，定义static文件夹的路径
#     StaticDir = $${PWD}/static
#     # 将路径中的"/"替换为"\"
#     StaticDir = $$replace(StaticDir, /, \\)
#     #message($${StaticDir})
#     # 使用xcopy命令拷贝文件夹，/E表示拷贝子目录及其内容，包括空目录。/I表示如果目标不存在则创建目录。/Y表示覆盖现有文件而不提示。
#     QMAKE_POST_LINK += xcopy /E /I /Y \"$$StaticDir\" \"$$OutputDir\\static\\\"
# }else{
#     message("release mode");
#     #指定要拷贝的文件目录为工程目录下release目录下的所有dll、lib文件，例如工程目录在D:\QT\Test
#     #PWD就为D:/QT/Test，DllFile = D:/QT/Test/release/*.dll
#     TargetConfig = $${PWD}/config.ini
#     #将输入目录中的"/"替换为"\"
#     TargetConfig = $$replace(TargetConfig, /, \\)
#     #将输出目录中的"/"替换为"\"
#     OutputDir =  $${OUT_PWD}/$${DESTDIR}
#     OutputDir = $$replace(OutputDir, /, \\)
#     #执行copy命令
#     #QMAKE_POST_LINK += copy /Y \"$$TargetConfig\" \"$$OutputDir\"
#     QMAKE_POST_LINK += cp \"$$TargetConfig\" \"$$OutputDir\" &

#     # 首先，定义static文件夹的路径
#     StaticDir = $${PWD}/static
#     # 将路径中的"/"替换为"\"
#     StaticDir = $$replace(StaticDir, /, \\)
#     #message($${StaticDir})
#     # 使用xcopy命令拷贝文件夹，/E表示拷贝子目录及其内容，包括空目录。/I表示如果目标不存在则创建目录。/Y表示覆盖现有文件而不提示。
#     QMAKE_POST_LINK += xcopy /E /I /Y \"$$StaticDir\" \"$$OutputDir\\static\\\"
# }

CONFIG(debug, debug|release){
    DESTDIR = $$OUT_PWD/bin/debug
}else{
    DESTDIR = $$OUT_PWD/bin/release
}

OutputDir = $$DESTDIR

#打印路径日志
message("当前构建模式: " $$CONFIG)
message("项目根目录: $$PWD")
message("构建目录: $$OUT_PWD")
message("目标 bin 目录: $$OutputDir")

#创建bin目录
QMAKE_POST_LINK += mkdir -p "$${OutputDir}" &&
#复制config.ini到bin目录
QMAKE_POST_LINK += cp -f "$${PWD}/config.ini" "$${OutputDir}/" &&
#复制static文件夹到bin/static
QMAKE_POST_LINK += cp -rf "$${PWD}/static" "$${OutputDir}/" &&
#复制完成提示
QMAKE_POST_LINK += echo "文件复制完成，已复制到 $${OutputDir}"

win32-g++:QMAKE_LFLAGS += -mwindows
win32-g++:DEFINES += QT_NEEDS_QMAIN
win32-msvc*:QMAKE_CXXFLAGS += /wd"4819" /utf-8

# 修复 Qt 6 入口点库链接问题
win32-g++:LIBS += -Wl,--subsystem,windows -lmingw32
