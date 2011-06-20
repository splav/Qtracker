/*
 * struct.h
 *
 *  Created on: 11.08.2008
 *      Author: splav
 */

#ifndef STRUCT_H_
#define STRUCT_H_

#include <QHash>
#include <QCache>
#include <QList>
#include <QMap>
#include <QSet>
#include <QTime>
#include <QDateTime>
#include <QByteArray>
#include <QStringList>
#include <QTimer>
#include <QMutex>
#include <QSemaphore>
#include <QReadWriteLock>
#include <QTcpSocket>
#include <QSettings>

class QTRWorkerThread;
class DBUnsorted;

struct User {
    quint32 user_id;
    quint32 expire;
    qlonglong up;
    qlonglong down;
    qlonglong bonus;
};

struct Host {
    QString ip;
    quint16 port;
};

struct Peer {
    bool seeding;
    quint32 expire;
    qlonglong up;
    qlonglong down;

    quint32 user_id;
    quint32 torrent_id;
    quint32 topic_id;

    qlonglong s_up;
    qlonglong s_down;

    QByteArray peer_id;

    bool free;
    bool primary; // we are the primary node for this item
};


typedef QHash <QByteArray,Peer> PeerList;

struct Torrent {
    quint32 peerc;
    quint32 seedc;
    PeerList peers;
};

typedef QHash <QByteArray, Torrent> TorrentList;
typedef QHash <QByteArray, User>    UserList;
typedef QHash <QString,QString>     ConfigList;
typedef QHash <QString,int>         BanList;


struct SharedData {
    BanList     bans;
    TorrentList tr;
    UserList    u;
    ConfigList  cfg;

    QReadWriteLock trackerLock;
    QReadWriteLock configLock;
    QReadWriteLock settingsLock;

    bool free;
    int  expire_delta;
    int  announce_interval;
    int  autoclean_interval;

    DBUnsorted *db_gate;
    QSettings * settings;
};

#endif /* STRUCT_H_ */
