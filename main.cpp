#include <QApplication>
#include "ChatWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Example IDs and ports for testing a single instance
    ChatWindow window("1", 9001, "2", 9002);
    window.setWindowTitle("SimpleChat");
    window.resize(400, 300);
    window.show();

    return app.exec();
}
