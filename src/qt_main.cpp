#include <QApplication>
#include <QString>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QString port = (argc > 1) ? argv[1] : "/dev/ttyUSB0";

    MainWindow window(port);
    window.show();

    return app.exec();
}
