#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void add_binary_to_table(QString offset, QString path);
    void ui_running();
    void ui_finnished();

private slots:
    void on_add_file_btn_clicked();

    void on_delete_btn_clicked();

    void on_browse_btn_clicked();

    void scanner_handle();

    void on_erase_btn_clicked();

    void esptool_stdout();
    void esptool_stderror();
    void esp_secure_stdout();
    void esp_secure_stderror();
    void esp_fuse_stdout();
    void esp_fuse_stderror();
    void esptool_exit(int code);
    void esp_secure_exit(int code);
    void esp_fuse_exit(int code);

    void on_flash_btn_clicked();


    void on_stop_btn_clicked();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;

    void dropEvent(QDropEvent *event) override;

private:
    struct binaries_list
    {
        QString offset;
        QString path;
    };
    struct efuses_orders
    {
        QString block;
        QString value;
    };

    Ui::MainWindow *ui;
    QTimer *scanner_timer = nullptr;
    QProcess *esp_tool_process = nullptr;
    QProcess *esp_efuse_process = nullptr;
    QProcess *esp_secure_process = nullptr;
    QString esp_tool_path;
    void run_esp_tool(QStringList args);
    void run_esp_fuse(QStringList args);
    void run_esp_secure(QStringList args);
    QVector<binaries_list> get_table_binaries();
    void run_esp_tool_flash(QVector<binaries_list> binaries);
    void burn_efuses(QString command, QVector<efuses_orders> orders);
    bool automatic_mode = false;
};
#endif // MAINWINDOW_H
