#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QStyleFactory>

/**
 * @brief check_deps This function will look for the input list of files in the working directory. If a file was missing,
 * a message will be shown and it will return false.
 * @param deps A string list of file names to check.
 * @return true if all files were present.
 */
bool check_deps(QStringList deps)
{
    for (const auto &element : deps) {
        if (!QFile(QDir::toNativeSeparators(QDir::currentPath() + "/" + element)).exists()) {
            QMessageBox::critical(
                nullptr,
                "Error",
                "This program need \"" + element + "\". Please place this file in the working directory");
            return false;
        }
    }
    return true;
}

int main (int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Set Fusion as the global style
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Scale everything bigger.
    QFont font = QApplication::font();
    font.setPointSizeF(font.pointSizeF() * 1.5);  // 150%
    a.setFont(font);

    // Check for the required files
    QStringList deps;

    deps << "esptool.exe" << "espsecure.exe" << "espefuse.exe";
    // Exit if any were missing.
    if (!check_deps(deps)) {
        return 1;
    }

    MainWindow   w;
    w.show();
    return a.exec();
}
