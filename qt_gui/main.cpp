#include "personnel_gui.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");
    MainWindow window;
    window.show();
    return app.exec();
}
