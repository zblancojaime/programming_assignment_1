#pragma once // include guard

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTcpSocket>

class ChatWindow : public QWidget
{
Q_OBJECT // required for Qt meta-object features

    public :
    // Constructor with parameters
    ChatWindow(QString myId, quint16 myPort, QString nextPeerId, quint16 nextPeerPort, QWidget *parent = nullptr);

    // Destructor MUST be declared here
    ~ChatWindow();

private slots:
    void sendMessage();       // handle send button click
    void onMessageReceived(); // handle incoming messages

private:
    QTextEdit *chatLog;
    QLineEdit *input;
    QTcpSocket *socket;
    QString myId;
    quint16 myPort;
    QString nextPeerId;
    quint16 nextPeerPort;
    int sequenceNumber;
};
