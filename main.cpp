#include "drawgame.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    DrawGame w;
    w.show();
    return a.exec();
}
