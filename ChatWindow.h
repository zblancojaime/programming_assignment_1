#pragma once // include guard

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QVariantMap>

class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    // Constructor: id, listening port, next peer id & port
    ChatWindow(QString myId, quint16 myPort, QString nextPeerId, quint16 nextPeerPort, QWidget *parent = nullptr);

    // Destructor
    ~ChatWindow();

private slots:
    void sendMessage();       // send typed message
    void receiveMessage();    // receive from previous peer
    void tryConnect();        // try connecting to next peer
    void onClientConnected(); // flush queued messages when connected
    void onNewConnection();   // handle incoming connection

private:
    QString myId;                // this peer's ID
    quint16 myPort;              // listening port
    QString nextPeerId;          // next peer ID
    quint16 nextPeerPort;        // next peer port
    int sequenceNumber;          // incrementing message sequence
    int maxHops;                 // max hops before stopping
    QTextEdit *chatLog;          // display chat messages
    QLineEdit *input;            // input field
    QPushButton *sendButton;     // send button
    QTcpServer *server;          // listens for previous peer
    QTcpSocket *incomingSocket;  // connection from previous peer
    QTcpSocket *clientSocket;    // connection to next peer
    QTimer *retryTimer;          // periodic retry for client
    QList<QByteArray> sendQueue; // queue outgoing messages
};
