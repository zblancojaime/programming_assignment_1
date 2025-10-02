#include <QApplication>
#include "ChatWindow.h"
#include <QString>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("SimpleChat Ring Instance");
    parser.addHelpOption();

    // Define required command-line arguments
    QCommandLineOption myIdOption(QStringList() << "i" << "id", "This instance's ID", "id");
    QCommandLineOption myPortOption(QStringList() << "p" << "port", "This instance's listening port", "port");
    QCommandLineOption nextIdOption(QStringList() << "n" << "next-id", "Next peer ID", "nextId");
    QCommandLineOption nextPortOption(QStringList() << "q" << "next-port", "Next peer port", "nextPort");

    parser.addOption(myIdOption);
    parser.addOption(myPortOption);
    parser.addOption(nextIdOption);
    parser.addOption(nextPortOption);

    parser.process(app);

    // Read values from command line
    QString myId = parser.value(myIdOption);                        // Unique ID for this instance
    quint16 myPort = parser.value(myPortOption).toUShort();         // Listening port
    QString nextPeerId = parser.value(nextIdOption);                // Next peer's ID in the ring
    quint16 nextPeerPort = parser.value(nextPortOption).toUShort(); // Next peer's port

    // Create the main chat window
    ChatWindow window(myId, myPort, nextPeerId, nextPeerPort);
    window.setWindowTitle("SimpleChat - " + myId); // show ID in title
    window.resize(400, 300);                       // default size
    window.show();                                 // show the GUI

    return app.exec(); // enter Qt event loop
}
