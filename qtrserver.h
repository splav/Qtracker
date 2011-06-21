#ifndef FORTUNESERVER_H
#define FORTUNESERVER_H

#include <QTcpServer>

#include "qtrworkerthread.h"

class QTRServer : public QTcpServer
{
    Q_OBJECT

public:
    int defaultPort;
    QTRServer(QObject *parent = 0);

private slots:
    void reloadAndClean();

public slots:
    void finished();

protected:
    void incomingConnection(int socketDescriptor);

private:
    QSettings * settings;

    QTimer cfgTimer;
    SharedData data;
    int workerThreads;
    int maxPendingConnections;
    int maxWorkerThreads;

    void http_error(QTcpSocket * socket, int code);
    void http_request(QTcpSocket * socket, QString * data, size_t length);
};

#endif
