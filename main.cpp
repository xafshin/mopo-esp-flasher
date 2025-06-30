#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QFile>

int main (int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFont font = QApplication::font();
    font.setPointSizeF(font.pointSizeF() * 1.5);  // 150%
    a.setFont(font);

    // Check for ESPTOOL
    if (!QFile(QDir::toNativeSeparators(QDir::currentPath() + "/" + "esptool.exe")).exists()) {
        QMessageBox::critical(
            nullptr,
            "Error",
            "This program need \"esptool.exe\". Please place esptool in the working directory");
        return 1;
    }

    MainWindow   w;
    w.show();
    return a.exec();
}
