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
    void esptool_exit(int code);

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

    Ui::MainWindow *ui;
    QTimer *scanner_timer = nullptr;
    QProcess *esp_tool_process = nullptr;
    QString esp_tool_path;
    void run_esp_tool(QStringList args);
    QVector<binaries_list> get_table_binaries();
    void run_esp_tool_flash(QVector<binaries_list> binaries);
    bool automatic_mode = false;
};
#endif // MAINWINDOW_H
