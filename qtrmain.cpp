#include <QCoreApplication>
#include <QtCore>

#include "qtrserver.h"

#include <stdlib.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qsrand(QTime(0,0,0).secsTo(QTime::currentTime()));
    QTRServer server(&app);
    if (!server.listen(QHostAddress::Any, server.defaultPort))
        qFatal("Unable to start the server: %s", server.errorString().toAscii().data());

    qDebug() << "server is running on port" << server.serverPort();
    return app.exec();
}
