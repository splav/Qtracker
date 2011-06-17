HEADERS = \
    fortuneserver.h \
    fortunethread.h \
    db_unsorted.h \
    struct.h
SOURCES = fortuneserver.cpp \
    fortunethread.cpp \
    main.cpp \
    db_unsorted.cpp
QT = network \
    sql \
    core

#DYLD_IMAGE_SUFFIX=_debug

QMAKE_CXXFLAGS_RELEASE += -Os -DQT_NO_DEBUG_OUTPUT
QMAKE_CXXFLAGS_DEBUG += -O0 -g 
