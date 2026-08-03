#include "mainwindow.h"

#include <QApplication>

// point d'entree du programme, rien de special ici
// on cree juste la fenetre principale et on lance la boucle d'evenements de qt
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return QApplication::exec();
}
