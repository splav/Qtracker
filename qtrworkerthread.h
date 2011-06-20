#ifndef FORTUNETHREAD_H
#define FORTUNETHREAD_H

#include "struct.h"
#include "db_unsorted.h"

#include <QThread>

class QTRWorkerThread : public QThread
{
    Q_OBJECT

public:
    QTRWorkerThread(struct SharedData *shared, QObject *parent, int socketDescriptor);

    void run();

signals:
    void error(QTcpSocket::SocketError socketError);

private:
    DBUnsorted *db_gate;

    //int n;

    int sock_id;
    int numwant;
    quint32 address;
    quint32 serv;
    uint now;

    QByteArray reply;

    struct SharedData *data;

    void http_error(const char *error);
    void http_status();
    void http_request(QByteArray data);
    void http_announce(QByteArray data);
};

#endif
