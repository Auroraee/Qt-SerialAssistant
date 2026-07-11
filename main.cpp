#include "mainwindow.h"

#include <QApplication>
#include <QSettings>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("SerialAssistant"));
    QApplication::setApplicationName(QStringLiteral("SerialAssistant"));

    QTranslator translator;
    QSettings settings;
    settings.beginGroup(QStringLiteral("Application"));
    const QString language = settings.value(QStringLiteral("language"),
                                            QStringLiteral("zh_CN")).toString();
    settings.endGroup();
    if (language == QLatin1String("en_US")
        && translator.load(QStringLiteral(":/i18n/SerialAssistant_en_US"))) {
        a.installTranslator(&translator);
    }
    MainWindow w;
    w.show();
    return QApplication::exec();
}
