HEADERS = \
    qtrserver.h \
    qtrworkerthread.h \
    qtrstruct.h \
    mod_db_unsorted.h
SOURCES = \
    qtrserver.cpp \
    qtrworkerthread.cpp \
    qtrmain.cpp \
    mod_db_unsorted.cpp
QT = network \
    sql \
    core

#DYLD_IMAGE_SUFFIX=_debug

QMAKE_CXXFLAGS_RELEASE += -DQT_NO_DEBUG_OUTPUT
QMAKE_CXXFLAGS_DEBUG += -O0 -g 
