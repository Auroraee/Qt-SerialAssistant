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
    void toHexString_basic();
    void toAsciiDisplayString_withControlChars();
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
    QByteArray result = Utils::parseHexString(QStringLiteral("48 GZ"), &ok);
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

QTEST_APPLESS_MAIN(TestUtils)
#include "test_utils.moc"
