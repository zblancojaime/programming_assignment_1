#include "ChatWindow.h"
#include <QVBoxLayout>
#include <QVariantMap>
#include <QDataStream>
#include <QHostAddress>
#include <QDebug>

ChatWindow::ChatWindow(QString id, quint16 port, QString nextId, quint16 nextPort, QWidget *parent)
    : QWidget(parent),
      myId(id),
      myPort(port),
      nextPeerId(nextId),
      nextPeerPort(nextPort),
      sequenceNumber(1),
      maxHops(4),
      incomingSocket(nullptr),
      nextPeerConnectionErrorShown(false) // initialize error flag for "Connection refused"
{
    // Layout
    QVBoxLayout *layout = new QVBoxLayout(this);

    chatLog = new QTextEdit(this);
    chatLog->setReadOnly(true); // display messages only

    input = new QLineEdit(this);
    sendButton = new QPushButton("Send", this);

    layout->addWidget(chatLog);
    layout->addWidget(input);
    layout->addWidget(sendButton);

    connect(sendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    connect(input, &QLineEdit::returnPressed, this, &ChatWindow::sendMessage);

    // Set up server to listen for incoming connections from previous peer
    server = new QTcpServer(this);
    connect(server, &QTcpServer::newConnection, this, &ChatWindow::onNewConnection);

    if (!server->listen(QHostAddress::LocalHost, myPort))
    {
        qDebug() << myId << "server failed to start on port" << myPort << ":" << server->errorString();
    }
    else
    {
        qDebug() << myId << "listening on port" << myPort;
    }

    // Prepare client socket to connect to next peer and retry until connected
    clientSocket = new QTcpSocket(this);

    // When client socket connects, flush queued frames
    connect(clientSocket, &QTcpSocket::connected, this, &ChatWindow::onClientConnected);

    // Log socket errors (show "Connection refused" only once)
    connect(clientSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, [this](QAbstractSocket::SocketError)
            {
                if (clientSocket->error() == QAbstractSocket::ConnectionRefusedError)
                {
                    if (!nextPeerConnectionErrorShown)
                    {
                        qDebug() << myId << "client socket error: Connection refused";
                        nextPeerConnectionErrorShown = true; // prevent repeating
                    }
                }
                else
                {
                    qDebug() << myId << "client socket error:" << clientSocket->errorString();
                } });

    // Start retry timer (attempt connect every 1s)
    retryTimer = new QTimer(this);
    retryTimer->setInterval(1000);
    connect(retryTimer, &QTimer::timeout, this, &ChatWindow::tryConnect);
    retryTimer->start();

    // initial attempt to connect to next peer
    tryConnect();
}

// Destructor MUST be declared here
ChatWindow::~ChatWindow()
{
    // Qt automatically deletes child widgets
}

// handle send button click
void ChatWindow::sendMessage()
{
    QString msg = input->text().trimmed();
    if (msg.isEmpty())
        return;

    chatLog->append("Me: " + msg); // display locally
    input->clear();

    // create message map
    QVariantMap message;
    message["ChatText"] = msg;
    message["Origin"] = myId;
    message["Destination"] = nextPeerId;
    message["Sequence"] = sequenceNumber++;
    message["HopCount"] = 1; // first hop

    // serialize payload
    QByteArray payload;
    {
        QDataStream payloadStream(&payload, QIODevice::WriteOnly);
        payloadStream.setVersion(QDataStream::Qt_6_0);
        payloadStream << message;
    }

    // prefix with size
    QByteArray frame;
    {
        QDataStream frameStream(&frame, QIODevice::WriteOnly);
        frameStream.setVersion(QDataStream::Qt_6_0);
        quint32 size = static_cast<quint32>(payload.size());
        frameStream << size;
        frame.append(payload);
    }

    // if connected, send; otherwise queue
    if (clientSocket->state() == QAbstractSocket::ConnectedState)
    {
        qint64 written = clientSocket->write(frame);
        clientSocket->flush();
        qDebug() << myId << "sent" << written << "bytes to" << nextPeerId << "(hop=" << message["HopCount"].toInt() << ")";
    }
    else
    {
        sendQueue.append(frame);
        qDebug() << myId << "queued message (next peer not connected yet)";
    }
}

// handle new incoming connection
void ChatWindow::onNewConnection()
{
    incomingSocket = server->nextPendingConnection();
    qDebug() << myId << "accepted connection from previous peer";

    connect(incomingSocket, &QTcpSocket::readyRead, this, &ChatWindow::receiveMessage);
}

// receive readyRead data from previous peer
void ChatWindow::receiveMessage()
{
    if (!incomingSocket)
        return;

    QDataStream in(incomingSocket);
    in.setVersion(QDataStream::Qt_6_0);

    while (true)
    {
        // Need at least 4 bytes for the length prefix
        if (incomingSocket->bytesAvailable() < (int)sizeof(quint32))
            break;

        // Peek header (4 bytes) to get payload size
        QByteArray header = incomingSocket->peek(sizeof(quint32));
        if (header.size() < (int)sizeof(quint32))
            break;

        QDataStream headerStream(header);
        headerStream.setVersion(QDataStream::Qt_6_0);
        quint32 blockSize = 0;
        headerStream >> blockSize;

        // Wait for entire frame (size prefix + payload)
        if (incomingSocket->bytesAvailable() < (int)sizeof(quint32) + blockSize)
            break;

        // Consume size prefix
        incomingSocket->read((qint64)sizeof(quint32));

        // Read payload
        QByteArray payload;
        payload.resize(blockSize);
        qint64 got = incomingSocket->read(payload.data(), blockSize);
        if (got != blockSize)
        {
            qDebug() << myId << "failed to read payload (got" << got << "expected" << blockSize << ")";
            break;
        }

        // Deserialize payload into QVariantMap
        QVariantMap message;
        {
            QDataStream payloadStream(&payload, QIODevice::ReadOnly);
            payloadStream.setVersion(QDataStream::Qt_6_0);
            payloadStream >> message;
        }

        if (message.isEmpty())
        {
            qDebug() << myId << "received empty/invalid message payload";
            continue;
        }

        // read fields
        QString origin = message.value("Origin").toString();
        QString text = message.value("ChatText").toString();
        int hop = message.value("HopCount").toInt();

        // display the message at this peer (every peer shows it)
        chatLog->append(origin + ": " + text);
        qDebug() << myId << "displayed message from" << origin << "(hop=" << hop << ")";

        // forward to next peer only if hop < maxHops (one full cycle)
        if (hop < maxHops)
        {
            // increment hop and update meta
            message["HopCount"] = hop + 1;
            message["Destination"] = nextPeerId;
            message["Sequence"] = sequenceNumber++;

            // prepare forward payload with length-prefix framing
            QByteArray fpayload;
            {
                QDataStream fpayloadStream(&fpayload, QIODevice::WriteOnly);
                fpayloadStream.setVersion(QDataStream::Qt_6_0);
                fpayloadStream << message;
            }

            QByteArray fframe;
            {
                QDataStream fframeStream(&fframe, QIODevice::WriteOnly);
                fframeStream.setVersion(QDataStream::Qt_6_0);
                quint32 fsize = static_cast<quint32>(fpayload.size());
                fframeStream << fsize;
                fframe.append(fpayload);
            }

            // send or queue if not connected
            if (clientSocket->state() == QAbstractSocket::ConnectedState)
            {
                qint64 written = clientSocket->write(fframe);
                clientSocket->flush();
                qDebug() << myId << "forwarded message to" << nextPeerId
                         << "(newHop=" << message["HopCount"].toInt() << ", bytes=" << written << ")";
            }
            else
            {
                sendQueue.append(fframe);
                qDebug() << myId << "queued forward (next peer not connected yet)";
                // attempt to connect immediately
                tryConnect();
            }
        }
        else
        {
            qDebug() << myId << "stopping forwarding (max hops reached)";
        }
    }
}

// tryConnect: attempt to connect to next peer, called periodically
void ChatWindow::tryConnect()
{
    if (clientSocket->state() == QAbstractSocket::ConnectedState)
    {
        // already connected
        if (retryTimer->isActive())
            retryTimer->stop();
        return;
    }

    // reset/abort then try
    clientSocket->abort();
    clientSocket->connectToHost(QHostAddress::LocalHost, nextPeerPort);
}

// onClientConnected: flush queued messages when connected
void ChatWindow::onClientConnected()
{
    qDebug() << myId << "connected to next peer" << nextPeerId;

    // flush queued frames
    while (!sendQueue.isEmpty())
    {
        QByteArray frame = sendQueue.takeFirst();
        qint64 w = clientSocket->write(frame);
        clientSocket->flush();
        qDebug() << myId << "flushed queued frame (" << w << " bytes) to" << nextPeerId;
    }

    if (retryTimer->isActive())
        retryTimer->stop();
}
