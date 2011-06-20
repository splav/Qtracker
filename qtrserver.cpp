/****************************************************************************
**
** Copyright (C) 2005-2008 Trolltech ASA. All rights reserved.
**
** This file is part of the example classes of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License versions 2.0 or 3.0 as published by the Free Software
** Foundation and appearing in the files LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file.  Alternatively you may (at
** your option) use any later version of the GNU General Public
** License if such license has been publicly approved by Trolltech ASA
** (or its successors, if any) and the KDE Free Qt Foundation. In
** addition, as a special exception, Trolltech gives you certain
** additional rights. These rights are described in the Trolltech GPL
** Exception version 1.2, which can be found at
** http://www.trolltech.com/products/qt/gplexception/ and in the file
** GPL_EXCEPTION.txt in this package.
**
** Please review the following information to ensure GNU General
** Public Licensing requirements will be met:
** http://trolltech.com/products/qt/licenses/licensing/opensource/. If
** you are unsure which license is appropriate for your use, please
** review the following information:
** http://trolltech.com/products/qt/licenses/licensing/licensingoverview
** or contact the sales department at sales@trolltech.com.
**
** In addition, as a special exception, Trolltech, as the sole
** copyright holder for Qt Designer, grants users of the Qt/Eclipse
** Integration plug-in the right for the Qt/Eclipse Integration to
** link to functionality provided by Qt Designer and its related
** libraries.
**
** This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
** INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE. Trolltech reserves all rights not expressly
** granted herein.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include "qtrserver.h"

#include <QDebug>

#include <stdlib.h>
#include "config.h"

void QtrackerServer::reloadAndClean()
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

QtrackerServer::QtrackerServer(QObject *parent)
    : QTcpServer(parent)
{
    data.settings = new QSettings("/etc/qtracker.conf", QSettings::IniFormat, this);

    //read global configuration
    defaultPort      = data.settings->value("global/port",               80).toInt();
    maxWorkerThreads = data.settings->value("global/max_worker_threads", 25).toInt();

    //write basic configuration
    data.settings->setValue("global/port",               defaultPort     );
    data.settings->setValue("global/max_worker_threads", maxWorkerThreads);
    data->settings->sync();

    data.db_gate = new DBUnsorted(&data, this);

    reloadAndClean();
    connect(&cfgTimer, SIGNAL(timeout()), this, SLOT(reloadAndClean()));

    setMaxPendingConnections(maxWorkerThreads);
    workerThreads = 0;
}

void QtrackerServer::incomingConnection(int socketDescriptor)
{
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
}


void QtrackerServer::finished()
{
    --workerThreads;
}

