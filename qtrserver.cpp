#include "qtrserver.h"

#include <QDebug>

#include <stdlib.h>

void QTRServer::reloadAndClean()
{
    data.db_gate->getConfig();

    uint current = QDateTime::currentDateTime().toTime_t();
data.trackerLock.lockForWrite();

    //remove expired tracker records
    QHash < QByteArray, Torrent >::iterator i = data.tr.begin();
    while (i != data.tr.end())
    {
        QHash < QByteArray, Peer >::iterator j = i.value().peers.begin();
        while (j != i.value().peers.end())
            if (j.value().expire < current )
            {
                if (j.value().seeding)
                    --i.value().seedc;

                j = i.value().peers.erase(j);
            } else ++j;

        if (i.value().peers.isEmpty())
            i = data.tr.erase(i);
        else ++i;
    }

    // remove expired users from cache
    QHash < QByteArray, User >::iterator j = data.u.begin();
    while (j != data.u.end())
        if (j.value().expire < current )
            j = data.u.erase(j);
        else ++j;

data.trackerLock.unlock();

    cfgTimer.start(data.autoclean_interval * 1000);
}

QTRServer::QTRServer(QObject *parent)
    : QTcpServer(parent)
{
    data.settings = new QSettings("/etc/qtracker.conf", QSettings::IniFormat, this);

    //read global configuration
    defaultPort      = data.settings->value("global/port",               80).toInt();
    maxWorkerThreads = data.settings->value("global/max_worker_threads", 25).toInt();

    //write basic configuration
    data.settings->setValue("global/port",               defaultPort     );
    data.settings->setValue("global/max_worker_threads", maxWorkerThreads);
    data.settings->sync();

    data.db_gate = new DBUnsorted(&data, this);
    data.sockets = new QSemaphore(800);

    data.log_error = &log_error;

    reloadAndClean();
    connect(&cfgTimer, SIGNAL(timeout()), this, SLOT(reloadAndClean()));

    setMaxPendingConnections(maxWorkerThreads);
    //workerThreads = 0;
    threadPool = new QThreadPool(this);
    threadPool->setMaxThreadCount(maxWorkerThreads);
}

void QTRServer::incomingConnection(int socketDescriptor)
{
/*
    if(workerThreads < maxWorkerThreads){
        ++workerThreads;
        QTRWorkerThread *thread = new QTRWorkerThread(&data, this, socketDescriptor);
        connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
        connect(thread, SIGNAL(finished()), this, SLOT(finished()));
        thread->start();
    }
    else
    {
        QTcpSocket sock;
        sock.setSocketDescriptor(socketDescriptor);
        sock.close();
    }
*/

    if(data.sockets->tryAcquire()){
    QTRWorkerThread *thread = new QTRWorkerThread(&data, socketDescriptor);
        thread->setAutoDelete(true);

        threadPool->start(thread);
    } else {
        QTcpSocket sock;
        sock.setSocketDescriptor(socketDescriptor);
        sock.close();
    }
}

void log_error(QByteArray in)
{
    QFile log("/var/log/qtracker.log");
    if (!log.open(QIODevice::ReadWrite | QIODevice::Text))
             return;

    QTextStream out(&log);

    out << in;

    out.flush();
    log.close();
}

/*
void QTRServer::finished()
{
    --workerThreads;
}
*/
