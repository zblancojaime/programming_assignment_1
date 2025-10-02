#include "ChatWindow.h"
#include <QVBoxLayout>
#include <QVariantMap>

ChatWindow::ChatWindow(QString myId, quint16 myPort, QString nextPeerId, quint16 nextPeerPort, QWidget *parent)
    : QWidget(parent), myId(myId), myPort(myPort), nextPeerId(nextPeerId), nextPeerPort(nextPeerPort), sequenceNumber(1)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    chatLog = new QTextEdit(this);
    chatLog->setReadOnly(true);

    input = new QLineEdit(this);
    QPushButton *sendButton = new QPushButton("Send", this);

    layout->addWidget(chatLog);
    layout->addWidget(input);
    layout->addWidget(sendButton);

    connect(sendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);

    // TODO: initialize QTcpSocket and connect signals
}

// Destructor definition (even empty)
ChatWindow::~ChatWindow()
{
    // Qt will delete child widgets automatically
}

void ChatWindow::sendMessage()
{
    QString msg = input->text();
    if (!msg.isEmpty())
    {
        chatLog->append("Me: " + msg);
        input->clear();
        // TODO: send msg to next peer
    }
}

void ChatWindow::onMessageReceived()
{
    // TODO: read message from socket and forward to next peer
}
