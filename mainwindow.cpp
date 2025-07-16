#include "mainwindow.h"
#include <QDir>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("MOPO ESP Flasher");
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    QRegularExpression hexPattern("^0x[0-9A-Fa-f]{1,8}$"); // max 8 hex digits
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(hexPattern, this);
    ui->add_offset->setValidator(validator);
    ui->add_path->setAcceptDrops(true);
    ui->tableWidget->setAcceptDrops(true);
    scanner_timer = new QTimer(this);
    connect(scanner_timer, &QTimer::timeout, this, &MainWindow::scanner_handle);
    scanner_timer->setInterval(1000);
    scanner_timer->start();

#ifdef _WIN32
    esp_tool_path = QDir::toNativeSeparators(QDir::currentPath() + "/" + "esptool.exe");
#else
    esp_tool_path = QDir::toNativeSeparators(QDir::currentPath() + "/" + "esptool");
#endif

    esp_tool_process = new QProcess(this);
    esp_tool_process->setProgram(esp_tool_path);
    connect(esp_tool_process, &QProcess::readyReadStandardOutput, this, &MainWindow::esptool_stdout);
    connect(esp_tool_process,
            &QProcess::readyReadStandardError,
            this,
            &MainWindow::esptool_stderror);
    connect(esp_tool_process, &QProcess::finished, this, &MainWindow::esptool_exit);

    esp_secure_process = new QProcess(this);
    esp_secure_process->setProgram(esp_tool_path);
    connect(esp_secure_process, &QProcess::readyReadStandardOutput, this, &MainWindow::esp_secure_stdout);
    connect(esp_secure_process,
            &QProcess::readyReadStandardError,
            this,
            &MainWindow::esp_secure_stderror);
    connect(esp_secure_process, &QProcess::finished, this, &MainWindow::esp_secure_exit);

    esp_efuse_process = new QProcess(this);
    esp_efuse_process->setProgram(esp_tool_path);
    connect(esp_efuse_process, &QProcess::readyReadStandardOutput, this, &MainWindow::esp_fuse_stdout);
    connect(esp_efuse_process,
            &QProcess::readyReadStandardError,
            this,
            &MainWindow::esp_fuse_stderror);
    connect(esp_efuse_process, &QProcess::finished, this, &MainWindow::esp_fuse_exit);

    QFile orders(QDir::currentPath() + "/orders");
    if (orders.exists()) {
        automatic_mode = true;
        ui_running();
        ui_finnished();

        if (!orders.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Cannot open file:" << orders.errorString();
            return;
        }

        QTextStream in(&orders);
        while (!in.atEnd()) {
            QString line = in.readLine();
            QStringList list = line.split(',');
            qDebug() << line;
            if (list.size() != 2)
                break;

            add_binary_to_table(list[0], list[1]);
        }

        orders.close();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::add_binary_to_table(QString offset, QString path)
{
    if ((offset.size() < 3) || (path.size() == 0))
        return;
    if (!QFile(path).exists())
        return;

    int row = ui->tableWidget->rowCount(); // Get the current row count
    ui->tableWidget->insertRow(row);       // Insert a new row at the end

    QTableWidgetItem *item_offset = new QTableWidgetItem(offset);
    ui->tableWidget->setItem(row, 0, item_offset);
    QTableWidgetItem *item_path = new QTableWidgetItem(path);
    ui->tableWidget->setItem(row, 1, item_path);
}

void MainWindow::on_add_file_btn_clicked()
{
    add_binary_to_table(ui->add_offset->text(), ui->add_path->text());
}

void MainWindow::on_delete_btn_clicked()
{
    QList<int> rowsToDelete;

    // Get all selected indexes (cells)
    QModelIndexList selectedIndexes = ui->tableWidget->selectionModel()->selectedIndexes();

    // Extract unique row numbers
    for (const QModelIndex &index : selectedIndexes) {
        int row = index.row();
        if (!rowsToDelete.contains(row)) {
            rowsToDelete.append(row);
        }
    }

    // Sort in descending order so row indices don't shift as we delete
    std::sort(rowsToDelete.begin(), rowsToDelete.end(), std::greater<int>());

    // Delete the rows
    for (int row : rowsToDelete) {
        ui->tableWidget->removeRow(row);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString filePath = urls.first().toLocalFile();
        ui->add_path->setText(filePath);
    }
}

void MainWindow::on_browse_btn_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "Select a file",
                                                    "", // Starting directory ("" = default)
                                                    "All Files (*.*);;Text Files (*.bin)");
    if (!QFile(filePath).exists())
        return;

    ui->add_path->setText(filePath);
}

void MainWindow::scanner_handle()
{
    QString selected = ui->comboBoxPorts->currentText();

    int count = ui->comboBoxPorts->count();

    const auto ports = QSerialPortInfo::availablePorts();
    bool noChange = true;
    if (ports.size() == count - 1) {
        for (int i = 1; i < count; ++i) {
            QString portName = ports[i-1].portName(); // e.g. COM3 or /dev/ttyUSB0
            QString description = ports[i-1].description(); // e.g. "USB Serial Device"
            QString displayText = QString("%1 (%2)").arg(portName, description);

            if (ui->comboBoxPorts->itemText(i) != displayText)
                noChange = false;
        }
    } else {
        noChange = false;
    }

    if (noChange)
        return;

    ui->comboBoxPorts->clear(); // Clear existing items
    ui->comboBoxPorts->addItem("Auto");

    for (const QSerialPortInfo &port : ports) {
        QString portName = port.portName();       // e.g. COM3 or /dev/ttyUSB0
        QString description = port.description(); // e.g. "USB Serial Device"
        QString displayText = QString("%1 (%2)").arg(portName, description);

        ui->comboBoxPorts->addItem(displayText, portName); // Store portName as userData
    }
    ui->comboBoxPorts->setCurrentText(selected);

}

void MainWindow::run_esp_tool(QStringList args)
{
    if (ui->comboBoxPorts->currentText() != "Auto") {
        QString com = ui->comboBoxPorts->currentText().replace('\r', "").split(' ')[0];
        args.push_front(com);
        args.push_front("--port");
    }
    qDebug() << "args" << args;
    esp_tool_process->setArguments(args);
    ui_running();
    esp_tool_process->start();
}

void MainWindow::run_esp_tool_flash(QVector<binaries_list> binaries)
{
    QStringList args;
    args.append("--before");
    args.append("default_reset");
    args.append("--after");
    args.append("hard_reset");
    args.append("write_flash");

    for (const auto &binary : binaries) {
        args.append(binary.offset);
        args.append(binary.path);
    }

    run_esp_tool(args);
}

void MainWindow::esptool_stdout()
{
    QByteArray data = esp_tool_process->readAllStandardOutput().replace('\r', "");
    QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);

    for (const auto &line : lines) {
        if (line.contains("Writing at 0x")) {
            QStringList t1 = line.split('(');
            if (t1.size() == 2) {
                t1 = t1[1].split(')');
                if (t1.size() == 2) {
                    quint16 percent = t1[0].remove(' ').remove('%').toUInt();
                    if (percent != 100)
                        ui->progressBar->setValue(percent);
                }
            }
        }

        qDebug() << line;
    }
}

void MainWindow::esptool_stderror()
{
    QByteArray data = esp_tool_process->readAllStandardError().replace('\r', "");
    QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);

    for (const auto &line : lines) {
        qDebug() << line;
    }
}
void MainWindow::esp_fuse_stdout()
{
    QByteArray data = esp_tool_process->readAllStandardOutput().replace('\r', "");
    QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);

    for (const auto &line : lines) {
        if (line.contains("Writing at 0x")) {
            QStringList t1 = line.split('(');
            if (t1.size() == 2) {
                t1 = t1[1].split(')');
                if (t1.size() == 2) {
                    quint16 percent = t1[0].remove(' ').remove('%').toUInt();
                    if (percent != 100)
                        ui->progressBar->setValue(percent);
                }
            }
        }

        qDebug() << line;
    }
}

void MainWindow::esp_fuse_stderror()
{
    QByteArray data = esp_tool_process->readAllStandardError().replace('\r', "");
    QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);

    for (const auto &line : lines) {
        qDebug() << line;
    }
}
void MainWindow::esp_secure_stdout()
{
    QByteArray data = esp_tool_process->readAllStandardOutput().replace('\r', "");
    QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);

    for (const auto &line : lines) {
        if (line.contains("Writing at 0x")) {
            QStringList t1 = line.split('(');
            if (t1.size() == 2) {
                t1 = t1[1].split(')');
                if (t1.size() == 2) {
                    quint16 percent = t1[0].remove(' ').remove('%').toUInt();
                    if (percent != 100)
                        ui->progressBar->setValue(percent);
                }
            }
        }

        qDebug() << line;
    }
}

void MainWindow::esp_secure_stderror()
{
    QByteArray data = esp_tool_process->readAllStandardError().replace('\r', "");
    QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);

    for (const auto &line : lines) {
        qDebug() << line;
    }
}

void MainWindow::esptool_exit(int code)
{
    qDebug() << "Exit code:" << code;
    if (code == 0) {
        ui->progressBar->setValue(100);
    } else {
        ui->progressBar->setValue(0);
    }
    ui_finnished();
}
void MainWindow::esp_secure_exit(int code)
{
    qDebug() << "Exit code:" << code;
    if (code == 0) {
        ui->progressBar->setValue(100);
    } else {
        ui->progressBar->setValue(0);
    }
    ui_finnished();
}
void MainWindow::esp_fuse_exit(int code)
{
    qDebug() << "Exit code:" << code;
    if (code == 0) {
        ui->progressBar->setValue(100);
    } else {
        ui->progressBar->setValue(0);
    }
    ui_finnished();
}

void MainWindow::ui_running()
{
    ui->progressBar->setValue(0);
    ui->flash_btn->setEnabled(false);
    ui->tableWidget->setEnabled(false);
    ui->erase_btn->setEnabled(false);
    ui->add_file_btn->setEnabled(false);
    ui->browse_btn->setEnabled(false);
    ui->delete_btn->setEnabled(false);
    ui->comboBoxPorts->setEnabled(false);
    ui->add_path->setEnabled(false);
    ui->add_offset->setEnabled(false);
    ui->stop_btn->setEnabled(true);
}
void MainWindow::ui_finnished()
{
    ui->comboBoxPorts->setEnabled(true);
    ui->flash_btn->setEnabled(true);
    ui->erase_btn->setEnabled(true);

    if (!automatic_mode) {
        ui->tableWidget->setEnabled(true);
        ui->add_file_btn->setEnabled(true);
        ui->browse_btn->setEnabled(true);
        ui->delete_btn->setEnabled(true);
        ui->add_path->setEnabled(true);
        ui->add_offset->setEnabled(true);
    }
    ui->stop_btn->setEnabled(false);
}

void MainWindow::on_erase_btn_clicked()
{
    QStringList args;
    args << "erase_flash";
    run_esp_tool(args);
}

void MainWindow::on_flash_btn_clicked()
{
    QVector<binaries_list> binaries = get_table_binaries();

    run_esp_tool_flash(binaries);
}

QVector<MainWindow::binaries_list> MainWindow::get_table_binaries()
{
    QVector<MainWindow::binaries_list> bins;
    int rowCount = ui->tableWidget->rowCount();

    for (int row = 0; row < rowCount; ++row) {
        QTableWidgetItem *item_offset = ui->tableWidget->item(row, 0);
        QTableWidgetItem *item_path = ui->tableWidget->item(row, 1);
        MainWindow::binaries_list t = {.offset = item_offset->text(), .path = item_path->text()};
        bins.push_back(t);
    }
    return bins;
}

void MainWindow::on_stop_btn_clicked()
{
    if(esp_tool_process->state() != QProcess::ProcessState::NotRunning)
        esp_tool_process->terminate();
}

void MainWindow::burn_efuses(QString command, QVector<efuses_orders> orders)
{
    QStringList args;
    args.push_back(command);
    for (const auto &item : orders) {
        args.push_back(item.block);
        args.push_back(item.value);
    }
    run_esp_fuse(args);
}

void MainWindow::run_esp_fuse(QStringList args){
    if (ui->comboBoxPorts->currentText() != "Auto") {
        QString com = ui->comboBoxPorts->currentText().replace('\r', "").split(' ')[0];
        args.push_front(com);
        args.push_front("--port");
    }
    qDebug() << "args" << args;
    esp_efuse_process->setArguments(args);
    ui_running();
    esp_efuse_process->start();
}

void MainWindow::run_esp_secure(QStringList args){
    qDebug() << "args" << args;
    esp_secure_process->setArguments(args);
    ui_running();
    esp_secure_process->start();
}
