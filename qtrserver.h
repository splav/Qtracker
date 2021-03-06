#ifndef FORTUNESERVER_H
#define FORTUNESERVER_H

#include <QTcpServer>
#include <QThreadPool>
#include <QMutexLocker>

#include "qtrworkerthread.h"

void log_error(QByteArray in);

class QTRServer : public QTcpServer
{
    Q_OBJECT

public:
    int defaultPort;
    QTRServer(QObject *parent = 0);

private slots:
    void reloadAndClean();

protected:
    void incomingConnection(int socketDescriptor);

private:
    QSettings *settings;
    QThreadPool *threadPool;

    QTimer cfgTimer;
    SharedData data;

    int maxWorkerThreads;
};

#endif
