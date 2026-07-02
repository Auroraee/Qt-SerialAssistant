#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "serialportmanager.h"
#include "utils.h"

#include <QMessageBox>
#include <QSerialPortInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_portManager(new SerialPortManager(this))
    , m_rxFormatGroup(new QButtonGroup(this))
    , m_txFormatGroup(new QButtonGroup(this))
{
    ui->setupUi(this);

    m_rxFormatGroup->addButton(ui->pushButtonRxHex, 0);
    m_rxFormatGroup->addButton(ui->pushButtonRxAscii, 1);
    m_rxFormatGroup->setExclusive(true);

    m_txFormatGroup->addButton(ui->pushButtonTxHex, 0);
    m_txFormatGroup->addButton(ui->pushButtonTxAscii, 1);
    m_txFormatGroup->setExclusive(true);

    setupUiDefaults();
    connectSignals();
    refreshPortList();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUiDefaults()
{
    setWindowTitle(tr("串口助手"));

    ui->comboBoxBaudRate->addItems({
        QStringLiteral("1200"),
        QStringLiteral("2400"),
        QStringLiteral("4800"),
        QStringLiteral("9600"),
        QStringLiteral("19200"),
        QStringLiteral("38400"),
        QStringLiteral("57600"),
        QStringLiteral("115200")
    });
    ui->comboBoxBaudRate->setCurrentText(QStringLiteral("115200"));

    ui->pushButtonRxHex->setChecked(true);
    ui->pushButtonTxAscii->setChecked(true);
    m_isHexReceive = true;
    m_isHexSend = false;

    ui->pushButtonSend->setEnabled(false);
    ui->lineEditSend->setEnabled(false);
    ui->checkBoxAutoSend->setEnabled(false);
    ui->lineEditAutoSendInterval->setEnabled(false);
}

void MainWindow::connectSignals()
{
    connect(m_portManager, &SerialPortManager::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);
    connect(m_portManager, &SerialPortManager::dataReceived,
            this, &MainWindow::onDataReceived);
    connect(m_portManager, &SerialPortManager::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    connect(m_portManager, &SerialPortManager::autoSendTriggered,
            this, &MainWindow::onAutoSendTriggered);

    connect(m_rxFormatGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id) { m_isHexReceive = (id == 0); });
    connect(m_txFormatGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id) { m_isHexSend = (id == 0); });

    // 允许刷新端口列表：点击下拉时重新枚举
    connect(ui->comboBoxPort, QOverload<int>::of(&QComboBox::activated),
            this, &MainWindow::refreshPortList);
}

void MainWindow::refreshPortList()
{
    QString current = ui->comboBoxPort->currentText();
    ui->comboBoxPort->clear();

    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        ui->comboBoxPort->addItem(info.portName());
    }

    int index = ui->comboBoxPort->findText(current);
    if (index >= 0)
        ui->comboBoxPort->setCurrentIndex(index);
}

void MainWindow::on_pushButtonOpen_clicked()
{
    if (m_portManager->isOpen()) {
        m_portManager->close();
        return;
    }

    QString portName = ui->comboBoxPort->currentText();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先选择串口"));
        return;
    }

    bool ok = false;
    int baudRate = ui->comboBoxBaudRate->currentText().toInt(&ok);
    if (!ok || baudRate <= 0) {
        QMessageBox::warning(this, tr("警告"), tr("波特率无效"));
        return;
    }

    m_portManager->open(portName, baudRate);
}

void MainWindow::onConnectionStateChanged(bool connected)
{
    ui->pushButtonOpen->setText(connected ? tr("关闭串口") : tr("打开串口"));
    ui->comboBoxPort->setEnabled(!connected);
    ui->comboBoxBaudRate->setEnabled(!connected);
    ui->pushButtonSend->setEnabled(connected);
    ui->lineEditSend->setEnabled(connected);
    ui->checkBoxAutoSend->setEnabled(connected);
    ui->lineEditAutoSendInterval->setEnabled(connected && ui->checkBoxAutoSend->isChecked());
    ui->labelStatus->setText(connected ? tr("状态：已连接") : tr("状态：未连接"));

    if (connected && ui->checkBoxAutoSend->isChecked()) {
        on_checkBoxAutoSend_stateChanged(Qt::Checked);
    }

    if (!connected) {
        ui->checkBoxAutoSend->setChecked(false);
    }
}

void MainWindow::onDataReceived(const QByteArray &data)
{
    appendReceiveData(data);
}

void MainWindow::appendReceiveData(const QByteArray &data)
{
    QString text;
    if (m_isHexReceive) {
        text = Utils::toHexString(data);
    } else {
        text = Utils::toAsciiDisplayString(data);
    }

    ui->plainTextEditReceive->moveCursor(QTextCursor::End);
    ui->plainTextEditReceive->insertPlainText(text + QStringLiteral(" "));
    ui->plainTextEditReceive->moveCursor(QTextCursor::End);
}

void MainWindow::onErrorOccurred(const QString &message)
{
    QMessageBox::critical(this, tr("串口错误"), message);
}

void MainWindow::on_pushButtonSend_clicked()
{
    QByteArray data = buildSendData();
    if (data.isEmpty())
        return;

    m_portManager->sendData(data);
}

void MainWindow::onAutoSendTriggered()
{
    QByteArray data = buildSendData(false);
    if (!data.isEmpty())
        m_portManager->sendData(data);
}

QByteArray MainWindow::buildSendData(bool showErrors)
{
    QString input = ui->lineEditSend->text();
    if (input.isEmpty())
        return QByteArray();

    if (m_isHexSend) {
        bool ok = false;
        QByteArray data = Utils::parseHexString(input, &ok);
        if (!ok) {
            if (showErrors) {
                QMessageBox::warning(this,
                                     tr("警告"),
                                     tr("发送内容不是有效的 HEX 字符串"));
            }
            return QByteArray();
        }
        return data;
    }

    return input.toUtf8();
}

void MainWindow::on_checkBoxAutoSend_stateChanged(int state)
{
    bool enabled = (state == Qt::Checked);
    ui->lineEditAutoSendInterval->setEnabled(enabled);

    if (!m_portManager->isOpen())
        return;

    bool ok = false;
    int interval = ui->lineEditAutoSendInterval->text().toInt(&ok);
    if (enabled && (!ok || interval <= 0)) {
        QMessageBox::warning(this, tr("警告"), tr("定时发送间隔无效，已恢复为 1000 ms"));
        ui->lineEditAutoSendInterval->setText(QStringLiteral("1000"));
        interval = 1000;
    }

    m_portManager->setAutoSendEnabled(enabled, interval);
}
