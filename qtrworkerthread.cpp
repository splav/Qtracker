#include "qtrworkerthread.h"

#include <QtNetwork>
#include <arpa/inet.h>


#define INBUF_SIZE 32768

static char fromhex(char x) {
  if( (x-'0')<=9 ) return (x-'0');
  if( (x-'A')<=5 ) return (x-'A'+10);
  if( (x-'a')<=5 ) return (x-'a'+10);
  return 0xff;
}

QByteArray decode(QByteArray in)
{
    QByteArray d;
    char b;
    char c;
    for (int i = 0; i < in.size(); ++i) {
        if (in[i] == '%') {
            b = fromhex(in[++i]);
            c = fromhex(in[++i]);
            c|=(b<<4);
            d.append(c);
        } else
            d.append(in[i]);
    }
    return d;
}

QTRWorkerThread::QTRWorkerThread(struct SharedData *shared, int socketDescriptor)
    :sock_id(socketDescriptor),data(shared)
{
    db_gate = data->db_gate;
}

void QTRWorkerThread::http_error(const char *error)
{
    QString err = QString(error);
    reply = QString("d14:failure reason%1:").arg(err.size()).append(err).append("e").toAscii();
}

void QTRWorkerThread::http_status()
{
    reply = QString("Version: 2.7.1; total torrents: %1; total users: %2;").arg(data->tr.size()).arg(data->u.size()).toAscii();
}

void QTRWorkerThread::http_announce(QByteArray in)
{
    QByteArray hash;
    QByteArray pk;
    Peer pd;
    User user;

    uint expire = QDateTime::currentDateTime().toTime_t() + data->announce_interval + data->expire_delta;

    QString sip = QString("%1").arg(htonl(serv),8,16,QChar('0'));
    Host host;
    host.ip = QString("%1").arg(htonl(address),8,16,QChar('0'));
    host.port = htons(6881);

    bool started = false;
    bool stopped = false;
    bool seeding = false;

    qlonglong up   = 0;
    qlonglong down = 0;

    foreach (QByteArray st, in.split('&'))
        if(!st.isEmpty())
        {
            QList<QByteArray> tmp = st.split('=');
            if (tmp.size() != 2)
                continue;

            if (tmp[0].startsWith("left")) { //left match
                if (!tmp[1].toLongLong())
                    seeding = true;
                continue;
            }

            if (tmp[0].startsWith("event")) { //event match
                if (tmp[1].startsWith("stop"))
                    stopped = true;
                else if (tmp[1].startsWith("start"))
                    started = true;
                else if ( !tmp[1].startsWith("compl") )
                    return http_error("Invalid event");
                continue;
            }

            if (tmp[0].startsWith("num")) { //numwant match
                int tn = tmp[1].toInt();
                if (tn < numwant)
                    numwant = tn;
                continue;
            }

            if (tmp[0].startsWith("compact")) { //compact match
                if ( !tmp[1].toInt())
                    return http_error("Please enable COMPACT MODE in your client");
                continue;
            }

            if (tmp[0].startsWith("download")) { //download match
                down = tmp[1].toLongLong();
                if (down < 0)
                    return http_error("Invalid downloaded");
                continue;
            }

            if (tmp[0].startsWith("upload")) { //upload match
                up = tmp[1].toLongLong();
                if (up < 0)
                    return http_error("Invalid uploaded");
                continue;
            }

            if (tmp[0].startsWith("info_hash")) { //info_hash match
                hash = decode(tmp[1]);
                if (hash.size() != 20)
                    return http_error("Invalid hash");
                continue;
            }

            if (tmp[0].startsWith("port")) { //port match
                host.port = htons(tmp[1].toInt());
                continue;
            }

            if (tmp[0].startsWith("uk")) { //uk match
                pk = tmp[1];
                if (pk.size() != 10)
                    return http_error("Please LOG IN and REDOWNLOAD this torrent (passkey not found)");
                continue;
            }
	}

    QByteArray pid = QByteArray((char*)&address,4).append(QByteArray((char*)&(host.port),2));

//now we have all data we needed
data->trackerLock.lockForWrite();

    bool peer_exist = data->tr[hash].peers.contains(pid);
    Peer opd = data->tr[hash].peers[pid];
    pd = opd;

    //setting non static data
    pd.seeding = seeding;
    pd.expire = expire;
    pd.up = up;
    pd.down = down;
    pd.peer_id = pid;

    user = data->u[pk];
    if(!user.user_id && !db_gate->getUserData(user, pk)){
        data->u.remove(pk);
data->trackerLock.unlock();
        return http_error("User not registered");
    }

    pd.user_id = user.user_id;

    if (!peer_exist && !db_gate->getTorrentData(pd, hash, sip)){
        data->tr.remove(hash);
data->trackerLock.unlock();
        return http_error("Torrent not registered");
    }

data->trackerLock.unlock();
    if (pd.seeding){
        if (!db_gate->setSeederLastSeen(pd))
            return http_error("internal db error: setSeederLastSeen");
    }

    if (started || !peer_exist) {
        opd.up   = pd.up;
        opd.down = pd.down;
    }

    //torrent statistics
    qlonglong add_up    = (pd.up >= opd.up)?        pd.up - opd.up     : pd.up;
    qlonglong add_down  = (pd.down >= opd.down)?    pd.down - opd.down : pd.down;

    pd.s_up   += add_up;
    pd.s_down += add_down;

    //user statistics
    if (data->free || pd.free)
        add_down = 0;

    user.up    += add_up;
    user.down  += add_down;
    user.bonus += 2 * add_up * data->free;
    user.expire = expire;

    //torrent
    if (!db_gate->setStatus(pd, sip, host))
        return http_error("internal db error: setStatus");

    //user
    if (pd.primary && !db_gate->setUserStat(user)) //set user data only if we are the primary
        return http_error("internal db error: upUserStat");

    // standart reply
    reply = QString("d8:completei0e10:incompletei0e8:intervali%1e5:peers0:e").arg(data->announce_interval).toAscii();

data->trackerLock.lockForWrite();
    data->u[pk] = user;

    if (stopped && peer_exist){
        data->tr[hash].seedc -= opd.seeding;
        data->tr[hash].peers.remove(pd.peer_id);
data->trackerLock.unlock();
        return;
    }

    data->tr[hash].seedc += pd.seeding - opd.seeding; //inc if l->s, dec if s->l
    pd.expire = expire;

    data->tr[hash].peers[pd.peer_id] = pd;
data->trackerLock.unlock();

//return peers
//====================================================================================

        if (numwant > 0){

data->trackerLock.lockForRead();
            Torrent tr = data->tr[hash];

            if (numwant > tr.peers.size())
                numwant = tr.peers.size();

            reply = QString("d8:completei%1e10:incompletei%2e8:intervali%3e5:peers%4:"
                                       ).arg(tr.seedc).arg(tr.peers.size()-tr.seedc).arg(data->announce_interval).arg(6*numwant).toAscii();

            if (numwant == tr.peers.size())
                foreach(QByteArray peer, tr.peers.keys())
                    reply.append(peer);
            else
                for(int i = 0; i < numwant; ++i)
                    reply.append(tr.peers.keys()[qrand() % tr.peers.size()]);

data->trackerLock.unlock();

            reply.append('e');
        }
//====================================================================================
}


void QTRWorkerThread::http_request(QByteArray in)
{
    if (!in.startsWith("GET /") ) return http_error("HTTP 400");

    QList<QByteArray> tmp = in.split(' ');
    if (tmp.size() < 3)
        http_error("HTTP 400");

    tmp = tmp[1].split('/').last().split('?');
    // getting the string after '/' then splitting it by '?'

    if( (tmp.size() > 1) && ( tmp[0].startsWith("ann") || tmp[0].isEmpty()))
        http_announce((tmp.size()>2) ? tmp[1].append(tmp[2]) : tmp[1]);
    else {
        if( (tmp.size() == 1) && (tmp[0] == "stat"))
            http_status();
        else
            http_error("HTTP 404");
    }

}

void QTRWorkerThread::run()
{
    QTcpSocket sock;
    QByteArray req, header;

    if (!sock.setSocketDescriptor(sock_id)) {
        return;
    }

    address = htonl(sock.peerAddress().toIPv4Address());
    serv = htonl(sock.localAddress().toIPv4Address());

    if(!sock.waitForReadyRead(3000))
        goto fin;

data->configLock.lockForRead();
    if (data->bans.contains(sock.peerAddress().toString())){
        http_error("You are banned, see http://unsorted.ru/viewtopic.php?t=2867");
data->configLock.unlock();
        goto send;
    }

    numwant = data->cfg["numwant"].toInt();
data->configLock.unlock();

    now = QDateTime::currentDateTime().toTime_t();
    req = sock.readLine();

    //ugly proxy hack
    header = sock.readLine();
    if (header.startsWith("X-Real-IP:")){
        QHostAddress peer_addr(QString(header.split(' ').last()));
        address = htonl(peer_addr.toIPv4Address());
    }

    http_request(req);

send:
    reply = QByteArray("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ").append(QByteArray::number(reply.size())).append("\r\n\r\n").append(reply);
    sock.write(reply);
    sock.flush();
fin:
    sock.disconnectFromHost();
    sock.close();
}
