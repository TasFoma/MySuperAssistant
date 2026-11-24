#include "msa/mobile/MobileRuntime.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QVariant>

#include <exception>

Q_IMPORT_QML_PLUGIN(MsaUiCommonPlugin)
Q_IMPORT_QML_PLUGIN(MsaUiMobilePlugin)

int main(int argc, char* argv[])
{
    QGuiApplication application{argc, argv};
    QCoreApplication::setOrganizationName("MySuperAssistant");
    QCoreApplication::setApplicationName("msa_mobile");
    QGuiApplication::setApplicationDisplayName("MySuperAssistant");
    QCommandLineParser parser;
    parser.setApplicationDescription("Мобильный клиент MySuperAssistant");
    parser.addHelpOption();
    const QCommandLineOption config{"config", "Файл параметров подключения.", "path"};
    parser.addOption(config);
    const QCommandLineOption sportData{"sport-data", "Отдельный файл локальных данных спорта.", "path"};
    parser.addOption(sportData);
    parser.process(application);
    if (!parser.positionalArguments().isEmpty())
    {
        qCritical("Используйте --config для указания файла настроек.");
        return 1;
    }
    try
    {
        msa::mobile::MobileRuntimeOptions options{
            parser.isSet(config) ? parser.value(config) : msa::mobile::MobileRuntime::defaultSettingsPath()};
        options.sportDatabasePath = parser.isSet(sportData) ? parser.value(sportData) :
            QDir{QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)}.filePath("sport/sport-local.sqlite");
        msa::mobile::MobileRuntime runtime{options};
        QQmlApplicationEngine engine;
        engine.setInitialProperties(
            {{"client", QVariant::fromValue(runtime.models())}, {"mobile", QVariant::fromValue(runtime.state())}});
        engine.loadFromModule("Msa.Ui.Mobile", "MobileWindow");
        if (engine.rootObjects().isEmpty())
        {
            qCritical("Не удалось открыть окно клиента.");
            return 1;
        }
        QObject::connect(&application, &QCoreApplication::aboutToQuit, &runtime, [&runtime]() { runtime.stop(); });
        runtime.start();

        return application.exec();
    }
    catch (const std::exception&)
    {
        qCritical("Не удалось запустить клиент. Проверьте настройки и доступность компонентов Qt.");
        return 1;
    }
}
