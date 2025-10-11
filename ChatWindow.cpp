#include "ChatWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
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

    // Peer selection layout
    QHBoxLayout *peerLayout = new QHBoxLayout();
    peerLabel = new QLabel("Send to:", this);
    peerSelector = new QComboBox(this);
    peerSelector->addItem("(Select peer)"); // placeholder
    peerLayout->addWidget(peerLabel);
    peerLayout->addWidget(peerSelector);
    peerLayout->addStretch(); // push to left

    // Input layout
    QHBoxLayout *inputLayout = new QHBoxLayout();
    input = new QLineEdit(this);
    sendButton = new QPushButton("Send", this);
    inputLayout->addWidget(input);
    inputLayout->addWidget(sendButton);

    layout->addWidget(chatLog);
    layout->addLayout(peerLayout);
    layout->addLayout(inputLayout);

    connect(sendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    connect(input, &QLineEdit::returnPressed, this, &ChatWindow::sendMessage);

    // Add next peer to dropdown initially
    addKnownPeer(nextPeerId);

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

    // Check if a peer is selected
    QString selectedPeer = peerSelector->currentText();
    if (selectedPeer == "(Select peer)" || selectedPeer.isEmpty())
    {
        chatLog->append("Error: Please select a peer to send message to");
        return;
    }

    chatLog->append("Me to " + selectedPeer + ": " + msg); // display locally
    input->clear();

    // create message map
    QVariantMap message;
    message["ChatText"] = msg;
    message["Origin"] = myId;
    message["Destination"] = selectedPeer; // Use selected peer as destination
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
        QString messageType = message.value("MessageType").toString();
        QString origin = message.value("Origin").toString();
        QString destination = message.value("Destination").toString();
        QString text = message.value("ChatText").toString();
        int hop = message.value("HopCount").toInt();

        // Add origin to known peers if not already there
        addKnownPeer(origin);

        // Handle different message types
        if (messageType == "PeerAnnouncement")
        {
            // This is a peer announcement - only show if we haven't seen this peer before
            if (!announcedPeers.contains(origin))
            {
                chatLog->append("[System] Peer " + origin + " joined the network");
                announcedPeers.insert(origin);
                qDebug() << myId << "received peer announcement from" << origin << "(first time)";
            }
            else
            {
                qDebug() << myId << "received duplicate peer announcement from" << origin << "(ignoring display)";
            }

            // Forward announcement if it's a broadcast and hasn't completed the ring
            bool shouldForward = (destination == "ALL" && hop < maxHops);

            if (shouldForward)
            {
                // Forward the announcement
                message["HopCount"] = hop + 1;
                message["Sequence"] = sequenceNumber++;

                // prepare forward payload
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

                // send or queue
                if (clientSocket->state() == QAbstractSocket::ConnectedState)
                {
                    qint64 written = clientSocket->write(fframe);
                    clientSocket->flush();
                    qDebug() << myId << "forwarded peer announcement from" << origin;
                }
                else
                {
                    sendQueue.append(fframe);
                    qDebug() << myId << "queued peer announcement forward";
                }
            }
        }
        else
        {
            // Regular chat message
            // Check if this message is for us
            bool isForMe = (destination == myId);

            if (isForMe)
            {
                // This message is for us - display it and DON'T forward
                chatLog->append(origin + " to me: " + text);
                qDebug() << myId << "received message from" << origin << "(final destination reached)";
            }
            else
            {
                // This message is for someone else - display that we're forwarding it
                chatLog->append("[Forwarding] " + origin + " to " + destination + ": " + text);
                qDebug() << myId << "forwarding message from" << origin << "to" << destination << "(hop=" << hop << ")";

                // forward to next peer only if not for us and hop < maxHops
                if (hop < maxHops)
                {
                    // increment hop and update meta
                    message["HopCount"] = hop + 1;
                    message["Destination"] = destination; // Keep original destination
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

    // Send peer announcement to let other peers know we exist
    sendPeerAnnouncement();
}

// Add peer to dropdown if not already there
void ChatWindow::addKnownPeer(const QString &peerId)
{
    // Don't add ourselves
    if (peerId == myId)
        return;

    // Check if peer is already in the dropdown
    for (int i = 0; i < peerSelector->count(); ++i)
    {
        if (peerSelector->itemText(i) == peerId)
            return; // Already exists
    }

    // Add the new peer
    peerSelector->addItem(peerId);
    qDebug() << myId << "added peer" << peerId << "to dropdown";
}

// Send peer announcement to let other peers know we exist
void ChatWindow::sendPeerAnnouncement()
{
    // Create announcement message
    QVariantMap message;
    message["MessageType"] = "PeerAnnouncement";
    message["Origin"] = myId;
    message["Destination"] = "ALL"; // Broadcast to all peers
    message["Sequence"] = sequenceNumber++;
    message["HopCount"] = 1;

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
        qDebug() << myId << "sent peer announcement (" << written << " bytes)";
    }
    else
    {
        sendQueue.append(frame);
        qDebug() << myId << "queued peer announcement (next peer not connected yet)";
    }
}
