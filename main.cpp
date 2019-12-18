#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(HLS);

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icon.png"));

    MainWindow window;
    window.setMinimumSize(1400, 700);
    window.setWindowTitle("Утилита HLS");
    window.showMaximized();
    return app.exec();
}
