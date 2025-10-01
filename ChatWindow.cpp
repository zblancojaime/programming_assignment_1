#include "ChatWindow.h"

// Constructor: sets up GUI elements
ChatWindow::ChatWindow(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this); // Layout for vertical stacking

    chatLog = new QTextEdit(this); // Chat display area
    chatLog->setReadOnly(true);    // Users cannot edit chat log

    input = new QLineEdit(this);                             // Text input field
    QPushButton *sendButton = new QPushButton("Send", this); // Send button

    // Add widgets to layout
    layout->addWidget(chatLog);
    layout->addWidget(input);
    layout->addWidget(sendButton);

    // Connect button click to sendMessage slot
    connect(sendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);
}

// Slot: handles sending messages
void ChatWindow::sendMessage()
{
    QString msg = input->text(); // Get text from input
    if (!msg.isEmpty())
    {
        chatLog->append("Me: " + msg); // Append message to chat log
        input->clear();                // Clear input field
        // TODO: Send message via network (TCP)
    }
}
