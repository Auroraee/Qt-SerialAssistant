#include <QtTest>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTableView>
#include <QVBoxLayout>
#include "../asyncwritequeue.h"
#include "../frameassembler.h"
#include "../framelogmodel.h"
#include "../modbusrtudecoder.h"
#include "../mainwindow.h"
#include "../serialportmanager.h"
#include "../serialsessioncontroller.h"
#include "../utils.h"

namespace {
QByteArray withModbusCrc(QByteArray body)
{
    const quint16 crc = Utils::crc16Modbus(body);
    body.append(static_cast<char>(crc & 0xFF));
    body.append(static_cast<char>((crc >> 8) & 0xFF));
    return body;
}

class PartialWriteDevice : public QIODevice
{
public:
    explicit PartialWriteDevice(qint64 maxChunk, QObject *parent = nullptr)
        : QIODevice(parent)
        , m_maxChunk(maxChunk)
    {
        open(QIODevice::ReadWrite);
    }

    QByteArray submittedData() const { return m_submittedData; }
    void acknowledge(qint64 bytes) { emit bytesWritten(bytes); }

protected:
    qint64 readData(char *, qint64) override { return -1; }
    qint64 writeData(const char *data, qint64 maxSize) override
    {
        const qint64 written = qMin(maxSize, m_maxChunk);
        m_submittedData.append(data, written);
        return written;
    }

private:
    qint64 m_maxChunk;
    QByteArray m_submittedData;
};
}

class TestUtils : public QObject
{
    Q_OBJECT

private slots:
    void parseHexString_validWithSpaces();
    void parseHexString_noSpaces();
    void parseHexString_empty();
    void parseHexString_oddLength();
    void parseHexString_withPrefixes();
    void parseHexString_invalidPrefixOnly();
    void parseHexString_invalidChar();
    void parseHexString_noOkParam();
    void parseHexString_lowerCase();
    void parseHexString_leadingTrailingSpaces();
    void toHexString_basic();
    void toHexString_empty();
    void toHexString_binaryData();
    void toAsciiDisplayString_withControlChars();
    void toAsciiDisplayString_empty();
    void toAsciiDisplayString_boundaries();
    void toAsciiDisplayString_nonAscii();
    void crc16Modbus_standardVector();
    void crc16Modbus_empty();
    void parityBits_evenOnes();
    void parityBits_oddOnes();
    void frameAssembler_fixedLengthHandlesSplitAndMergedInput();
    void frameAssembler_delimiterAcrossChunks();
    void frameAssembler_idleGapFlushesBufferedFrame();
    void frameAssembler_oversizeClearsBufferAndRecovers();
    void frameAssembler_delimitedOversizeFrameIsRejected();
    void modbusDecoder_supportedReadRequests_data();
    void modbusDecoder_supportedReadRequests();
    void modbusDecoder_supportedWriteRequests_data();
    void modbusDecoder_supportedWriteRequests();
    void modbusDecoder_readResponse();
    void modbusDecoder_exceptionResponse();
    void modbusDecoder_badCrcIsReported();
    void modbusDecoder_infersReadRoleFromFrameShape();
    void frameLogModel_evictsOldestByRowAndByteLimits();
    void frameLogFilter_combinesProtocolMetadataAndKeyword();
    void frameLogModel_exportsTextCsvAndJsonLines();
    void asyncWriteQueue_partialWritesCompleteAfterAcknowledgement();
    void asyncWriteQueue_preservesRequestOrder();
    void asyncWriteQueue_abortFailsPendingRequests();
    void asyncWriteQueue_timesOutWithoutProgress();
    void serialSessionController_idleGapDecodesReceivedFrame();
    void serialSessionController_logsAssemblyErrors();
    void serialSessionController_tracksConnectionState();
    void mainWindow_protocolWorkbenchAndConnectionState();
};

void TestUtils::parseHexString_validWithSpaces()
{
    bool ok = false;
    QByteArray result = Utils::parseHexString(QStringLiteral("48 65 6C 6C 6F"), &ok);
    QVERIFY(ok);
    QCOMPARE(result, QByteArray("Hello"));
}

void TestUtils::parseHexString_noSpaces()
{
    bool ok = false;
    QByteArray result = Utils::parseHexString(QStringLiteral("48656C6C6F"), &ok);
    QVERIFY(ok);
    QCOMPARE(result, QByteArray("Hello"));
}

void TestUtils::parseHexString_empty()
{
    bool ok = true;
    QByteArray result = Utils::parseHexString(QStringLiteral(""), &ok);
    QVERIFY(ok);
    QCOMPARE(result, QByteArray());
}

void TestUtils::parseHexString_oddLength()
{
    bool ok = true;
    QByteArray result = Utils::parseHexString(QStringLiteral("486"), &ok);
    QVERIFY(ok);
    QCOMPARE(result, QByteArray::fromHex("0486"));
}

void TestUtils::parseHexString_withPrefixes()
{
    bool ok = false;
    QByteArray result = Utils::parseHexString(QStringLiteral("0xF 0X10"), &ok);
    QVERIFY(ok);
    QCOMPARE(result, QByteArray::fromHex("0F10"));
}

void TestUtils::parseHexString_invalidPrefixOnly()
{
    bool ok = true;
    QByteArray result = Utils::parseHexString(QStringLiteral("0x"), &ok);
    QVERIFY(!ok);
    QCOMPARE(result, QByteArray());
}

void TestUtils::parseHexString_invalidChar()
{
    bool ok = true;
    QByteArray result = Utils::parseHexString(QStringLiteral("48 XX 65"), &ok);
    QVERIFY(!ok);
    QCOMPARE(result, QByteArray());
}

void TestUtils::toHexString_basic()
{
    QString result = Utils::toHexString(QByteArray("Hello"));
    QCOMPARE(result, QStringLiteral("48 65 6C 6C 6F"));
}

void TestUtils::toAsciiDisplayString_withControlChars()
{
    QByteArray input;
    input.append('A');
    input.append('\x00');
    input.append('\x7F');
    QString result = Utils::toAsciiDisplayString(input);
    QCOMPARE(result, QStringLiteral("A.."));
}

void TestUtils::parseHexString_noOkParam()
{
    QByteArray result = Utils::parseHexString(QStringLiteral("48 65 6C 6C 6F"));
    QCOMPARE(result, QByteArray("Hello"));
}

void TestUtils::parseHexString_lowerCase()
{
    bool ok = false;
    QByteArray result = Utils::parseHexString(QStringLiteral("48 65 6c 6c 6f"), &ok);
    QVERIFY(ok);
    QCOMPARE(result, QByteArray("Hello"));
}

void TestUtils::parseHexString_leadingTrailingSpaces()
{
    bool ok = false;
    QByteArray result = Utils::parseHexString(QStringLiteral(" 48 65 6C 6C 6F "), &ok);
    QVERIFY(ok);
    QCOMPARE(result, QByteArray("Hello"));
}

void TestUtils::toHexString_empty()
{
    QString result = Utils::toHexString(QByteArray());
    QCOMPARE(result, QStringLiteral(""));
}

void TestUtils::toHexString_binaryData()
{
    QString result = Utils::toHexString(QByteArray("\x00\x01\xFF", 3));
    QCOMPARE(result, QStringLiteral("00 01 FF"));
}

void TestUtils::toAsciiDisplayString_empty()
{
    QString result = Utils::toAsciiDisplayString(QByteArray());
    QCOMPARE(result, QStringLiteral(""));
}

void TestUtils::toAsciiDisplayString_boundaries()
{
    QString result = Utils::toAsciiDisplayString(QByteArray("\x20\x7E", 2));
    QCOMPARE(result, QStringLiteral(" ~"));
}

void TestUtils::toAsciiDisplayString_nonAscii()
{
    QString result = Utils::toAsciiDisplayString(QByteArray("\x80\xFF", 2));
    QCOMPARE(result, QStringLiteral(".."));
}

void TestUtils::crc16Modbus_standardVector()
{
    QCOMPARE(Utils::crc16Modbus(QByteArray("123456789")), quint16(0x4B37));
}

void TestUtils::crc16Modbus_empty()
{
    QCOMPARE(Utils::crc16Modbus(QByteArray()), quint16(0xFFFF));
}

void TestUtils::parityBits_evenOnes()
{
    const QByteArray input = QByteArray::fromHex("55");
    QCOMPARE(Utils::bitCount(input), 4);
    QCOMPARE(Utils::evenParityBit(input), 0);
    QCOMPARE(Utils::oddParityBit(input), 1);
}

void TestUtils::parityBits_oddOnes()
{
    const QByteArray input = QByteArray::fromHex("07");
    QCOMPARE(Utils::bitCount(input), 3);
    QCOMPARE(Utils::evenParityBit(input), 1);
    QCOMPARE(Utils::oddParityBit(input), 0);
}

void TestUtils::frameAssembler_fixedLengthHandlesSplitAndMergedInput()
{
    FrameConfig config;
    config.mode = FramingMode::FixedLength;
    config.fixedLength = 3;
    FrameAssembler assembler(config);

    QCOMPARE(assembler.append(QByteArray::fromHex("0102")).frames.size(), 0);
    const FrameAssemblyResult result = assembler.append(QByteArray::fromHex("0304050607"));
    QCOMPARE(result.frames,
             QList<QByteArray>({QByteArray::fromHex("010203"), QByteArray::fromHex("040506")}));
    QCOMPARE(assembler.bufferedData(), QByteArray::fromHex("07"));
}

void TestUtils::frameAssembler_delimiterAcrossChunks()
{
    FrameConfig config;
    config.mode = FramingMode::Delimiter;
    config.delimiter = QByteArray::fromHex("0D0A");
    config.includeDelimiter = false;
    FrameAssembler assembler(config);

    QCOMPARE(assembler.append(QByteArray("ABC\r")).frames.size(), 0);
    const FrameAssemblyResult result = assembler.append(QByteArray("\nDEF\r\n"));
    QCOMPARE(result.frames, QList<QByteArray>({QByteArray("ABC"), QByteArray("DEF")}));
    QVERIFY(assembler.bufferedData().isEmpty());
}

void TestUtils::frameAssembler_idleGapFlushesBufferedFrame()
{
    FrameConfig config;
    config.mode = FramingMode::IdleGap;
    FrameAssembler assembler(config);

    QCOMPARE(assembler.append(QByteArray::fromHex("010203")).frames.size(), 0);
    const FrameAssemblyResult result = assembler.flush();
    QCOMPARE(result.frames, QList<QByteArray>({QByteArray::fromHex("010203")}));
    QVERIFY(assembler.bufferedData().isEmpty());
}

void TestUtils::frameAssembler_oversizeClearsBufferAndRecovers()
{
    FrameConfig config;
    config.mode = FramingMode::IdleGap;
    config.maxFrameSize = 4;
    FrameAssembler assembler(config);

    const FrameAssemblyResult oversized = assembler.append(QByteArray::fromHex("0102030405"));
    QVERIFY(!oversized.error.isEmpty());
    QVERIFY(assembler.bufferedData().isEmpty());

    assembler.append(QByteArray::fromHex("AABB"));
    QCOMPARE(assembler.flush().frames, QList<QByteArray>({QByteArray::fromHex("AABB")}));
}

void TestUtils::frameAssembler_delimitedOversizeFrameIsRejected()
{
    FrameConfig config;
    config.mode = FramingMode::Delimiter;
    config.delimiter = QByteArray("\n");
    config.maxFrameSize = 4;
    FrameAssembler assembler(config);

    const FrameAssemblyResult oversized = assembler.append(QByteArray("12345\n"));
    QVERIFY(!oversized.error.isEmpty());
    QVERIFY(oversized.frames.isEmpty());
    QVERIFY(assembler.bufferedData().isEmpty());

    QCOMPARE(assembler.append(QByteArray("OK\n")).frames,
             QList<QByteArray>({QByteArray("OK")}));
}

void TestUtils::modbusDecoder_supportedReadRequests_data()
{
    QTest::addColumn<int>("functionCode");
    QTest::newRow("read-coils") << 0x01;
    QTest::newRow("read-discrete-inputs") << 0x02;
    QTest::newRow("read-holding-registers") << 0x03;
    QTest::newRow("read-input-registers") << 0x04;
}

void TestUtils::modbusDecoder_supportedReadRequests()
{
    QFETCH(int, functionCode);
    QByteArray body;
    body.append(char(0x01));
    body.append(char(functionCode));
    body.append(QByteArray::fromHex("00100002"));

    const DecodeResult result = ModbusRtuDecoder::decode(withModbusCrc(body), FrameDirection::Tx);
    QCOMPARE(result.protocol, QStringLiteral("Modbus RTU"));
    QCOMPARE(result.crcStatus, CrcStatus::Valid);
    QCOMPARE(result.fields.value(QStringLiteral("role")).toString(), QStringLiteral("request"));
    QCOMPARE(result.fields.value(QStringLiteral("functionCode")).toInt(), functionCode);
    QCOMPARE(result.fields.value(QStringLiteral("startAddress")).toInt(), 0x10);
    QCOMPARE(result.fields.value(QStringLiteral("quantity")).toInt(), 2);
}

void TestUtils::modbusDecoder_supportedWriteRequests_data()
{
    QTest::addColumn<int>("functionCode");
    QTest::addColumn<QByteArray>("payload");
    QTest::newRow("write-coil") << 0x05 << QByteArray::fromHex("0010FF00");
    QTest::newRow("write-register") << 0x06 << QByteArray::fromHex("0010002A");
    QTest::newRow("write-coils") << 0x0F << QByteArray::fromHex("0010000A02CD01");
    QTest::newRow("write-registers") << 0x10 << QByteArray::fromHex("001000020400010002");
}

void TestUtils::modbusDecoder_supportedWriteRequests()
{
    QFETCH(int, functionCode);
    QFETCH(QByteArray, payload);
    QByteArray body;
    body.append(char(0x01));
    body.append(char(functionCode));
    body.append(payload);

    const DecodeResult result = ModbusRtuDecoder::decode(withModbusCrc(body), FrameDirection::Tx);
    QCOMPARE(result.protocol, QStringLiteral("Modbus RTU"));
    QCOMPARE(result.crcStatus, CrcStatus::Valid);
    QCOMPARE(result.fields.value(QStringLiteral("functionCode")).toInt(), functionCode);
    QCOMPARE(result.fields.value(QStringLiteral("role")).toString(), QStringLiteral("request"));
    QVERIFY(result.error.isEmpty());
}

void TestUtils::modbusDecoder_readResponse()
{
    const QByteArray frame = withModbusCrc(QByteArray::fromHex("010306022B00000064"));
    const DecodeResult result = ModbusRtuDecoder::decode(frame, FrameDirection::Rx);

    QCOMPARE(result.crcStatus, CrcStatus::Valid);
    QCOMPARE(result.fields.value(QStringLiteral("role")).toString(), QStringLiteral("response"));
    QCOMPARE(result.fields.value(QStringLiteral("byteCount")).toInt(), 6);
    QCOMPARE(result.fields.value(QStringLiteral("valuesHex")).toString(), QStringLiteral("02 2B 00 00 00 64"));
}

void TestUtils::modbusDecoder_exceptionResponse()
{
    const DecodeResult result = ModbusRtuDecoder::decode(
        withModbusCrc(QByteArray::fromHex("018302")), FrameDirection::Rx);

    QCOMPARE(result.crcStatus, CrcStatus::Valid);
    QCOMPARE(result.fields.value(QStringLiteral("role")).toString(), QStringLiteral("exception"));
    QCOMPARE(result.fields.value(QStringLiteral("functionCode")).toInt(), 0x03);
    QCOMPARE(result.fields.value(QStringLiteral("exceptionCode")).toInt(), 0x02);
}

void TestUtils::modbusDecoder_badCrcIsReported()
{
    const DecodeResult result = ModbusRtuDecoder::decode(
        QByteArray::fromHex("0103001000020000"), FrameDirection::Tx);

    QCOMPARE(result.protocol, QStringLiteral("Modbus RTU"));
    QCOMPARE(result.crcStatus, CrcStatus::Invalid);
    QVERIFY(!result.error.isEmpty());
}

void TestUtils::modbusDecoder_infersReadRoleFromFrameShape()
{
    const DecodeResult receivedRequest = ModbusRtuDecoder::decode(
        withModbusCrc(QByteArray::fromHex("010300100002")), FrameDirection::Rx);
    QCOMPARE(receivedRequest.fields.value(QStringLiteral("role")).toString(),
             QStringLiteral("request"));

    const DecodeResult sentResponse = ModbusRtuDecoder::decode(
        withModbusCrc(QByteArray::fromHex("01030400010002")), FrameDirection::Tx);
    QCOMPARE(sentResponse.fields.value(QStringLiteral("role")).toString(),
             QStringLiteral("response"));
}

void TestUtils::frameLogModel_evictsOldestByRowAndByteLimits()
{
    FrameLogModel model(nullptr, 2, 7);
    FrameRecord first;
    first.rawData = QByteArray("1111");
    FrameRecord second;
    second.rawData = QByteArray("2222");
    FrameRecord third;
    third.rawData = QByteArray("33");

    model.appendBatch({first, second, third});

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.rawByteCount(), qint64(6));
    QCOMPARE(model.recordAt(0).rawData, QByteArray("2222"));
    QCOMPARE(model.recordAt(1).rawData, QByteArray("33"));
}

void TestUtils::frameLogFilter_combinesProtocolMetadataAndKeyword()
{
    FrameLogModel model;
    FrameRecord matching;
    matching.direction = FrameDirection::Rx;
    matching.protocol = QStringLiteral("Modbus RTU");
    matching.summary = QStringLiteral("temperature register");
    matching.crcStatus = CrcStatus::Valid;
    matching.fields.insert(QStringLiteral("station"), 1);
    matching.fields.insert(QStringLiteral("functionCode"), 3);
    matching.rawData = QByteArray::fromHex("0103");

    FrameRecord other = matching;
    other.direction = FrameDirection::Tx;
    other.fields.insert(QStringLiteral("station"), 2);
    other.summary = QStringLiteral("pressure register");
    model.appendBatch({matching, other});

    FrameFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    FrameFilter filter;
    filter.directionEnabled = true;
    filter.direction = FrameDirection::Rx;
    filter.protocol = QStringLiteral("Modbus RTU");
    filter.station = 1;
    filter.functionCode = 3;
    filter.crcEnabled = true;
    filter.crcStatus = CrcStatus::Valid;
    filter.keyword = QStringLiteral("temperature");
    proxy.setFrameFilter(filter);

    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, FrameLogModel::SummaryColumn).data().toString(),
             QStringLiteral("temperature register"));
}

void TestUtils::frameLogModel_exportsTextCsvAndJsonLines()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FrameLogModel model;
    FrameRecord record;
    record.timestamp = QDateTime::fromString(QStringLiteral("2026-07-11T10:20:30.123+08:00"),
                                             Qt::ISODateWithMs);
    record.direction = FrameDirection::Rx;
    record.rawData = QByteArray("\0\xFF,A", 4);
    record.protocol = QStringLiteral("Raw");
    record.summary = QStringLiteral("binary,frame");
    model.append(record);

    QString error;
    const QString textPath = directory.filePath(QStringLiteral("frames.txt"));
    const QString csvPath = directory.filePath(QStringLiteral("frames.csv"));
    const QString jsonPath = directory.filePath(QStringLiteral("frames.jsonl"));
    QVERIFY2(model.exportToFile(textPath, LogExportFormat::Text, &error), qPrintable(error));
    QVERIFY2(model.exportToFile(csvPath, LogExportFormat::Csv, &error), qPrintable(error));
    QVERIFY2(model.exportToFile(jsonPath, LogExportFormat::JsonLines, &error), qPrintable(error));

    QFile jsonFile(jsonPath);
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(jsonFile.readLine());
    QCOMPARE(QByteArray::fromHex(document.object().value(QStringLiteral("rawHex")).toString().toLatin1()),
             record.rawData);

    QFile csvFile(csvPath);
    QVERIFY(csvFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(csvFile.readAll().contains("\"binary,frame\""));

    QFile textFile(textPath);
    QVERIFY(textFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(textFile.readAll().contains("00 FF 2C 41"));
}

void TestUtils::asyncWriteQueue_partialWritesCompleteAfterAcknowledgement()
{
    PartialWriteDevice device(2);
    AsyncWriteQueue queue(&device);
    QSignalSpy completed(&queue, &AsyncWriteQueue::writeCompleted);

    const quint64 id = queue.enqueue(QByteArray("ABCDE"));
    QCOMPARE(device.submittedData(), QByteArray("AB"));
    QCOMPARE(completed.count(), 0);

    device.acknowledge(2);
    QCOMPARE(device.submittedData(), QByteArray("ABCD"));
    device.acknowledge(2);
    QCOMPARE(device.submittedData(), QByteArray("ABCDE"));
    device.acknowledge(1);

    QCOMPARE(completed.count(), 1);
    QCOMPARE(completed.at(0).at(0).toULongLong(), id);
    QCOMPARE(completed.at(0).at(1).toLongLong(), qint64(5));
}

void TestUtils::asyncWriteQueue_preservesRequestOrder()
{
    PartialWriteDevice device(16);
    AsyncWriteQueue queue(&device);
    QSignalSpy completed(&queue, &AsyncWriteQueue::writeCompleted);

    const quint64 first = queue.enqueue(QByteArray("ONE"));
    const quint64 second = queue.enqueue(QByteArray("TWO"));
    QCOMPARE(device.submittedData(), QByteArray("ONE"));

    device.acknowledge(3);
    QCOMPARE(device.submittedData(), QByteArray("ONETWO"));
    device.acknowledge(3);

    QCOMPARE(completed.count(), 2);
    QCOMPARE(completed.at(0).at(0).toULongLong(), first);
    QCOMPARE(completed.at(1).at(0).toULongLong(), second);
}

void TestUtils::asyncWriteQueue_abortFailsPendingRequests()
{
    PartialWriteDevice device(16);
    AsyncWriteQueue queue(&device);
    QSignalSpy failed(&queue, &AsyncWriteQueue::writeFailed);

    queue.enqueue(QByteArray("ONE"));
    queue.enqueue(QByteArray("TWO"));
    queue.abortAll(QStringLiteral("disconnected"));

    QCOMPARE(failed.count(), 2);
    QCOMPARE(failed.at(0).at(1).toString(), QStringLiteral("disconnected"));
    QCOMPARE(queue.pendingCount(), 0);
}

void TestUtils::asyncWriteQueue_timesOutWithoutProgress()
{
    PartialWriteDevice device(16);
    AsyncWriteQueue queue(&device);
    queue.setTimeoutMs(20);
    QSignalSpy failed(&queue, &AsyncWriteQueue::writeFailed);

    queue.enqueue(QByteArray("WAIT"));

    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 250);
    QVERIFY(!failed.at(0).at(1).toString().isEmpty());
    QCOMPARE(queue.pendingCount(), 0);
}

void TestUtils::serialSessionController_idleGapDecodesReceivedFrame()
{
    SerialPortManager manager;
    SerialSessionController controller(&manager);
    FrameConfig config;
    config.mode = FramingMode::IdleGap;
    config.idleGapMs = 5;
    controller.setFrameConfig(config);

    const QByteArray frame = withModbusCrc(QByteArray::fromHex("010302002A"));
    manager.dataReceived(frame.left(3));
    manager.dataReceived(frame.mid(3));

    QTRY_COMPARE_WITH_TIMEOUT(controller.logModel()->rowCount(), 1, 500);
    const FrameRecord record = controller.logModel()->recordAt(0);
    QCOMPARE(record.direction, FrameDirection::Rx);
    QCOMPARE(record.rawData, frame);
    QCOMPARE(record.protocol, QStringLiteral("Modbus RTU"));
    QCOMPARE(record.crcStatus, CrcStatus::Valid);
    QCOMPARE(controller.rxBytes(), qint64(frame.size()));
}

void TestUtils::serialSessionController_logsAssemblyErrors()
{
    SerialPortManager manager;
    SerialSessionController controller(&manager);
    FrameConfig config;
    config.mode = FramingMode::IdleGap;
    config.maxFrameSize = 4;
    controller.setFrameConfig(config);

    manager.dataReceived(QByteArray::fromHex("0102030405"));

    QTRY_COMPARE_WITH_TIMEOUT(controller.logModel()->rowCount(), 1, 500);
    const FrameRecord record = controller.logModel()->recordAt(0);
    QCOMPARE(record.direction, FrameDirection::Error);
    QVERIFY(!record.error.isEmpty());
}

void TestUtils::serialSessionController_tracksConnectionState()
{
    SerialPortManager manager;
    SerialSessionController controller(&manager);
    QSignalSpy stateChanged(&controller, &SerialSessionController::stateChanged);

    manager.connectionStateChanged(true);
    QCOMPARE(controller.state(), ConnectionState::Connected);
    manager.connectionStateChanged(false);
    QCOMPARE(controller.state(), ConnectionState::Disconnected);
    QCOMPARE(stateChanged.count(), 2);
}

void TestUtils::mainWindow_protocolWorkbenchAndConnectionState()
{
    MainWindow window;
    auto *frameTable = window.findChild<QTableView *>(QStringLiteral("frameTable"));
    auto *frameMode = window.findChild<QComboBox *>(QStringLiteral("frameModeCombo"));
    auto *languageCombo = window.findChild<QComboBox *>(QStringLiteral("comboBoxLanguage"));
    auto *configLayout = window.findChild<QVBoxLayout *>(QStringLiteral("configLayout"));
    auto *sendButton = window.findChild<QPushButton *>(QStringLiteral("pushButtonSend"));
    auto *rawView = window.findChild<QPlainTextEdit *>(QStringLiteral("plainTextEditReceive"));
    auto *controller = window.findChild<SerialSessionController *>();

    QVERIFY(frameTable);
    QVERIFY(frameMode);
    QVERIFY(languageCombo);
    QCOMPARE(languageCombo->count(), 2);
    QCOMPARE(languageCombo->itemData(0).toString(), QStringLiteral("zh_CN"));
    QCOMPARE(languageCombo->itemData(1).toString(), QStringLiteral("en_US"));
    QVERIFY(configLayout);
    QCOMPARE(configLayout->indexOf(languageCombo), -1);
    QVERIFY(window.statusBar()->isAncestorOf(languageCombo));
    QVERIFY(sendButton);
    QVERIFY(rawView);
    QVERIFY(controller);
    QCOMPARE(rawView->document()->maximumBlockCount(), 10000);
    QVERIFY(!sendButton->isEnabled());

    controller->portManager()->connectionStateChanged(true);
    QVERIFY(sendButton->isEnabled());
    controller->portManager()->connectionStateChanged(false);
    QVERIFY(!sendButton->isEnabled());
}

QTEST_MAIN(TestUtils)
#include "test_utils.moc"
