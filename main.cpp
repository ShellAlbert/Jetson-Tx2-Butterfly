#include "zmainui.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ZMainUI w;
    w.showFullScreen();

    return app.exec();
}
