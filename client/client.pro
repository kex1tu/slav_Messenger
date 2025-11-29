QT += core gui
QT += network sql svg httpserver
QT += multimedia network
QT += sql


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
INCLUDEPATH += \
    $$PWD/core \
    $$PWD/models \
    $$PWD/delegates \
    $$PWD/ui \
    $$PWD/widgets \
    $$PWD/utils \
    $$PWD/resources
INCLUDEPATH += "$$PWD/opus-1.5.2/include"
LIBS += -L"$$PWD/opus-1.5.2/.libs" -lopus


SOURCES += \
    core/avatarcache.cpp \
    core/callservice.cpp \
    core/cryptoutils.cpp \
    core/databaseservice.cpp \
    core/tokenmanager.cpp \
    core/monocypher.c \
    main.cpp \
    mainwindow.cpp \
    core/dataservice.cpp \
    core/networkservice.cpp \
    models/chatmessagemodel.cpp \
    models/contactlistmodel.cpp \
    models/chatfilterproxymodel.cpp \
    delegates/chatmessagedelegate.cpp \
    delegates/contactlistdelegate.cpp \
    widgets/callwidget.cpp \
    ui/chatviewwidget.cpp \
    ui/loginwidget.cpp \
    widgets/profileviewwidget.cpp \
    widgets/incomingrequestswidget.cpp \
    widgets/searchresultspopup.cpp \
    ui/smoothlistview.cpp \
    ui/smoothtextedit.cpp \
    widgets/callhistorywidget.cpp \
    widgets/bottomsheetdialog.cpp \
    widgets/requestitemwidget.cpp \


HEADERS += \
    core/avatarcache.h \
    core/callservice.h \
    core/cryptoutils.h \
    core/databaseservice.h \
    core/tokenmanager.h \
    core/monocypher.h \
    mainwindow.h \
    core/dataservice.h \
    core/networkservice.h \
    core/structures.h \
    models/chatmessagemodel.h \
    models/contactlistmodel.h \
    models/chatfilterproxymodel.h \
    delegates/chatmessagedelegate.h \
    delegates/contactlistdelegate.h \
    widgets/callwidget.h \
    ui/chatviewwidget.h \
    ui/loginwidget.h \
    widgets/profileviewwidget.h \
    widgets/incomingrequestswidget.h \
    widgets/searchresultspopup.h \
    ui/smoothlistview.h \
    ui/smoothtextedit.h \
    widgets/callhistorywidget.h \
    widgets/bottomsheetdialog.h \
    widgets/requestitemwidget.h \



FORMS += \
    forms/chatviewwidget.ui \
    forms/loginwidget.ui \
    forms/mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

QMAKE_CXXFLAGS_CLAZY += -Wno-clazy-qcolor-from-literal
QMAKE_CXXFLAGS_RELEASE += -g
QMAKE_CXXFLAGS_DEBUG += -g

RESOURCES += \
    resources/resources.qrc
