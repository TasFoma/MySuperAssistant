#include "msa/mobile/MobileRuntime.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
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
    parser.process(application);
    if (!parser.positionalArguments().isEmpty())
    {
        qCritical("Используйте --config для указания файла настроек.");
        return 1;
    }
    try
    {
        msa::mobile::MobileRuntime runtime{
            {parser.isSet(config) ? parser.value(config) : msa::mobile::MobileRuntime::defaultSettingsPath()}};
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
