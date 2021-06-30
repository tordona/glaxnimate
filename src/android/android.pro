QT += widgets xml uitools concurrent svg
# requires(qtConfig(listview))

CONFIG += c++17

INCLUDEPATH += $$PWD/../core $$PWD/../gui $$PWD/../../external/QtAppSetup/src $$PWD/../../external/Qt-Color-Widgets/include

LIBS += -L../../external/Qt-Color-Widgets -L../core -L../gui -L../../external/QtAppSetup -lglaxnimate_gui -lglaxnimate_core -lQtAppSetup -lQtColorWidgets -lz

!android {
    DEFINES += Q_OS_ANDROID
}

SOURCES = \
    main.cpp \
    main_window.cpp \
    glaxnimate_app_android.cpp

FORMS += \
    main_window.ui

HEADERS += \
    glaxnimate_app_android.hpp \
    main_window.hpp

RESOURCES += \
    resources.qrc
