#include <QApplication>
#include <QIcon>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("IPsec Client");
    app.setOrganizationName("ipsec-client");
    app.setWindowIcon(QIcon::fromTheme("ipsec-client"));

    MainWindow window;
    window.show();
    return app.exec();
}
