#include <QApplication>
#include "ChatWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv); // Create Qt application

    ChatWindow window;                   // Create main chat window
    window.setWindowTitle("SimpleChat"); // Set window title
    window.resize(400, 300);             // Set initial window size
    window.show();                       // Show the window

    return app.exec(); // Enter Qt event loop
}
