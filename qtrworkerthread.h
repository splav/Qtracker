#ifndef FORTUNETHREAD_H
#define FORTUNETHREAD_H

#include "qtrstruct.h"
#include "mod_db_unsorted.h"

//#include <QThread>
#include <QRunnable>

class QTRWorkerThread : public QRunnable
{

public:
    QTRWorkerThread(struct SharedData *shared, int socketDescriptor);

    void run();

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
