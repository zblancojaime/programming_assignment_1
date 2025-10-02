#pragma once // include guard

#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QList>
#include <QByteArray>

class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    // Constructor: initialize with ID, listening port, next peer info, parent widget
    ChatWindow(QString myId, quint16 myPort, QString nextPeerId, quint16 nextPeerPort, QWidget *parent = nullptr);
    ~ChatWindow(); // Destructor

private slots:
    void sendMessage();       // handle sending messages from input
    void onNewConnection();   // handle incoming connection from previous peer
    void receiveMessage();    // process data from previous peer
    void tryConnect();        // attempt connection to next peer periodically
    void onClientConnected(); // handle next peer connection

private:
    QString myId;           // this node's unique ID
    quint16 myPort;         // this node's listening port
    QString nextPeerId;     // next peer's ID in ring
    quint16 nextPeerPort;   // next peer's listening port
    quint32 sequenceNumber; // sequence number of messages sent
    int maxHops;            // maximum hops to avoid infinite forwarding

    QTcpServer *server;         // server to accept connections from previous peer
    QTcpSocket *incomingSocket; // connected socket from previous peer
    QTcpSocket *clientSocket;   // socket to next peer
    QTimer *retryTimer;         // timer to retry connection

    QTextEdit *chatLog;      // display chat messages
    QLineEdit *input;        // input field
    QPushButton *sendButton; // send button

    QList<QByteArray> sendQueue; // queue frames if next peer not connected

    // track if we already showed connection error to next peer
    bool nextPeerConnectionErrorShown;
};
