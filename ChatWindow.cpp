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

    // Create message map
    QVariantMap message;
    message["MessageType"] = "Chat";
    message["ChatText"] = msg;
    message["Origin"] = myId;
    message["Sequence"] = sequenceNumber++;

    if (selectedPeer == "BROADCAST")
    {
        message["Destination"] = -1; // -1 indicates broadcast
        chatLog->append("Me (broadcast): " + msg);
        broadcastMessage(message);
    }
    else
    {
        message["Destination"] = selectedPeer;
        chatLog->append("Me to " + selectedPeer + ": " + msg);

        // Send directly to the selected peer
        if (knownPeers.contains(selectedPeer))
        {
            PeerInfo peer = knownPeers[selectedPeer];
            sendUdpMessage(message, peer.address, peer.port);
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
    else if (messageType == "Chat")
    {
        // Handle chat message
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