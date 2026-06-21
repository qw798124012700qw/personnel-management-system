#include "personnel_gui.h"

#include <QApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>

int main(int argc, char *argv[]) {
    // 开启高 DPI 缩放，使界面在高分屏上更清晰、尺寸更合理(Qt 5.6+)。
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");
    // 供 QSettings 记忆列宽/排序状态使用（确定配置存储位置）。
    QApplication::setOrganizationName("PMS");
    QApplication::setApplicationName("PersonnelManagementSystem");

    // 简易身份选择（课程演示，非生产级鉴权）：管理员可写，只读访客仅可查看。
    // 管理员口令为演示用途、硬编码；真实系统应使用安全的账号体系。
    bool ok = false;
    const QStringList roles{"管理员", "只读访客"};
    const QString role =
        QInputDialog::getItem(nullptr, "登录", "请选择身份：", roles, 0, false, &ok);
    if (!ok) {
        return 0; // 取消登录则退出
    }
    bool readOnly = (role != "管理员");
    if (!readOnly) {
        const QString pw =
            QInputDialog::getText(nullptr, "管理员登录", "请输入口令（演示口令：admin123）：",
                                  QLineEdit::Password, "", &ok);
        if (!ok) {
            return 0;
        }
        if (pw != "admin123") {
            QMessageBox::information(nullptr, "登录", "口令错误，以只读访客身份进入。");
            readOnly = true;
        }
    }

    MainWindow window(readOnly);
    window.show();
    return app.exec();
}
