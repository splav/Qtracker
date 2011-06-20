HEADERS = \
    db_unsorted.h \
    struct.h \
    qtrserver.h \
    qtrworkerthread.h
SOURCES = \
    main.cpp \
    db_unsorted.cpp \
    qtrserver.cpp \
    qtrworkerthread.cpp
QT = network \
    sql \
    core

#DYLD_IMAGE_SUFFIX=_debug

QMAKE_CXXFLAGS_RELEASE += -DQT_NO_DEBUG_OUTPUT
QMAKE_CXXFLAGS_DEBUG += -O0 -g 
