#include "PDBExplorer.h"
#include <QtWidgets/QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    PDBExplorer w;

    a.setStyle(QStyleFactory::create("Fusion"));
    w.show();

    return a.exec();
}
