#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

// Main chat window class
class ChatWindow : public QWidget
{
Q_OBJECT // Enables Qt signals and slots

    public : explicit ChatWindow(QWidget *parent = nullptr); // Constructor

private slots:
    void sendMessage(); // Called when the send button is clicked

private:
    QTextEdit *chatLog; // Area that displays chat messages
    QLineEdit *input;   // Input field for typing messages
};

#endif // CHATWINDOW_H
