#include "ChatWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QVariantMap>
#include <QDataStream>
#include <QHostAddress>
#include <QDebug>

ChatWindow::ChatWindow(QString id, quint16 port, QWidget *parent)
    : QWidget(parent),
      myId(id),
      myPort(port),
      sequenceNumber(1)
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
    peerSelector->addItem("BROADCAST");     // broadcast option
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

    // Set up UDP socket for sending and receiving messages
    udpSocket = new QUdpSocket(this);

    if (!udpSocket->bind(QHostAddress::LocalHost, myPort))
    {
        qDebug() << myId << "UDP socket failed to bind to port" << myPort << ":" << udpSocket->errorString();
    }
    else
    {
        qDebug() << myId << "UDP socket listening on port" << myPort;
    }

    // Connect UDP socket to receive messages
    connect(udpSocket, &QUdpSocket::readyRead, this, &ChatWindow::receiveMessage);

    // Set up discovery timer to periodically discover peers
    discoveryTimer = new QTimer(this);
    discoveryTimer->setInterval(5000); // every 5 seconds
    connect(discoveryTimer, &QTimer::timeout, this, &ChatWindow::discoverPeers);
    discoveryTimer->start();

    // Set up anti-entropy timer
    antiEntropyTimer = new QTimer(this);
    antiEntropyTimer->setInterval(10000); // every 10 seconds
    connect(antiEntropyTimer, &QTimer::timeout, this, &ChatWindow::performAntiEntropy);
    antiEntropyTimer->start();

    // Set up resend timer for reliable messaging
    resendTimer = new QTimer(this);
    resendTimer->setInterval(1500); // every 1.5 seconds
    connect(resendTimer, &QTimer::timeout, this, &ChatWindow::checkResendMessages);
    resendTimer->start();

    // Initialize vector clock for ourselves
    vectorClock[myId] = 0;

    // Initial peer discovery
    discoverPeers();
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

    input->clear();

    // Update our vector clock
    vectorClock[myId] = sequenceNumber;

    // Create message map
    QVariantMap message;
    message["MessageType"] = "Chat";
    message["ChatText"] = msg;
    message["Origin"] = myId;
    message["Sequence"] = sequenceNumber++;
    QString messageId = myId + "_" + QString::number(message["Sequence"].toUInt());
    message["MessageId"] = messageId;

    // Store the message in our message store
    StoredMessage stored;
    stored.message = message;
    stored.origin = myId;
    stored.sequence = message["Sequence"].toUInt();
    stored.timestamp = QDateTime::currentDateTime();
    messageStore[messageId] = stored;

    if (selectedPeer == "BROADCAST")
    {
        message["Destination"] = -1; // -1 indicates broadcast
        chatLog->append("Me (broadcast): " + msg);

        // Send broadcast message to all known peers (non-reliable for broadcast)
        for (auto it = knownPeers.begin(); it != knownPeers.end(); ++it)
        {
            const PeerInfo &peer = it.value();
            sendUdpMessage(message, peer.address, peer.port);
        }
        qDebug() << myId << "broadcasted message to" << knownPeers.size() << "peers";
    }
    else
    {
        message["Destination"] = selectedPeer;
        chatLog->append("Me to " + selectedPeer + ": " + msg);

        // Send reliable message to the selected peer
        if (knownPeers.contains(selectedPeer))
        {
            sendReliableMessage(message, selectedPeer);
        }
        else
        {
            chatLog->append("Error: Peer " + selectedPeer + " not found");
        }
    }
}

// Receive UDP messages
void ChatWindow::receiveMessage()
{
    while (udpSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(udpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        // Deserialize the message
        QVariantMap message;
        QDataStream stream(&datagram, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_0);
        stream >> message;

        if (message.isEmpty())
        {
            qDebug() << myId << "received empty/invalid message";
            continue;
        }

        processReceivedMessage(message, sender, senderPort);
    }
}

// Process received message
void ChatWindow::processReceivedMessage(const QVariantMap &message, const QHostAddress &sender, quint16 senderPort)
{
    if (message.isEmpty())
    {
        qDebug() << myId << "received empty/invalid message";
        return;
    }

    // Read message fields
    QString messageType = message.value("MessageType").toString();
    QString origin = message.value("Origin").toString();
    QVariant destVar = message.value("Destination");
    QString chatText = message.value("ChatText").toString();
    quint32 sequence = message.value("Sequence").toUInt();

    // Add origin to known peers if not already there
    addKnownPeer(origin, sender, senderPort);

    // Handle different message types
    if (messageType == "Discovery")
    {
        // Only log if this is a new peer
        if (!knownPeers.contains(origin))
        {
            qDebug() << myId << "received discovery message from new peer" << origin;
        }
        // Respond with our peer info
        QVariantMap response;
        response["MessageType"] = "DiscoveryResponse";
        response["Origin"] = myId;
        response["Destination"] = origin;
        response["Sequence"] = sequenceNumber++;
        sendUdpMessage(response, sender, senderPort);
    }
    else if (messageType == "DiscoveryResponse")
    {
        // Only log if this is a new peer
        if (!knownPeers.contains(origin))
        {
            qDebug() << myId << "received discovery response from new peer" << origin;
        }
        // Peer info already added above
    }
    else if (messageType == "Ack")
    {
        QString messageId = message.value("AckFor").toString();
        if (pendingMessages.contains(messageId))
        {
            pendingMessages.remove(messageId);
            qDebug() << myId << "received ACK for message" << messageId;
        }
    }
    else if (messageType == "VectorClock")
    {
        processVectorClock(message, origin);
    }
    else if (messageType == "MessageRequest")
    {
        QStringList requestedIds = message.value("RequestedMessages").toStringList();
        for (const QString &msgId : requestedIds)
        {
            if (messageStore.contains(msgId))
            {
                QVariantMap storedMsg = messageStore[msgId].message;
                sendUdpMessage(storedMsg, sender, senderPort);
            }
        }
    }
    else if (messageType == "Chat")
    {
        QString messageId = message.value("MessageId").toString();

        // Check if this is a duplicate message
        bool isNewMessage = !messageStore.contains(messageId);

        // Only process new messages
        if (isNewMessage)
        {
            // Store the message
            StoredMessage stored;
            stored.message = message;
            stored.origin = origin;
            stored.sequence = sequence;
            stored.timestamp = QDateTime::currentDateTime();
            messageStore[messageId] = stored;

            // Update vector clock
            if (!vectorClock.contains(origin) || vectorClock[origin] < sequence)
            {
                vectorClock[origin] = sequence;
            }

            // Handle chat message display
            bool isBroadcast = (destVar.toInt() == -1);
            QString destination = destVar.toString();

            if (isBroadcast)
            {
                // Broadcast message - display it
                chatLog->append(origin + " (broadcast): " + chatText);
                qDebug() << myId << "received broadcast message from" << origin;
            }
            else if (destination == myId)
            {
                // Direct message to us
                chatLog->append(origin + " to me: " + chatText);
                qDebug() << myId << "received direct message from" << origin;
            }
            else
            {
                // Message not for us - ignore (no forwarding in UDP broadcast model)
                qDebug() << myId << "received message for" << destination << "from" << origin << "(ignoring)";
            }
        }
        else
        {
            // This is a duplicate message - just log it but don't display
            qDebug() << myId << "received duplicate message" << messageId << "from" << origin << "(ignoring)";
        }

        // Send acknowledgment (even for duplicates to stop retransmission)
        sendAck(messageId, origin);
    }
}

// Discover peers on the network
void ChatWindow::discoverPeers()
{
    static int discoveryCount = 0;
    discoveryCount++;

    // Only log every 10th discovery to reduce spam
    if (discoveryCount % 10 == 1)
    {
        qDebug() << myId << "performing peer discovery (count:" << discoveryCount << ")";
    }

    // Create discovery message
    QVariantMap message;
    message["MessageType"] = "Discovery";
    message["Origin"] = myId;
    message["Destination"] = -1; // Broadcast
    message["Sequence"] = sequenceNumber++;

    // Broadcast discovery message to common port range
    for (quint16 port = 12340; port <= 12350; ++port)
    {
        if (port != myPort) // Don't send to ourselves
        {
            sendUdpMessage(message, QHostAddress::LocalHost, port);
        }
    }
}

// Add peer to known peers and dropdown
void ChatWindow::addKnownPeer(const QString &peerId, const QHostAddress &address, quint16 port)
{
    // Don't add ourselves
    if (peerId == myId)
        return;

    // Update or add peer info
    PeerInfo &peer = knownPeers[peerId];
    peer.id = peerId;
    peer.address = address;
    peer.port = port;
    peer.lastSeen = QDateTime::currentDateTime();

    // Check if peer is already in the dropdown
    bool foundInDropdown = false;
    for (int i = 0; i < peerSelector->count(); ++i)
    {
        if (peerSelector->itemText(i) == peerId)
        {
            foundInDropdown = true;
            break;
        }
    }

    if (!foundInDropdown)
    {
        // Add the new peer to dropdown
        peerSelector->addItem(peerId);
        qDebug() << myId << "NEW PEER DISCOVERED:" << peerId;

        // Show peer joined message only once
        if (!announcedPeers.contains(peerId))
        {
            chatLog->append("[System] Peer " + peerId + " joined the network");
            announcedPeers.insert(peerId);
        }
    }
}

// Send UDP message to specific address and port
void ChatWindow::sendUdpMessage(const QVariantMap &message, const QHostAddress &address, quint16 port)
{
    // Serialize the message
    QByteArray datagram;
    QDataStream stream(&datagram, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << message;

    // Send the datagram
    qint64 written = udpSocket->writeDatagram(datagram, address, port);
    if (written == -1)
    {
        // Only log failures for chat messages, not routine heartbeats/discovery
        QString messageType = message.value("MessageType").toString();
        if (messageType == "Chat")
        {
            qDebug() << myId << "failed to send chat message to" << address.toString() << ":" << port;
        }
    }
}

// Broadcast message to all known peers
void ChatWindow::broadcastMessage(const QVariantMap &message)
{
    for (auto it = knownPeers.begin(); it != knownPeers.end(); ++it)
    {
        const PeerInfo &peer = it.value();
        sendUdpMessage(message, peer.address, peer.port);
    }

    // Only log if we have peers to broadcast to
    if (knownPeers.size() > 0)
    {
        qDebug() << myId << "broadcasted message to" << knownPeers.size() << "peers";
    }
}

// Send message with reliability tracking
void ChatWindow::sendReliableMessage(const QVariantMap &message, const QString &targetPeer)
{
    if (!knownPeers.contains(targetPeer))
        return;

    PeerInfo peer = knownPeers[targetPeer];
    QString messageId = message.value("MessageId").toString();

    // Store as pending message for retransmission
    PendingMessage pending;
    pending.message = message;
    pending.targetPeer = targetPeer;
    pending.sentTime = QDateTime::currentDateTime();
    pending.retryCount = 0;
    pendingMessages[messageId] = pending;

    // Send the message
    sendUdpMessage(message, peer.address, peer.port);
    qDebug() << myId << "sent reliable message" << messageId << "to" << targetPeer;
}

// Send acknowledgment
void ChatWindow::sendAck(const QString &messageId, const QString &targetPeer)
{
    if (!knownPeers.contains(targetPeer))
        return;

    PeerInfo peer = knownPeers[targetPeer];

    QVariantMap ack;
    ack["MessageType"] = "Ack";
    ack["Origin"] = myId;
    ack["AckFor"] = messageId;

    sendUdpMessage(ack, peer.address, peer.port);
}

// Check for message timeouts and resend
void ChatWindow::checkResendMessages()
{
    QDateTime now = QDateTime::currentDateTime();
    QStringList toResend;

    for (auto it = pendingMessages.begin(); it != pendingMessages.end(); ++it)
    {
        PendingMessage &pending = it.value();

        // Check if message has timed out (2 seconds)
        if (pending.sentTime.msecsTo(now) > 2000)
        {
            if (pending.retryCount < 3) // Max 3 retries
            {
                toResend.append(it.key());
                pending.retryCount++;
                pending.sentTime = now;
            }
            else
            {
                // Give up after 3 retries
                qDebug() << myId << "giving up on message" << it.key() << "after 3 retries";
                toResend.clear();
                it = pendingMessages.erase(it);
                continue;
            }
        }
        ++it;
    }

    // Resend timed out messages
    for (const QString &messageId : toResend)
    {
        if (pendingMessages.contains(messageId))
        {
            PendingMessage &pending = pendingMessages[messageId];
            if (knownPeers.contains(pending.targetPeer))
            {
                PeerInfo peer = knownPeers[pending.targetPeer];
                sendUdpMessage(pending.message, peer.address, peer.port);
                qDebug() << myId << "resending message" << messageId << "to" << pending.targetPeer
                         << "(attempt" << (pending.retryCount + 1) << ")";
            }
        }
    }
}

// Perform anti-entropy with peers
void ChatWindow::performAntiEntropy()
{
    if (knownPeers.isEmpty())
        return;

    qDebug() << myId << "performing anti-entropy with" << knownPeers.size() << "peers";

    // Send vector clock to all known peers
    for (auto it = knownPeers.begin(); it != knownPeers.end(); ++it)
    {
        sendVectorClock(it.key());
    }
}

// Send vector clock for anti-entropy
void ChatWindow::sendVectorClock(const QString &targetPeer)
{
    if (!knownPeers.contains(targetPeer))
        return;

    PeerInfo peer = knownPeers[targetPeer];

    QVariantMap message;
    message["MessageType"] = "VectorClock";
    message["Origin"] = myId;

    // Convert vector clock to QVariantMap for serialization
    QVariantMap clockMap;
    for (auto it = vectorClock.begin(); it != vectorClock.end(); ++it)
    {
        clockMap[it.key()] = it.value();
    }
    message["VectorClock"] = clockMap;

    sendUdpMessage(message, peer.address, peer.port);
}

// Process received vector clock
void ChatWindow::processVectorClock(const QVariantMap &message, const QString &fromPeer)
{
    QVariantMap theirClockMap = message.value("VectorClock").toMap();
    QMap<QString, quint32> theirClock;

    // Convert QVariantMap back to QMap<QString, quint32>
    for (auto it = theirClockMap.begin(); it != theirClockMap.end(); ++it)
    {
        theirClock[it.key()] = it.value().toUInt();
    }

    // Find messages they're missing
    QStringList missingMessages;
    for (auto it = messageStore.begin(); it != messageStore.end(); ++it)
    {
        const StoredMessage &stored = it.value();
        QString origin = stored.origin;
        quint32 sequence = stored.sequence;

        // If they don't have this origin or have a lower sequence number
        if (!theirClock.contains(origin) || theirClock[origin] < sequence)
        {
            missingMessages.append(it.key());
        }
    }

    if (!missingMessages.isEmpty())
    {
        qDebug() << myId << "peer" << fromPeer << "is missing" << missingMessages.size() << "messages";

        // Send the missing messages
        for (const QString &messageId : missingMessages)
        {
            if (messageStore.contains(messageId))
            {
                QVariantMap storedMsg = messageStore[messageId].message;
                sendReliableMessage(storedMsg, fromPeer);
            }
        }
    }

    // Update our knowledge of what they have
    for (auto it = theirClock.begin(); it != theirClock.end(); ++it)
    {
        QString origin = it.key();
        quint32 theirSequence = it.value();

        // If we're missing messages from this origin, request them
        if (!vectorClock.contains(origin) || vectorClock[origin] < theirSequence)
        {
            requestMissingMessages(fromPeer);
            break;
        }
    }
}

// Request missing messages from peer
void ChatWindow::requestMissingMessages(const QString &peerId)
{
    if (!knownPeers.contains(peerId))
        return;

    // For now, we'll just send our vector clock back to trigger them to send us what we're missing
    // In a more sophisticated implementation, we would specifically request message ranges
    sendVectorClock(peerId);
}