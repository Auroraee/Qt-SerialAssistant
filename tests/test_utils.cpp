#include <QtTest>
#include "../utils.h"

class TestUtils : public QObject
{
    Q_OBJECT

private slots:
    void parseHexString_validWithSpaces();
    void parseHexString_noSpaces();
    void parseHexString_empty();
    void parseHexString_oddLength();
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

QTEST_APPLESS_MAIN(TestUtils)
#include "test_utils.moc"
