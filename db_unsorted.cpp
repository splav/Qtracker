#include "db_unsorted.h"
#include "arpa/inet.h"

DBUnsorted::DBUnsorted(struct SharedData *shared, QObject *parent)
    : QObject(parent)
{  
        data = shared;

data->settingsLock.lockForWrite();
        dbserver   = data->settings->value("unsorted/dbserver",   "unsorted.me").toString();
        dbdatabase = data->settings->value("unsorted/dbdatabase", "unsorted"   ).toString();
        dbuser     = data->settings->value("unsorted/dbuser",     "bt"         ).toString();
        dbpassword = data->settings->value("unsorted/dbpassword", "password"   ).toString();

        data->settings->setValue("unsorted/dbserver",   dbserver  );
        data->settings->setValue("unsorted/dbdatabase", dbdatabase);
        data->settings->setValue("unsorted/dbuser",     dbuser    );
        data->settings->setValue("unsorted/dbpassword", dbpassword);
data->settingsLock.unlock();

        db = QSqlDatabase::addDatabase("QMYSQL","dbthread");
        db.setHostName(dbserver);
        db.setDatabaseName(dbdatabase);
        db.setUserName(dbuser);
        db.setPassword(dbpassword);
        if (!db.open())
                qFatal("db connection error");

        qup_seeder_last_seen = new QSqlQuery(db);
        qup_seeder_last_seen->setForwardOnly(true);
        qup_seeder_last_seen->prepare("UPDATE phpbb_bt_torrents SET complete_count = :compl, seeder_last_seen = :seeder_ls, last_seeder_uid = :user_id WHERE torrent_id = :torrent_id");

        qget_stat = new QSqlQuery(db);
        qget_stat->setForwardOnly(true);
        qget_stat->prepare("SELECT main_ip, expire_time, t_up_total, t_down_total FROM phpbb_bt_stat WHERE torrent_id = :t_id AND user_id = :u_id LIMIT 1");

                qget_torrent_data = new QSqlQuery(db);
                qget_torrent_data->setForwardOnly(true);
                qget_torrent_data->prepare("SELECT t.torrent_id, t.topic_id, t.bt_gold, t.size FROM phpbb_bt_torrents t, phpbb_topics tp WHERE tp.topic_id = t.topic_id AND t.info_hash = unhex( :hash ) LIMIT 1");

                qget_user_data = new QSqlQuery(db);
                qget_user_data->setForwardOnly(true);
                qget_user_data->prepare("SELECT user_id FROM phpbb_bt_users WHERE auth_key = :pk  LIMIT 1");

                qget_user_stat_data = new QSqlQuery(db);
                qget_user_stat_data->setForwardOnly(true);
                qget_user_stat_data->prepare("SELECT u_up_total, u_down_total, u_bonus_total FROM phpbb_bt_users_stat WHERE user_id = :user_id  LIMIT 1");

                qrepl_stat = new QSqlQuery(db);
                qrepl_stat->setForwardOnly(true);
                qrepl_stat->prepare("REPLACE INTO phpbb_bt_stat (topic_id, torrent_id, user_id, user_status, compl_count, t_up_total, t_down_total, expire_time, main_ip) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");

        qrepl_user_stat = new QSqlQuery(db);
        qrepl_user_stat->setForwardOnly(true);
        qrepl_user_stat->prepare("REPLACE INTO phpbb_bt_users_stat (user_id, u_up_total, u_down_total, u_bonus_total) VALUES (?, ?, ?, ?)");

        qrepl_tracker = new QSqlQuery(db);
        qrepl_tracker->setForwardOnly(true);
        qrepl_tracker->prepare("REPLACE INTO phpbb_bt_tracker (torrent_id, user_id, seeder, expire_time, ip ,port) VALUES (:tr_id, :u_id, :seeder, :exp_time, :ip, :port)");
}

bool DBUnsorted::getUserData(User &u, QByteArray pk)
{
    QMutexLocker locker(&dblock);

    qget_user_data->bindValue(0, pk);
    qget_user_data->exec();

    if (qget_user_data->next())
    {
        u.user_id = qget_user_data->value(0).toInt();
    } else {
        return false;
    }

    qget_user_stat_data->bindValue(0, u.user_id);
    qget_user_stat_data->exec();

    if (qget_user_stat_data->next()) {
        u.up    = qget_user_stat_data->value(0).toLongLong();
        u.down  = qget_user_stat_data->value(1).toLongLong();
        u.bonus = qget_user_stat_data->value(2).toLongLong();
    } else {
        u.up	= 0;
        u.down	= 0;
        u.bonus	= 5368709120LL;
    }

    return true;
}

bool DBUnsorted::getTorrentData(Peer &pd, QByteArray hash, QString sip)
{
    QMutexLocker locker(&dblock);

    qget_torrent_data->bindValue(0,hash.toHex());
    qget_torrent_data->exec();

    if (qget_torrent_data->next()) {
        pd.torrent_id = qget_torrent_data->value(0).toUInt();
        pd.topic_id   = qget_torrent_data->value(1).toUInt();
        pd.free       = qget_torrent_data->value(2).toBool();
    } else
        return false;

    qget_stat->bindValue(0,pd.torrent_id);
    qget_stat->bindValue(1,pd.user_id);
    qget_stat->exec();
    if (qget_stat->next())
    {
        pd.primary = ( qget_stat->value(0).toString() == sip)
                        || (qget_stat->value(1).toUInt() < QDateTime::currentDateTime().toTime_t() );
        pd.expire  = qget_stat->value(1).toUInt();
        pd.s_up    = qget_stat->value(2).toLongLong();
        pd.s_down  = qget_stat->value(3).toLongLong();
    } else {
        pd.primary = true;
        pd.s_up    = 0;
        pd.s_down  = 0;
    }

    return true;
}

bool DBUnsorted::setSeederLastSeen(Peer &pd)
{
QMutexLocker locker(&dblock);

    qup_seeder_last_seen->bindValue(0,pd.seeding);
    qup_seeder_last_seen->bindValue(1,QDateTime::currentDateTime().toTime_t());
    qup_seeder_last_seen->bindValue(2,pd.user_id);
    qup_seeder_last_seen->bindValue(3,pd.torrent_id);
    return qup_seeder_last_seen->exec();
}

bool DBUnsorted::setStatus(Peer &pd, QString sip, uint now)
{
QMutexLocker llocker(&slock);

    setStatusData.topic_id.append(pd.topic_id);
    setStatusData.torrent_id.append(pd.torrent_id);
    setStatusData.user_id.append(pd.user_id);
    setStatusData.user_status.append(pd.seeding+1);
    setStatusData.compl_count.append(pd.seeding);
    setStatusData.s_up.append(pd.s_up);
    setStatusData.s_down.append(pd.s_down);
    setStatusData.expire.append(now+300);
    setStatusData.main_ip.append(sip);


    if(setStatusData.topic_id.size()>50)
    {
        SetStatusData temp = setStatusData;

        setStatusData.topic_id.clear();
        setStatusData.torrent_id.clear();
        setStatusData.user_id.clear();
        setStatusData.user_status.clear();
        setStatusData.compl_count.clear();
        setStatusData.s_up.clear();
        setStatusData.s_down.clear();
        setStatusData.expire.clear();
        setStatusData.main_ip.clear();

llocker.unlock();
QMutexLocker locker(&dblock);

        //topic_id, torrent_id, user_id, user_status, compl_count, t_up_total, t_down_total, expire_time, main_ip
        if (pd.primary)
        {
            qrepl_stat->bindValue(0,temp.topic_id);
            qrepl_stat->bindValue(1,temp.torrent_id);
            qrepl_stat->bindValue(2,temp.user_id);
            qrepl_stat->bindValue(3,temp.user_status);
            qrepl_stat->bindValue(4,temp.compl_count);
            qrepl_stat->bindValue(5,temp.s_up);
            qrepl_stat->bindValue(6,temp.s_down);
            qrepl_stat->bindValue(7,temp.expire);
            qrepl_stat->bindValue(8,temp.main_ip);

            if (!qrepl_stat->execBatch())
            {
qDebug() << qrepl_stat->lastError().text();
                    return false;
            }

            temp.topic_id.clear();
            temp.torrent_id.clear();
            temp.user_id.clear();
            temp.user_status.clear();
            temp.compl_count.clear();
            temp.s_up.clear();
            temp.s_down.clear();
            temp.expire.clear();
            temp.main_ip.clear();
        }
    }
    return true;
}

bool DBUnsorted::setUserStat(User &u)
{
QMutexLocker llocker(&ulock);

    setUserData.user_id.append(u.user_id);
    setUserData.up.append(u.up);
    setUserData.down.append(u.down);
    setUserData.bonus.append(u.bonus);

    if(setUserData.user_id.size()>50)
    {

        SetUserData temp = setUserData;

        setUserData.user_id.clear();
        setUserData.up.clear();
        setUserData.down.clear();
        setUserData.bonus.clear();

llocker.unlock();
QMutexLocker locker(&dblock);

        qrepl_user_stat->bindValue(0,temp.user_id);
        qrepl_user_stat->bindValue(1,temp.up);
        qrepl_user_stat->bindValue(2,temp.down);
        qrepl_user_stat->bindValue(3,temp.bonus);

        if (!qrepl_user_stat->execBatch())
        {
qDebug() << qrepl_user_stat->lastError().text();
            return false;
        }

        temp.user_id.clear();
        temp.up.clear();
        temp.down.clear();
        temp.bonus.clear();

    }

    return true;
}


bool DBUnsorted::setTracker(Peer &pd, Host &host, uint exp_time)
{

QMutexLocker llocker(&tlock);
    //:tr_id, :u_id, :seeder, :exp_time, :ip, :port

    setTrackerData.torrent_id.append(pd.torrent_id);
    setTrackerData.user_id.append(pd.user_id);
    setTrackerData.seeding.append(pd.seeding);
    setTrackerData.expire.append(exp_time);
    setTrackerData.ip.append(host.ip);
    setTrackerData.port.append(htons(host.port));


    if(setTrackerData.torrent_id.size()>50)
    {
        SetTrackerData temp = setTrackerData;

        setTrackerData.torrent_id.clear();
        setTrackerData.user_id.clear();
        setTrackerData.seeding.clear();
        setTrackerData.expire.clear();
        setTrackerData.ip.clear();
        setTrackerData.port.clear();

llocker.unlock();
QMutexLocker locker(&dblock);

        qrepl_tracker->bindValue(0,temp.torrent_id);
        qrepl_tracker->bindValue(1,temp.user_id);
        qrepl_tracker->bindValue(2,temp.seeding);
        qrepl_tracker->bindValue(3,temp.expire);
        qrepl_tracker->bindValue(4,temp.ip);
        qrepl_tracker->bindValue(5,temp.port);

        if (!qrepl_tracker->execBatch())
        {
            qDebug() << qrepl_tracker->lastError().text();
            return false;
        }

        temp.torrent_id.clear();
        temp.user_id.clear();
        temp.seeding.clear();
        temp.expire.clear();
        temp.ip.clear();
        temp.port.clear();

    }
    return true;
}


bool DBUnsorted::getConfig()
{
QMutexLocker locker(&dblock);

    data->configLock.lockForWrite();

    qDebug() << "db rescan:" << QDateTime::currentDateTime();
    //get config and ban data from db
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL","main_thread");
        db.setHostName(dbserver);
        db.setDatabaseName(dbdatabase);
        db.setUserName(dbuser);
        db.setPassword(dbpassword);
        if (!db.open()){
            qDebug("db connection error !!!!!!!");
            return false;
        }

        QSqlQuery query(db);
        query.setForwardOnly(true);
        query.exec("SELECT * FROM phpbb_bt_config");
        while (query.next())
            data->cfg[query.value(0).toString()] = query.value(1).toString();

        data->free              = data->cfg["free_time"].toInt();
        data->expire_delta      = data->cfg["uqt_expire_delta"].toInt();
        data->announce_interval = data->cfg["uqt_announce_interval"].toInt();

        query.exec("SELECT ban_ip, user_id FROM phpbb_bt_banlist");
        while (query.next())
            data->bans[query.value(0).toString()] = query.value(1).toInt();

        query.exec(QString("DELETE FROM phpbb_bt_tracker WHERE expire_time < UNIX_TIMESTAMP()"));
        db.close();
    }

    QSqlDatabase::removeDatabase("main_thread");

    data->autoclean_interval = data->cfg["uqt_trcleanup_cfgupdate_interval"].toInt();
    data->configLock.unlock();

    return true;
}
