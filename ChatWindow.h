#pragma once // include guard

#include <QWidget>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QList>
#include <QByteArray>
#include <QSet>
#include <QMap>
#include <QDateTime>

class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    // Constructor: initialize with ID and listening port for UDP peer-to-peer
    ChatWindow(QString myId, quint16 myPort, QWidget *parent = nullptr);
    ~ChatWindow(); // Destructor

private slots:
    void sendMessage();    // handle sending messages from input
    void receiveMessage(); // process incoming UDP datagrams
    void discoverPeers();  // discover peers on the network

private:
    void addKnownPeer(const QString &peerId, const QHostAddress &address, quint16 port);                     // add peer to dropdown
    void sendUdpMessage(const QVariantMap &message, const QHostAddress &address, quint16 port);              // send UDP message
    void broadcastMessage(const QVariantMap &message);                                                       // broadcast message to all known peers
    void processReceivedMessage(const QVariantMap &message, const QHostAddress &sender, quint16 senderPort); // process received UDP message

private:
    // Peer information
    struct PeerInfo
    {
        QString id;
        QHostAddress address;
        quint16 port;
    };

    QString myId;           // this node's unique ID
    quint16 myPort;         // this node's listening port
    quint32 sequenceNumber; // sequence number of messages sent

    // UDP networking
    QUdpSocket *udpSocket;  // UDP socket for sending/receiving messages
    QTimer *discoveryTimer; // timer for peer discovery

    // Peer management
    QMap<QString, PeerInfo> knownPeers; // map of peer ID to peer information
    QSet<QString> announcedPeers;       // track peers we've already announced

    // GUI components
    QTextEdit *chatLog;      // display chat messages
    QLineEdit *input;        // input field
    QPushButton *sendButton; // send button
    QComboBox *peerSelector; // dropdown to select destination peer
    QLabel *peerLabel;       // label for peer selector
};
