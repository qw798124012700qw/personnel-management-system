#include "personnel_gui.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    // 开启高 DPI 缩放，使界面在高分屏上更清晰、尺寸更合理(Qt 5.6+)。
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");
    MainWindow window;
    window.show();
    return app.exec();
}
