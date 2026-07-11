#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "framelogmodel.h"
#include "serialportmanager.h"
#include "serialsessioncontroller.h"
#include "utils.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSpinBox>
#include <QTableView>
#include <QTabWidget>
#include <QTextCharFormat>
#include <QTextCursor>

namespace {
constexpr int kSendHistoryLimit = 20;
constexpr int kSettingsVersion = 2;

void setComboByData(QComboBox *combo, const QVariant &value)
{
    if (!combo)
        return;
    const int index = combo->findData(value);
    if (index >= 0)
        combo->setCurrentIndex(index);
}

QString formatName(bool hex)
{
    return hex ? QStringLiteral("HEX") : QStringLiteral("ASCII");
}

QTextCharFormat logFormat(const QColor &color, bool bold = false)
{
    QTextCharFormat textFormat;
    textFormat.setForeground(color);
    if (bold)
        textFormat.setFontWeight(QFont::DemiBold);
    return textFormat;
}

QTextCharFormat directionFormat(const QString &direction)
{
    if (direction == QLatin1String("RX"))
        return logFormat(QColor(QStringLiteral("#2563EB")), true);
    if (direction == QLatin1String("TX"))
        return logFormat(QColor(QStringLiteral("#16A34A")), true);
    if (direction == QLatin1String("ERR"))
        return logFormat(QColor(QStringLiteral("#DC2626")), true);
    return logFormat(QColor(QStringLiteral("#374151")), true);
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_session(new SerialSessionController(this))
    , m_rxFormatGroup(new QButtonGroup(this))
    , m_txFormatGroup(new QButtonGroup(this))
    , m_portRefreshTimer(new QTimer(this))
{
    ui->setupUi(this);
    setupProtocolWorkbench();

    m_rxFormatGroup->addButton(ui->pushButtonRxHex, 0);
    m_rxFormatGroup->addButton(ui->pushButtonRxAscii, 1);
    m_rxFormatGroup->setExclusive(true);

    m_txFormatGroup->addButton(ui->pushButtonTxHex, 0);
    m_txFormatGroup->addButton(ui->pushButtonTxAscii, 1);
    m_txFormatGroup->setExclusive(true);

    setupUiDefaults();
    connectSignals();
    refreshPortList();
    loadSettings();

    m_portRefreshTimer->setInterval(2000);
    connect(m_portRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshPortList);
    m_portRefreshTimer->start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupProtocolWorkbench()
{
    const int receiveIndex = ui->commLayout->indexOf(ui->plainTextEditReceive);
    ui->commLayout->removeWidget(ui->plainTextEditReceive);

    auto *workbench = new QGroupBox(tr("协议工作台"), ui->centralwidget);
    auto *layout = new QGridLayout(workbench);

    m_frameModeCombo = new QComboBox(workbench);
    m_frameModeCombo->setObjectName(QStringLiteral("frameModeCombo"));
    m_frameModeCombo->addItem(tr("静默间隔"), int(FramingMode::IdleGap));
    m_frameModeCombo->addItem(tr("分隔符"), int(FramingMode::Delimiter));
    m_frameModeCombo->addItem(tr("固定长度"), int(FramingMode::FixedLength));
    m_frameValueSpin = new QSpinBox(workbench);
    m_frameValueSpin->setObjectName(QStringLiteral("frameValueSpin"));
    m_frameValueSpin->setRange(1, 60000);
    m_frameValueSpin->setValue(20);
    m_frameValueSpin->setSuffix(QStringLiteral(" ms"));
    m_frameDelimiterEdit = new QLineEdit(QStringLiteral("0D 0A"), workbench);
    m_frameDelimiterEdit->setObjectName(QStringLiteral("frameDelimiterEdit"));
    m_frameDelimiterEdit->setPlaceholderText(tr("HEX 分隔符，如 0D 0A"));
    m_includeDelimiterCheck = new QCheckBox(tr("保留分隔符"), workbench);
    m_includeDelimiterCheck->setObjectName(QStringLiteral("includeDelimiterCheck"));

    m_filterDirectionCombo = new QComboBox(workbench);
    m_filterDirectionCombo->setObjectName(QStringLiteral("filterDirectionCombo"));
    m_filterDirectionCombo->addItem(tr("全部方向"), -1);
    m_filterDirectionCombo->addItem(QStringLiteral("RX"), int(FrameDirection::Rx));
    m_filterDirectionCombo->addItem(QStringLiteral("TX"), int(FrameDirection::Tx));
    m_filterDirectionCombo->addItem(QStringLiteral("ERR"), int(FrameDirection::Error));
    m_filterProtocolCombo = new QComboBox(workbench);
    m_filterProtocolCombo->setObjectName(QStringLiteral("filterProtocolCombo"));
    m_filterProtocolCombo->addItem(tr("全部协议"), QString());
    m_filterProtocolCombo->addItem(QStringLiteral("Modbus RTU"), QStringLiteral("Modbus RTU"));
    m_filterProtocolCombo->addItem(QStringLiteral("Raw"), QStringLiteral("Raw"));
    m_filterProtocolCombo->addItem(QStringLiteral("System"), QStringLiteral("System"));
    m_filterStationSpin = new QSpinBox(workbench);
    m_filterStationSpin->setObjectName(QStringLiteral("filterStationSpin"));
    m_filterStationSpin->setRange(-1, 247);
    m_filterStationSpin->setSpecialValueText(tr("全部站号"));
    m_filterStationSpin->setValue(-1);
    m_filterFunctionSpin = new QSpinBox(workbench);
    m_filterFunctionSpin->setObjectName(QStringLiteral("filterFunctionSpin"));
    m_filterFunctionSpin->setRange(-1, 255);
    m_filterFunctionSpin->setSpecialValueText(tr("全部功能码"));
    m_filterFunctionSpin->setValue(-1);
    m_filterFunctionSpin->setDisplayIntegerBase(16);
    m_filterFunctionSpin->setPrefix(QStringLiteral("0x"));
    m_filterCrcCombo = new QComboBox(workbench);
    m_filterCrcCombo->setObjectName(QStringLiteral("filterCrcCombo"));
    m_filterCrcCombo->addItem(tr("全部 CRC"), -1);
    m_filterCrcCombo->addItem(QStringLiteral("OK"), int(CrcStatus::Valid));
    m_filterCrcCombo->addItem(QStringLiteral("BAD"), int(CrcStatus::Invalid));
    m_filterCrcCombo->addItem(QStringLiteral("--"), int(CrcStatus::NotChecked));
    m_filterKeywordEdit = new QLineEdit(workbench);
    m_filterKeywordEdit->setObjectName(QStringLiteral("filterKeywordEdit"));
    m_filterKeywordEdit->setPlaceholderText(tr("关键字筛选"));

    layout->addWidget(new QLabel(tr("分帧"), workbench), 0, 0);
    layout->addWidget(m_frameModeCombo, 0, 1);
    layout->addWidget(m_frameValueSpin, 0, 2);
    layout->addWidget(m_frameDelimiterEdit, 0, 3);
    layout->addWidget(m_includeDelimiterCheck, 0, 4);
    layout->addWidget(new QLabel(tr("筛选"), workbench), 1, 0);
    layout->addWidget(m_filterDirectionCombo, 1, 1);
    layout->addWidget(m_filterProtocolCombo, 1, 2);
    layout->addWidget(m_filterStationSpin, 1, 3);
    layout->addWidget(m_filterFunctionSpin, 1, 4);
    layout->addWidget(m_filterCrcCombo, 1, 5);
    layout->addWidget(m_filterKeywordEdit, 1, 6);
    layout->setColumnStretch(6, 1);

    m_receiveTabs = new QTabWidget(ui->centralwidget);
    m_receiveTabs->setObjectName(QStringLiteral("receiveTabs"));
    ui->plainTextEditReceive->setParent(m_receiveTabs);
    m_receiveTabs->addTab(ui->plainTextEditReceive, tr("原始流"));
    m_frameTable = new QTableView(m_receiveTabs);
    m_frameTable->setObjectName(QStringLiteral("frameTable"));
    m_frameProxy = new FrameFilterProxyModel(this);
    m_frameProxy->setSourceModel(m_session->logModel());
    m_frameTable->setModel(m_frameProxy);
    m_frameTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_frameTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_frameTable->setAlternatingRowColors(true);
    m_frameTable->setSortingEnabled(true);
    m_frameTable->horizontalHeader()->setStretchLastSection(true);
    m_frameTable->horizontalHeader()->setSectionResizeMode(FrameLogModel::TimestampColumn,
                                                           QHeaderView::ResizeToContents);
    m_frameTable->horizontalHeader()->setSectionResizeMode(FrameLogModel::DirectionColumn,
                                                           QHeaderView::ResizeToContents);
    m_frameTable->horizontalHeader()->setSectionResizeMode(FrameLogModel::HexColumn,
                                                           QHeaderView::Stretch);
    m_frameTable->horizontalHeader()->setSectionResizeMode(FrameLogModel::SummaryColumn,
                                                           QHeaderView::Stretch);
    m_receiveTabs->addTab(m_frameTable, tr("帧列表"));

    ui->commLayout->insertWidget(receiveIndex, workbench);
    ui->commLayout->insertWidget(receiveIndex + 1, m_receiveTabs, 1);

    const auto updateFramingControls = [this] {
        const FramingMode mode = static_cast<FramingMode>(m_frameModeCombo->currentData().toInt());
        const bool idle = mode == FramingMode::IdleGap;
        const bool delimiter = mode == FramingMode::Delimiter;
        m_frameValueSpin->setEnabled(!delimiter);
        m_frameValueSpin->setRange(1, idle ? 60000 : 65536);
        m_frameValueSpin->setSuffix(idle ? QStringLiteral(" ms") : QStringLiteral(" B"));
        m_frameDelimiterEdit->setEnabled(delimiter);
        m_includeDelimiterCheck->setEnabled(delimiter);
        applyFrameSettings();
    };
    connect(m_frameModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [updateFramingControls](int) { updateFramingControls(); });
    connect(m_frameValueSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { applyFrameSettings(); });
    connect(m_frameDelimiterEdit, &QLineEdit::editingFinished,
            this, &MainWindow::applyFrameSettings);
    connect(m_includeDelimiterCheck, &QCheckBox::toggled,
            this, &MainWindow::applyFrameSettings);

    connect(m_filterDirectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::applyFrameFilter);
    connect(m_filterProtocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::applyFrameFilter);
    connect(m_filterStationSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::applyFrameFilter);
    connect(m_filterFunctionSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::applyFrameFilter);
    connect(m_filterCrcCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::applyFrameFilter);
    connect(m_filterKeywordEdit, &QLineEdit::textChanged,
            this, &MainWindow::applyFrameFilter);

    updateFramingControls();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupUiDefaults()
{
    setWindowTitle(tr("串口助手"));
    ui->plainTextEditReceive->document()->setMaximumBlockCount(10000);

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
    if (ui->comboBoxBaudRate->lineEdit()) {
        ui->comboBoxBaudRate->lineEdit()->setValidator(
            new QIntValidator(1, 4000000, ui->comboBoxBaudRate));
    }
    ui->lineEditAutoSendInterval->setValidator(
        new QIntValidator(1, 86400000, ui->lineEditAutoSendInterval));

    ui->comboBoxDataBits->addItem(QStringLiteral("5"), QSerialPort::Data5);
    ui->comboBoxDataBits->addItem(QStringLiteral("6"), QSerialPort::Data6);
    ui->comboBoxDataBits->addItem(QStringLiteral("7"), QSerialPort::Data7);
    ui->comboBoxDataBits->addItem(QStringLiteral("8"), QSerialPort::Data8);
    ui->comboBoxDataBits->setCurrentIndex(ui->comboBoxDataBits->findData(QSerialPort::Data8));

    ui->comboBoxParity->addItem(tr("None"), QSerialPort::NoParity);
    ui->comboBoxParity->addItem(tr("Even"), QSerialPort::EvenParity);
    ui->comboBoxParity->addItem(tr("Odd"), QSerialPort::OddParity);
    ui->comboBoxParity->addItem(tr("Space"), QSerialPort::SpaceParity);
    ui->comboBoxParity->addItem(tr("Mark"), QSerialPort::MarkParity);

    ui->comboBoxStopBits->addItem(QStringLiteral("1"), QSerialPort::OneStop);
    ui->comboBoxStopBits->addItem(QStringLiteral("1.5"), QSerialPort::OneAndHalfStop);
    ui->comboBoxStopBits->addItem(QStringLiteral("2"), QSerialPort::TwoStop);

    ui->comboBoxFlowControl->addItem(tr("None"), QSerialPort::NoFlowControl);
    ui->comboBoxFlowControl->addItem(tr("Hardware"), QSerialPort::HardwareControl);
    ui->comboBoxFlowControl->addItem(tr("Software"), QSerialPort::SoftwareControl);

    auto *languageLabel = new QLabel(tr("界面语言"), statusBar());
    languageLabel->setObjectName(QStringLiteral("labelLanguage"));
    m_languageCombo = new QComboBox(statusBar());
    m_languageCombo->setObjectName(QStringLiteral("comboBoxLanguage"));
    m_languageCombo->setMinimumContentsLength(8);
    m_languageCombo->addItem(QStringLiteral("简体中文"), QStringLiteral("zh_CN"));
    m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en_US"));
    statusBar()->addPermanentWidget(languageLabel);
    statusBar()->addPermanentWidget(m_languageCombo);
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("Application"));
        const QString language = settings.value(QStringLiteral("language"),
                                                QStringLiteral("zh_CN")).toString();
        settings.endGroup();
        setComboByData(m_languageCombo, language);
    }

    ui->comboBoxLineEnding->addItem(tr("无"), QString());
    ui->comboBoxLineEnding->addItem(QStringLiteral("CRLF"), QStringLiteral("\r\n"));
    ui->comboBoxLineEnding->addItem(QStringLiteral("LF"), QStringLiteral("\n"));
    ui->comboBoxLineEnding->addItem(QStringLiteral("CR"), QStringLiteral("\r"));

    ui->comboBoxSendHistory->setPlaceholderText(tr("发送历史"));
    ui->comboBoxChecksumFormat->addItem(QStringLiteral("HEX"), QStringLiteral("hex"));
    ui->comboBoxChecksumFormat->addItem(QStringLiteral("ASCII"), QStringLiteral("ascii"));

    ui->pushButtonRxHex->setChecked(true);
    ui->pushButtonTxAscii->setChecked(true);
    ui->checkBoxAutoScroll->setChecked(true);
    ui->checkBoxAutoSendEcho->setChecked(true);
    m_isHexReceive = true;
    m_isHexSend = false;

    setSerialControlsEnabled(true);
    setSendControlsEnabled(false);
    clearCounters();
    showStatusMessage(tr("状态：未连接"), 0);
}

void MainWindow::connectSignals()
{
    connect(m_session, &SerialSessionController::stateChanged,
            this, &MainWindow::onConnectionStateChanged);
    connect(m_session, &SerialSessionController::rawDataReceived,
            this, &MainWindow::onDataReceived);
    connect(m_session, &SerialSessionController::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    connect(m_session, &SerialSessionController::criticalErrorOccurred,
            this, &MainWindow::onCriticalErrorOccurred);
    connect(m_session, &SerialSessionController::countersChanged,
            this, [this](qint64 rxBytes, qint64 txBytes) {
                m_rxBytes = rxBytes;
                m_txBytes = txBytes;
                updateCounters();
            });
    connect(m_session, &SerialSessionController::autoSendStopped,
            this, [this](const QString &reason) {
                QSignalBlocker blocker(ui->checkBoxAutoSend);
                ui->checkBoxAutoSend->setChecked(false);
                ui->lineEditAutoSendInterval->setEnabled(false);
                showStatusMessage(tr("定时发送已停止：%1").arg(reason));
            });
    connect(m_session->logModel(), &QAbstractItemModel::rowsInserted,
            this, &MainWindow::onFrameRowsInserted);

    connect(m_rxFormatGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id) { m_isHexReceive = (id == 0); });
    connect(m_txFormatGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id) {
                m_isHexSend = (id == 0);
                updateAutoSendPayload();
            });

    connect(ui->pushButtonClearReceive, &QPushButton::clicked,
            this, [this] {
                ui->plainTextEditReceive->clear();
                m_session->logModel()->clear();
            });
    connect(ui->pushButtonExportLog, &QPushButton::clicked,
            this, &MainWindow::exportLog);
    connect(ui->pushButtonClearCounters, &QPushButton::clicked,
            m_session, &SerialSessionController::clearCounters);
    connect(ui->lineEditSend, &QLineEdit::returnPressed,
            this, &MainWindow::on_pushButtonSend_clicked);
    connect(ui->comboBoxSendHistory, QOverload<int>::of(&QComboBox::activated),
            this, [this](int index) {
                if (index >= 0)
                    ui->lineEditSend->setText(ui->comboBoxSendHistory->itemText(index));
            });

    connect(ui->pushButtonQuickSend1, &QPushButton::clicked,
            this, [this] { sendUserText(ui->lineEditQuickSend1->text(), true, true, true); });
    connect(ui->pushButtonQuickSend2, &QPushButton::clicked,
            this, [this] { sendUserText(ui->lineEditQuickSend2->text(), true, true, true); });
    connect(ui->pushButtonCalculateChecksum, &QPushButton::clicked,
            this, &MainWindow::calculateChecksum);
    connect(ui->lineEditChecksumInput, &QLineEdit::returnPressed,
            this, &MainWindow::calculateChecksum);
    connect(ui->lineEditSend, &QLineEdit::textChanged,
            this, &MainWindow::updateAutoSendPayload);
    connect(ui->comboBoxLineEnding, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateAutoSendPayload);
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                QSettings settings;
                settings.beginGroup(QStringLiteral("Application"));
                settings.setValue(QStringLiteral("language"),
                                  m_languageCombo->currentData());
                settings.endGroup();
                settings.sync();
                showStatusMessage(tr("语言设置将在重启后生效"), 5000);
            });
}

void MainWindow::refreshPortList()
{
    if (m_session->isOpen())
        return;

    const QString current = ui->comboBoxPort->currentData().toString().isEmpty()
                                ? ui->comboBoxPort->currentText()
                                : ui->comboBoxPort->currentData().toString();
    ui->comboBoxPort->clear();

    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        QString label = info.portName();
        if (!info.description().isEmpty())
            label += QStringLiteral(" — ") + info.description();
        ui->comboBoxPort->addItem(label, info.portName());
    }

    int index = ui->comboBoxPort->findData(current);
    if (index >= 0) {
        ui->comboBoxPort->setCurrentIndex(index);
    } else if (!current.isEmpty()) {
        // Keep last-used port visible even if temporarily unavailable.
        ui->comboBoxPort->insertItem(0, current, current);
        ui->comboBoxPort->setCurrentIndex(0);
    }
}

void MainWindow::loadSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("MainWindow"));

    if (settings.contains(QStringLiteral("geometry")))
        restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());

    const QString portName = settings.value(QStringLiteral("portName")).toString();
    if (!portName.isEmpty()) {
        int index = ui->comboBoxPort->findData(portName);
        if (index < 0) {
            ui->comboBoxPort->insertItem(0, portName, portName);
            index = 0;
        }
        ui->comboBoxPort->setCurrentIndex(index);
    }

    const QString baudRate = settings.value(QStringLiteral("baudRate"),
                                            QStringLiteral("115200")).toString();
    if (!baudRate.isEmpty())
        ui->comboBoxBaudRate->setCurrentText(baudRate);

    setComboByData(ui->comboBoxDataBits,
                   settings.value(QStringLiteral("dataBits"), static_cast<int>(QSerialPort::Data8)));
    setComboByData(ui->comboBoxParity,
                   settings.value(QStringLiteral("parity"), static_cast<int>(QSerialPort::NoParity)));
    setComboByData(ui->comboBoxStopBits,
                   settings.value(QStringLiteral("stopBits"), static_cast<int>(QSerialPort::OneStop)));
    setComboByData(ui->comboBoxFlowControl,
                   settings.value(QStringLiteral("flowControl"),
                                  static_cast<int>(QSerialPort::NoFlowControl)));

    const int lineEndingIndex = settings.value(QStringLiteral("lineEndingIndex"), 0).toInt();
    if (lineEndingIndex >= 0 && lineEndingIndex < ui->comboBoxLineEnding->count())
        ui->comboBoxLineEnding->setCurrentIndex(lineEndingIndex);

    m_isHexReceive = settings.value(QStringLiteral("rxHex"), true).toBool();
    m_isHexSend = settings.value(QStringLiteral("txHex"), false).toBool();
    ui->pushButtonRxHex->setChecked(m_isHexReceive);
    ui->pushButtonRxAscii->setChecked(!m_isHexReceive);
    ui->pushButtonTxHex->setChecked(m_isHexSend);
    ui->pushButtonTxAscii->setChecked(!m_isHexSend);

    ui->checkBoxAutoScroll->setChecked(
        settings.value(QStringLiteral("autoScroll"), true).toBool());
    ui->checkBoxAutoSendEcho->setChecked(
        settings.value(QStringLiteral("autoSendEcho"), true).toBool());
    ui->lineEditAutoSendInterval->setText(
        settings.value(QStringLiteral("autoSendInterval"), QStringLiteral("1000")).toString());

    ui->lineEditQuickSend1->setText(settings.value(QStringLiteral("quickSend1")).toString());
    ui->lineEditQuickSend2->setText(settings.value(QStringLiteral("quickSend2")).toString());
    ui->lineEditSend->setText(settings.value(QStringLiteral("lastSendText")).toString());

    const QString checksumFormat = settings.value(QStringLiteral("checksumFormat"),
                                                  QStringLiteral("hex")).toString();
    setComboByData(ui->comboBoxChecksumFormat, checksumFormat);
    ui->lineEditChecksumInput->setText(
        settings.value(QStringLiteral("checksumInput")).toString());

    const QStringList history = settings.value(QStringLiteral("sendHistory")).toStringList();
    {
        QSignalBlocker blocker(ui->comboBoxSendHistory);
        ui->comboBoxSendHistory->clear();
        for (const QString &item : history) {
            if (!item.isEmpty())
                ui->comboBoxSendHistory->addItem(item);
        }
        while (ui->comboBoxSendHistory->count() > kSendHistoryLimit)
            ui->comboBoxSendHistory->removeItem(ui->comboBoxSendHistory->count() - 1);
        if (ui->comboBoxSendHistory->count() > 0)
            ui->comboBoxSendHistory->setCurrentIndex(0);
    }

    const int settingsVersion = settings.value(QStringLiteral("version"), 1).toInt();
    if (settingsVersion >= 2 && settingsVersion <= kSettingsVersion) {
        const QSignalBlocker modeBlocker(m_frameModeCombo);
        setComboByData(m_frameModeCombo,
                       settings.value(QStringLiteral("framingMode"), int(FramingMode::IdleGap)));
        const FramingMode savedMode = static_cast<FramingMode>(
            m_frameModeCombo->currentData().toInt());
        m_frameValueSpin->setRange(1, savedMode == FramingMode::FixedLength ? 65536 : 60000);
        m_frameValueSpin->setValue(
            settings.value(QStringLiteral("framingValue"), 20).toInt());
        m_frameDelimiterEdit->setText(
            settings.value(QStringLiteral("frameDelimiter"), QStringLiteral("0D 0A")).toString());
        m_includeDelimiterCheck->setChecked(
            settings.value(QStringLiteral("includeDelimiter"), false).toBool());
        setComboByData(m_filterDirectionCombo,
                       settings.value(QStringLiteral("filterDirection"), -1));
        setComboByData(m_filterProtocolCombo,
                       settings.value(QStringLiteral("filterProtocol"), QString()));
        m_filterStationSpin->setValue(
            settings.value(QStringLiteral("filterStation"), -1).toInt());
        m_filterFunctionSpin->setValue(
            settings.value(QStringLiteral("filterFunction"), -1).toInt());
        setComboByData(m_filterCrcCombo,
                       settings.value(QStringLiteral("filterCrc"), -1));
        m_filterKeywordEdit->setText(
            settings.value(QStringLiteral("filterKeyword")).toString());
    }

    settings.endGroup();
    applyFrameSettings();
    applyFrameFilter();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("MainWindow"));

    settings.setValue(QStringLiteral("version"), kSettingsVersion);
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("portName"),
                      ui->comboBoxPort->currentData().toString().isEmpty()
                          ? ui->comboBoxPort->currentText()
                          : ui->comboBoxPort->currentData());
    settings.setValue(QStringLiteral("baudRate"), ui->comboBoxBaudRate->currentText());
    settings.setValue(QStringLiteral("dataBits"), ui->comboBoxDataBits->currentData());
    settings.setValue(QStringLiteral("parity"), ui->comboBoxParity->currentData());
    settings.setValue(QStringLiteral("stopBits"), ui->comboBoxStopBits->currentData());
    settings.setValue(QStringLiteral("flowControl"), ui->comboBoxFlowControl->currentData());
    settings.setValue(QStringLiteral("lineEndingIndex"), ui->comboBoxLineEnding->currentIndex());
    settings.setValue(QStringLiteral("rxHex"), m_isHexReceive);
    settings.setValue(QStringLiteral("txHex"), m_isHexSend);
    settings.setValue(QStringLiteral("autoScroll"), ui->checkBoxAutoScroll->isChecked());
    settings.setValue(QStringLiteral("autoSendEcho"), ui->checkBoxAutoSendEcho->isChecked());
    settings.setValue(QStringLiteral("autoSendInterval"), ui->lineEditAutoSendInterval->text());
    settings.setValue(QStringLiteral("quickSend1"), ui->lineEditQuickSend1->text());
    settings.setValue(QStringLiteral("quickSend2"), ui->lineEditQuickSend2->text());
    settings.setValue(QStringLiteral("lastSendText"), ui->lineEditSend->text());
    settings.setValue(QStringLiteral("checksumFormat"), ui->comboBoxChecksumFormat->currentData());
    settings.setValue(QStringLiteral("checksumInput"), ui->lineEditChecksumInput->text());
    settings.setValue(QStringLiteral("framingMode"), m_frameModeCombo->currentData());
    settings.setValue(QStringLiteral("framingValue"), m_frameValueSpin->value());
    settings.setValue(QStringLiteral("frameDelimiter"), m_frameDelimiterEdit->text());
    settings.setValue(QStringLiteral("includeDelimiter"), m_includeDelimiterCheck->isChecked());
    settings.setValue(QStringLiteral("filterDirection"), m_filterDirectionCombo->currentData());
    settings.setValue(QStringLiteral("filterProtocol"), m_filterProtocolCombo->currentData());
    settings.setValue(QStringLiteral("filterStation"), m_filterStationSpin->value());
    settings.setValue(QStringLiteral("filterFunction"), m_filterFunctionSpin->value());
    settings.setValue(QStringLiteral("filterCrc"), m_filterCrcCombo->currentData());
    settings.setValue(QStringLiteral("filterKeyword"), m_filterKeywordEdit->text());

    QStringList history;
    history.reserve(ui->comboBoxSendHistory->count());
    for (int i = 0; i < ui->comboBoxSendHistory->count(); ++i)
        history.append(ui->comboBoxSendHistory->itemText(i));
    settings.setValue(QStringLiteral("sendHistory"), history);

    settings.endGroup();
    settings.sync();
}

void MainWindow::exportLog()
{
    if (m_frameProxy->rowCount() == 0) {
        showStatusMessage(tr("帧列表为空，无需导出"));
        return;
    }

    const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString defaultName = QStringLiteral("serial_frames_%1.txt")
                                    .arg(QDateTime::currentDateTime().toString(
                                        QStringLiteral("yyyyMMdd_HHmmss")));
    const QString defaultPath = defaultDir.isEmpty()
                                    ? defaultName
                                    : defaultDir + QLatin1Char('/') + defaultName;

    QString selectedFilter;
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("导出日志"),
        defaultPath,
        tr("文本文件 (*.txt);;CSV 文件 (*.csv);;JSON Lines (*.jsonl)"),
        &selectedFilter);
    if (filePath.isEmpty())
        return;

    LogExportFormat format = LogExportFormat::Text;
    QString suffix = QStringLiteral(".txt");
    if (selectedFilter.contains(QStringLiteral("CSV"))) {
        format = LogExportFormat::Csv;
        suffix = QStringLiteral(".csv");
    } else if (selectedFilter.contains(QStringLiteral("JSON"))) {
        format = LogExportFormat::JsonLines;
        suffix = QStringLiteral(".jsonl");
    }
    if (QFileInfo(filePath).suffix().isEmpty())
        filePath += suffix;

    QList<FrameRecord> filteredRecords;
    filteredRecords.reserve(m_frameProxy->rowCount());
    for (int row = 0; row < m_frameProxy->rowCount(); ++row) {
        const QModelIndex sourceIndex = m_frameProxy->mapToSource(
            m_frameProxy->index(row, 0));
        filteredRecords.append(m_session->logModel()->recordAt(sourceIndex.row()));
    }
    FrameLogModel exportModel(nullptr, qMax(1, filteredRecords.size()), 32LL * 1024 * 1024);
    exportModel.appendBatch(filteredRecords);

    QString error;
    if (!exportModel.exportToFile(filePath, format, &error)) {
        const QString message = tr("无法写入文件: %1").arg(error);
        QMessageBox::warning(this, tr("导出失败"), message);
        appendErrorLog(message);
        showStatusMessage(message);
        return;
    }

    showStatusMessage(tr("日志已导出: %1").arg(filePath), 5000);
}

void MainWindow::on_pushButtonOpen_clicked()
{
    if (m_session->isOpen()) {
        m_session->close();
        return;
    }

    bool ok = false;
    const SerialPortConfig config = currentSerialConfig(&ok);
    if (!ok)
        return;

    m_session->open(config);
}

void MainWindow::onConnectionStateChanged(ConnectionState state)
{
    const bool connected = state == ConnectionState::Connected;
    const bool busy = state == ConnectionState::Opening || state == ConnectionState::Closing;
    ui->pushButtonOpen->setText(connected ? tr("关闭串口") : tr("打开串口"));
    ui->pushButtonOpen->setEnabled(!busy);
    setSerialControlsEnabled(!connected && !busy);
    setSendControlsEnabled(connected);
    QString statusText = tr("状态：未连接");
    if (state == ConnectionState::Opening)
        statusText = tr("状态：正在连接");
    else if (connected)
        statusText = tr("状态：已连接");
    else if (state == ConnectionState::Closing)
        statusText = tr("状态：正在断开");
    else if (state == ConnectionState::Error)
        statusText = tr("状态：错误");
    ui->labelStatus->setText(statusText);
    showStatusMessage(statusText, busy ? 0 : 3000);

    if (connected)
        m_portRefreshTimer->stop();
    else
        m_portRefreshTimer->start();

    if (!connected) {
        ui->checkBoxAutoSend->setChecked(false);
    } else if (ui->checkBoxAutoSend->isChecked()) {
        on_checkBoxAutoSend_stateChanged(Qt::Checked);
    }
}

void MainWindow::onDataReceived(const QByteArray &data)
{
    appendReceiveData(data);
}

void MainWindow::appendReceiveData(const QByteArray &data)
{
    appendLogLine(QStringLiteral("RX"), formatName(m_isHexReceive), formatData(data, m_isHexReceive));
}

void MainWindow::appendSentData(const QByteArray &data, bool echo)
{
    if (echo)
        appendLogLine(QStringLiteral("TX"), formatName(m_isHexSend), formatData(data, m_isHexSend));
}

void MainWindow::onErrorOccurred(const QString &message)
{
    appendErrorLog(message);
    showStatusMessage(message);
}

void MainWindow::onCriticalErrorOccurred(const QString &message)
{
    if (ui->checkBoxAutoSend->isChecked())
        ui->checkBoxAutoSend->setChecked(false);
    QMessageBox::critical(this, tr("串口错误"), message);
}

void MainWindow::on_pushButtonSend_clicked()
{
    sendUserText(ui->lineEditSend->text(), true, true, true);
}

void MainWindow::onFrameRowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    for (int row = first; row <= last; ++row) {
        const FrameRecord record = m_session->logModel()->recordAt(row);
        if (record.direction == FrameDirection::Tx) {
            const bool automatic = record.fields.value(QStringLiteral("automatic")).toBool();
            if (!automatic || ui->checkBoxAutoSendEcho->isChecked())
                appendSentData(record.rawData, true);
        }
    }
    if (ui->checkBoxAutoScroll->isChecked())
        m_frameTable->scrollToBottom();
}

SerialPortConfig MainWindow::currentSerialConfig(bool *ok)
{
    if (ok)
        *ok = false;

    SerialPortConfig config;
    config.portName = ui->comboBoxPort->currentData().toString().isEmpty()
                          ? ui->comboBoxPort->currentText()
                          : ui->comboBoxPort->currentData().toString();
    if (config.portName.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先选择串口"));
        return config;
    }

    bool baudOk = false;
    config.baudRate = ui->comboBoxBaudRate->currentText().toInt(&baudOk);
    if (!baudOk || config.baudRate <= 0) {
        QMessageBox::warning(this, tr("警告"), tr("波特率无效"));
        return config;
    }

    config.dataBits = static_cast<QSerialPort::DataBits>(ui->comboBoxDataBits->currentData().toInt());
    config.parity = static_cast<QSerialPort::Parity>(ui->comboBoxParity->currentData().toInt());
    config.stopBits = static_cast<QSerialPort::StopBits>(ui->comboBoxStopBits->currentData().toInt());
    config.flowControl = static_cast<QSerialPort::FlowControl>(ui->comboBoxFlowControl->currentData().toInt());

    if (ok)
        *ok = true;
    return config;
}

QByteArray MainWindow::buildSendDataFromText(const QString &input, bool showErrors, bool *ok)
{
    if (ok)
        *ok = false;

    if (input.isEmpty()) {
        if (ok)
            *ok = true;
        return QByteArray();
    }

    QByteArray data;
    if (m_isHexSend) {
        bool parseOk = false;
        data = Utils::parseHexString(input, &parseOk);
        if (!parseOk) {
            const QString message = tr("发送内容不是有效的 HEX 字符串");
            if (showErrors)
                QMessageBox::warning(this, tr("警告"), message);
            appendErrorLog(message);
            showStatusMessage(message);
            return QByteArray();
        }
    } else {
        for (const QChar &ch : input) {
            if (ch.unicode() > 0xFF) {
                const QString message = tr("ASCII 模式下不能包含非 ASCII/Latin1 字符");
                if (showErrors)
                    QMessageBox::warning(this, tr("警告"), message);
                appendErrorLog(message);
                showStatusMessage(message);
                return QByteArray();
            }
        }
        data = input.toLatin1();
    }

    data.append(lineEndingBytes());
    if (ok)
        *ok = true;
    return data;
}

QByteArray MainWindow::buildChecksumData(bool *ok)
{
    if (ok)
        *ok = false;

    const QString input = ui->lineEditChecksumInput->text();
    const QString mode = ui->comboBoxChecksumFormat->currentData().toString();
    QByteArray data;

    if (mode == QLatin1String("hex")) {
        bool parseOk = false;
        data = Utils::parseHexString(input, &parseOk);
        if (!parseOk) {
            const QString message = tr("校验输入不是有效的 HEX 字符串");
            ui->labelChecksumResult->setText(message);
            showStatusMessage(message);
            return QByteArray();
        }
    } else {
        for (const QChar &ch : input) {
            if (ch.unicode() > 0xFF) {
                const QString message = tr("ASCII 校验输入不能包含非 ASCII/Latin1 字符");
                ui->labelChecksumResult->setText(message);
                showStatusMessage(message);
                return QByteArray();
            }
        }
        data = input.toLatin1();
    }

    if (ok)
        *ok = true;
    return data;
}

void MainWindow::calculateChecksum()
{
    bool ok = false;
    const QByteArray data = buildChecksumData(&ok);
    if (!ok)
        return;

    const quint16 crc = Utils::crc16Modbus(data);
    QByteArray crcWireOrder;
    crcWireOrder.append(static_cast<char>(crc & 0xFF));
    crcWireOrder.append(static_cast<char>((crc >> 8) & 0xFF));

    const QString crcText = QStringLiteral("%1")
                                .arg(crc, 4, 16, QLatin1Char('0'))
                                .toUpper();
    const QString result = tr("CRC16(MODBUS): 0x%1 (低字节在前: %2)    Parity: 1位数=%3, Even=%4, Odd=%5")
                               .arg(crcText, Utils::toHexString(crcWireOrder))
                               .arg(Utils::bitCount(data))
                               .arg(Utils::evenParityBit(data))
                               .arg(Utils::oddParityBit(data));

    ui->labelChecksumResult->setText(result);
    showStatusMessage(tr("校验计算完成"), 3000);
}

QByteArray MainWindow::lineEndingBytes() const
{
    return ui->comboBoxLineEnding->currentData().toString().toLatin1();
}

QString MainWindow::formatData(const QByteArray &data, bool asHex) const
{
    if (asHex)
        return Utils::toHexString(data);
    return Utils::toAsciiDisplayString(data);
}

void MainWindow::sendUserText(const QString &input, bool addHistory, bool echoTx, bool showErrors)
{
    Q_UNUSED(echoTx)
    if (!m_session->isOpen()) {
        const QString message = tr("串口未打开");
        appendErrorLog(message);
        showStatusMessage(message);
        return;
    }

    bool ok = false;
    const QByteArray data = buildSendDataFromText(input, showErrors, &ok);
    if (!ok) {
        if (!showErrors && ui->checkBoxAutoSend->isChecked()) {
            ui->checkBoxAutoSend->setChecked(false);
            showStatusMessage(tr("定时发送已停止：发送内容无效"));
        }
        return;
    }

    if (data.isEmpty()) {
        const QString message = tr("发送内容为空");
        appendErrorLog(message);
        showStatusMessage(message);
        return;
    }

    const quint64 id = m_session->send(data);
    if (id != 0 && addHistory)
        addSendHistory(input);
}

void MainWindow::appendLogLine(const QString &direction, const QString &format, const QString &message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    QString label = direction;
    if (!format.isEmpty())
        label += QLatin1Char(' ') + format;

    QTextCursor cursor = ui->plainTextEditReceive->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!ui->plainTextEditReceive->document()->isEmpty())
        cursor.insertBlock();

    const QTextCharFormat timestampFormat = logFormat(QColor(QStringLiteral("#6B7280")));
    const QTextCharFormat labelFormat = directionFormat(direction);
    const QTextCharFormat dataFormat = direction == QLatin1String("ERR")
                                           ? logFormat(QColor(QStringLiteral("#B91C1C")))
                                           : logFormat(QColor(QStringLiteral("#111827")));

    cursor.insertText(QStringLiteral("[%1] ").arg(timestamp), timestampFormat);
    cursor.insertText(label, labelFormat);
    if (!message.isEmpty())
        cursor.insertText(QLatin1Char(' ') + message, dataFormat);

    if (ui->checkBoxAutoScroll->isChecked()) {
        QScrollBar *scrollBar = ui->plainTextEditReceive->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

void MainWindow::appendErrorLog(const QString &message)
{
    appendLogLine(QStringLiteral("ERR"), QString(), message);
}

void MainWindow::addSendHistory(const QString &input)
{
    if (input.isEmpty())
        return;

    QSignalBlocker blocker(ui->comboBoxSendHistory);
    const int existingIndex = ui->comboBoxSendHistory->findText(input);
    if (existingIndex >= 0)
        ui->comboBoxSendHistory->removeItem(existingIndex);

    ui->comboBoxSendHistory->insertItem(0, input);
    while (ui->comboBoxSendHistory->count() > kSendHistoryLimit)
        ui->comboBoxSendHistory->removeItem(ui->comboBoxSendHistory->count() - 1);
    ui->comboBoxSendHistory->setCurrentIndex(0);
}

void MainWindow::setSerialControlsEnabled(bool enabled)
{
    ui->comboBoxPort->setEnabled(enabled);
    ui->comboBoxBaudRate->setEnabled(enabled);
    ui->comboBoxDataBits->setEnabled(enabled);
    ui->comboBoxParity->setEnabled(enabled);
    ui->comboBoxStopBits->setEnabled(enabled);
    ui->comboBoxFlowControl->setEnabled(enabled);
}

void MainWindow::setSendControlsEnabled(bool connected)
{
    ui->pushButtonSend->setEnabled(connected);
    ui->checkBoxAutoSend->setEnabled(connected);
    ui->lineEditAutoSendInterval->setEnabled(connected && ui->checkBoxAutoSend->isChecked());
    ui->checkBoxAutoSendEcho->setEnabled(connected);
    ui->pushButtonQuickSend1->setEnabled(connected);
    ui->pushButtonQuickSend2->setEnabled(connected);
}

void MainWindow::updateCounters()
{
    ui->labelRxCount->setText(tr("RX: %1 B").arg(m_rxBytes));
    ui->labelTxCount->setText(tr("TX: %1 B").arg(m_txBytes));
}

void MainWindow::clearCounters()
{
    m_session->clearCounters();
    m_rxBytes = m_session->rxBytes();
    m_txBytes = m_session->txBytes();
    updateCounters();
}

void MainWindow::showStatusMessage(const QString &message, int timeoutMs)
{
    statusBar()->showMessage(message, timeoutMs);
}

void MainWindow::on_checkBoxAutoSend_stateChanged(int state)
{
    const bool enabled = (state == Qt::Checked);
    ui->lineEditAutoSendInterval->setEnabled(enabled && m_session->isOpen());

    if (!m_session->isOpen())
        return;

    bool ok = false;
    int interval = ui->lineEditAutoSendInterval->text().toInt(&ok);
    if (enabled && (!ok || interval <= 0)) {
        const QString message = tr("定时发送间隔无效，已恢复为 1000 ms");
        appendErrorLog(message);
        showStatusMessage(message);
        ui->lineEditAutoSendInterval->setText(QStringLiteral("1000"));
        interval = 1000;
    }

    QByteArray data;
    if (enabled) {
        bool dataOk = false;
        data = buildSendDataFromText(ui->lineEditSend->text(), true, &dataOk);
        if (!dataOk || data.isEmpty()) {
            QSignalBlocker blocker(ui->checkBoxAutoSend);
            ui->checkBoxAutoSend->setChecked(false);
            ui->lineEditAutoSendInterval->setEnabled(false);
            showStatusMessage(tr("定时发送已停止：发送内容无效"));
            return;
        }
    }
    m_session->setAutoSend(enabled, interval, data);
}

void MainWindow::updateAutoSendPayload()
{
    if (!ui->checkBoxAutoSend->isChecked())
        return;
    bool ok = false;
    const QByteArray data = buildSendDataFromText(ui->lineEditSend->text(), false, &ok);
    if (!ok || data.isEmpty()) {
        QSignalBlocker blocker(ui->checkBoxAutoSend);
        ui->checkBoxAutoSend->setChecked(false);
        ui->lineEditAutoSendInterval->setEnabled(false);
        m_session->setAutoSend(false, 0, QByteArray());
        showStatusMessage(tr("定时发送已停止：发送内容无效"));
        return;
    }
    m_session->updateAutoSendData(data);
}

void MainWindow::applyFrameSettings()
{
    FrameConfig config;
    config.mode = static_cast<FramingMode>(m_frameModeCombo->currentData().toInt());
    config.maxFrameSize = 64 * 1024;
    config.includeDelimiter = m_includeDelimiterCheck->isChecked();
    if (config.mode == FramingMode::IdleGap) {
        m_frameValueSpin->setRange(1, 60000);
        m_frameValueSpin->setSuffix(QStringLiteral(" ms"));
        config.idleGapMs = m_frameValueSpin->value();
    } else if (config.mode == FramingMode::FixedLength) {
        m_frameValueSpin->setRange(1, 65536);
        m_frameValueSpin->setSuffix(QStringLiteral(" B"));
        config.fixedLength = m_frameValueSpin->value();
    } else {
        bool ok = false;
        config.delimiter = Utils::parseHexString(m_frameDelimiterEdit->text(), &ok);
        if (!ok || config.delimiter.isEmpty()) {
            showStatusMessage(tr("分隔符必须是非空 HEX 字节串"));
            return;
        }
    }
    const bool delimiterMode = config.mode == FramingMode::Delimiter;
    m_frameValueSpin->setEnabled(!delimiterMode);
    m_frameDelimiterEdit->setEnabled(delimiterMode);
    m_includeDelimiterCheck->setEnabled(delimiterMode);
    m_session->setFrameConfig(config);
}

void MainWindow::applyFrameFilter()
{
    FrameFilter filter;
    const int direction = m_filterDirectionCombo->currentData().toInt();
    filter.directionEnabled = direction >= 0;
    if (filter.directionEnabled)
        filter.direction = static_cast<FrameDirection>(direction);
    filter.protocol = m_filterProtocolCombo->currentData().toString();
    filter.station = m_filterStationSpin->value();
    filter.functionCode = m_filterFunctionSpin->value();
    const int crc = m_filterCrcCombo->currentData().toInt();
    filter.crcEnabled = crc >= 0;
    if (filter.crcEnabled)
        filter.crcStatus = static_cast<CrcStatus>(crc);
    filter.keyword = m_filterKeywordEdit->text();
    m_frameProxy->setFrameFilter(filter);
}
