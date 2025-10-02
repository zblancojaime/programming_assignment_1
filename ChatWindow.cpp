#include "ChatWindow.h"
#include <QVBoxLayout>
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
      chatLog(new QTextEdit(this)),
      input(new QLineEdit(this)),
      sendButton(new QPushButton("Send", this)),
      server(new QTcpServer(this)),
      incomingSocket(nullptr),
      clientSocket(new QTcpSocket(this)),
      retryTimer(new QTimer(this))
{
    // Layout
    QVBoxLayout *layout = new QVBoxLayout(this);
    chatLog->setReadOnly(true);
    layout->addWidget(chatLog);
    layout->addWidget(input);
    layout->addWidget(sendButton);

    connect(sendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    connect(input, &QLineEdit::returnPressed, this, &ChatWindow::sendMessage);

    // Server setup
    connect(server, &QTcpServer::newConnection, this, &ChatWindow::onNewConnection);

    if (!server->listen(QHostAddress::LocalHost, myPort))
        qDebug() << myId << "server failed to start on port" << myPort << ":" << server->errorString();
    else
        qDebug() << myId << "listening on port" << myPort;

    // Client socket setup
    connect(clientSocket, &QTcpSocket::connected, this, &ChatWindow::onClientConnected);

    // Log socket errors
    connect(clientSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, [this](QAbstractSocket::SocketError)
            { qDebug() << myId << "client socket error:" << clientSocket->errorString(); });

    // Retry timer for client connect
    retryTimer->setInterval(1000);
    connect(retryTimer, &QTimer::timeout, this, &ChatWindow::tryConnect);
    retryTimer->start();

    tryConnect(); // initial attempt
}

// Destructor
ChatWindow::~ChatWindow() {}

// Send button clicked
void ChatWindow::sendMessage()
{
    QString msg = input->text().trimmed();
    if (msg.isEmpty())
        return;

    chatLog->append("Me: " + msg);
    input->clear();

    QVariantMap message;
    message["ChatText"] = msg;
    message["Origin"] = myId;
    message["Destination"] = nextPeerId;
    message["Sequence"] = sequenceNumber++;
    message["HopCount"] = 1;

    QByteArray payload;
    {
        QDataStream payloadStream(&payload, QIODevice::WriteOnly);
        payloadStream.setVersion(QDataStream::Qt_6_0);
        payloadStream << message;
    }

    QByteArray frame;
    {
        QDataStream frameStream(&frame, QIODevice::WriteOnly);
        frameStream.setVersion(QDataStream::Qt_6_0);
        quint32 size = static_cast<quint32>(payload.size());
        frameStream << size;
        frame.append(payload);
    }

    if (clientSocket->state() == QAbstractSocket::ConnectedState)
    {
        qint64 written = clientSocket->write(frame);
        clientSocket->flush();
        qDebug() << myId << "sent" << written << "bytes to" << nextPeerId
                 << "(hop=" << message["HopCount"].toInt() << ")";
    }
    else
    {
        sendQueue.append(frame);
        qDebug() << myId << "could not send message: next peer not connected";
    }
}

// New incoming connection
void ChatWindow::onNewConnection()
{
    incomingSocket = server->nextPendingConnection();
    qDebug() << myId << "accepted connection from previous peer";
    connect(incomingSocket, &QTcpSocket::readyRead, this, &ChatWindow::receiveMessage);
}

// Receive from previous peer
void ChatWindow::receiveMessage()
{
    if (!incomingSocket)
        return;

    QDataStream in(incomingSocket);
    in.setVersion(QDataStream::Qt_6_0);

    while (true)
    {
        if (incomingSocket->bytesAvailable() < (int)sizeof(quint32))
            break;

        QByteArray header = incomingSocket->peek(sizeof(quint32));
        if (header.size() < (int)sizeof(quint32))
            break;

        QDataStream headerStream(header);
        headerStream.setVersion(QDataStream::Qt_6_0);
        quint32 blockSize = 0;
        headerStream >> blockSize;

        if (incomingSocket->bytesAvailable() < (int)sizeof(quint32) + blockSize)
            break;

        incomingSocket->read((qint64)sizeof(quint32));

        QByteArray payload;
        payload.resize(blockSize);
        qint64 got = incomingSocket->read(payload.data(), blockSize);
        if (got != blockSize)
            break;

        QVariantMap message;
        QDataStream payloadStream(&payload, QIODevice::ReadOnly);
        payloadStream.setVersion(QDataStream::Qt_6_0);
        payloadStream >> message;

        QString origin = message.value("Origin").toString();
        QString text = message.value("ChatText").toString();
        int hop = message.value("HopCount").toInt();

        chatLog->append(origin + ": " + text);
        qDebug() << myId << "displayed message from" << origin << "(hop=" << hop << ")";

        if (hop < maxHops)
        {
            message["HopCount"] = hop + 1;
            message["Destination"] = nextPeerId;
            message["Sequence"] = sequenceNumber++;

            QByteArray fpayload;
            QDataStream fpayloadStream(&fpayload, QIODevice::WriteOnly);
            fpayloadStream.setVersion(QDataStream::Qt_6_0);
            fpayloadStream << message;

            QByteArray fframe;
            QDataStream fframeStream(&fframe, QIODevice::WriteOnly);
            fframeStream.setVersion(QDataStream::Qt_6_0);
            quint32 fsize = static_cast<quint32>(fpayload.size());
            fframeStream << fsize;
            fframe.append(fpayload);

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
                qDebug() << myId << "queued forward: next peer not connected";
                tryConnect();
            }
        }
    }
}

// Retry client connect
void ChatWindow::tryConnect()
{
    if (clientSocket->state() == QAbstractSocket::ConnectedState)
    {
        if (retryTimer->isActive())
            retryTimer->stop();
        return;
    }

    clientSocket->abort();
    clientSocket->connectToHost(QHostAddress::LocalHost, nextPeerPort);
}

// Client connected: flush queue
void ChatWindow::onClientConnected()
{
    qDebug() << myId << "connected to next peer" << nextPeerId;

    while (!sendQueue.isEmpty())
    {
        QByteArray frame = sendQueue.takeFirst();
        clientSocket->write(frame);
        clientSocket->flush();
    }

    if (retryTimer->isActive())
        retryTimer->stop();
}
