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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QVariantMap>
#include <QDataStream>
#include <QDebug>

class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    // Constructor: initialize with ID and listening port for UDP peer-to-peer
    ChatWindow(QString myId, quint16 myPort, QWidget *parent = nullptr);
    ~ChatWindow(); // Destructor

private slots:
    void sendMessage();         // handle sending messages from input
    void receiveMessage();      // process incoming UDP datagrams
    void discoverPeers();       // discover peers on the network
    void performAntiEntropy();  // perform anti-entropy with peers
    void checkResendMessages(); // check for message timeouts and resend

private:
    void addKnownPeer(const QString &peerId, const QHostAddress &address, quint16 port);                     // add peer to dropdown
    void sendUdpMessage(const QVariantMap &message, const QHostAddress &address, quint16 port);              // send UDP message
    void broadcastMessage(const QVariantMap &message);                                                       // broadcast message to all known peers
    void processReceivedMessage(const QVariantMap &message, const QHostAddress &sender, quint16 senderPort); // process received UDP message
    void sendReliableMessage(const QVariantMap &message, const QString &targetPeer);                         // send message with reliability tracking
    void sendAck(const QString &messageId, const QString &targetPeer);                                       // send acknowledgment
    void requestMissingMessages(const QString &peerId);                                                      // request missing messages from peer
    void sendVectorClock(const QString &targetPeer);                                                         // send vector clock for anti-entropy
    void processVectorClock(const QVariantMap &message, const QString &fromPeer);                            // process received vector clock
    void sendMissingMessages(const QString &targetPeer, const QMap<QString, quint32> &theirClock);           // send missing messages to peer

private:
    // Peer information
    struct PeerInfo
    {
        QString id;
        QHostAddress address;
        quint16 port;
        QDateTime lastSeen;
    };

    // Message tracking for reliability
    struct PendingMessage
    {
        QVariantMap message;
        QString targetPeer;
        QDateTime sentTime;
        int retryCount;
    };

    // Stored message for anti-entropy
    struct StoredMessage
    {
        QVariantMap message;
        QString origin;
        quint32 sequence;
        QDateTime timestamp;
    };

    QString myId;           // this node's unique ID
    quint16 myPort;         // this node's listening port
    quint32 sequenceNumber; // sequence number of messages sent

    // UDP networking
    QUdpSocket *udpSocket;    // UDP socket for sending/receiving messages
    QTimer *discoveryTimer;   // timer for peer discovery
    QTimer *antiEntropyTimer; // timer for anti-entropy operations
    QTimer *resendTimer;      // timer for checking message resends

    // Peer management
    QMap<QString, PeerInfo> knownPeers; // map of peer ID to peer information
    QSet<QString> announcedPeers;       // track peers we've already announced

    // Message reliability and tracking
    QMap<QString, PendingMessage> pendingMessages; // messages waiting for ACK (messageId -> message)
    QMap<QString, StoredMessage> messageStore;     // all messages we've seen (messageId -> message)
    QMap<QString, quint32> vectorClock;            // vector clock: peerId -> highest sequence seen
    QSet<QString> receivedAcks;                    // track received acknowledgments

    // GUI components
    QTextEdit *chatLog;      // display chat messages
    QLineEdit *input;        // input field
    QPushButton *sendButton; // send button
    QComboBox *peerSelector; // dropdown to select destination peer
    QLabel *peerLabel;       // label for peer selector
};
