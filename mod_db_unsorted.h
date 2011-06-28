#ifndef DB_UNSORTED_H
#define DB_UNSORTED_H

#include <QtSql>
#include <QCache>
#include <QVariantList>
#include "qtrstruct.h"

struct SetStatusData {
    QVariantList topic_id;
    QVariantList torrent_id;
    QVariantList user_id;
    QVariantList user_status;
    QVariantList s_up;
    QVariantList s_down;
    QVariantList expire;
    QVariantList main_ip;
    QVariantList ip;
    QVariantList port;
    QVariantList seeding;
};

struct SetUserData {
    QVariantList user_id;
    QVariantList up;
    QVariantList down;
    QVariantList bonus;
};


class DBUnsorted : public QObject
{
    Q_OBJECT

public:
    DBUnsorted(struct SharedData *shared, QObject *parent);

    bool getUserData(User &u, QByteArray pk);
    bool getTorrentData(Peer &pd, QByteArray hash, QString sip);
    bool setSeederLastSeen(Peer &pd);
    bool setStatus(Peer &pd, QString sip, uint now, Host &host);
    bool setUserStat(User &u);
    bool getConfig();

private:
    QString sip;
    struct SharedData *data;

    QMutex dblock;
    QMutex ulock;
    QMutex slock;

    QSqlDatabase db;

    QSqlQuery *qup_seeder_last_seen;
    QSqlQuery *qget_stat;
    QSqlQuery *qget_torrent_data;
    QSqlQuery *qget_user_data;
    QSqlQuery *qget_user_stat_data;
    QSqlQuery *qrepl_stat;
    QSqlQuery *qrepl_user_stat;
    QSqlQuery *qrepl_tracker;

    SetStatusData  setStatusData;
    SetUserData    setUserData;

    QString dbserver;
    QString dbdatabase;
    QString dbuser;
    QString dbpassword;
};

#endif // DB_UNSORTED_H
