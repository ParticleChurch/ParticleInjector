#include "mainwindow.hpp"

#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    /*if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole())
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
    */
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
