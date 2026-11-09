#include "msa/desktop/DesktopRuntime.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QVariant>

#include <exception>
#include <random>

Q_IMPORT_QML_PLUGIN(MsaUiCommonPlugin)
Q_IMPORT_QML_PLUGIN(MsaUiDesktopPlugin)

int main(int argc, char* argv[])
{
    QGuiApplication application{argc, argv};
    QCoreApplication::setOrganizationName("MySuperAssistant");
    QCoreApplication::setApplicationName("msa_desktop");
    QGuiApplication::setApplicationDisplayName("MySuperAssistant");
    QCommandLineParser parser;
    parser.setApplicationDescription("Клиент MySuperAssistant для ПК");
    parser.addHelpOption();
    const QCommandLineOption config{"config", "Файл параметров подключения.", "path"};
    const QCommandLineOption musicConfig{"music-config", "Отдельный файл параметров музыки.", "path"};
    parser.addOptions({config, musicConfig});
    parser.process(application);
    if (!parser.positionalArguments().isEmpty())
    {
        qCritical("Используйте --config и --music-config для указания файлов настроек.");
        return 1;
    }
    try
    {
        msa::desktop::DesktopRuntime runtime{
            {parser.isSet(config) ? parser.value(config) : msa::desktop::DesktopPreferencesController::defaultPath(),
             parser.isSet(musicConfig) ? parser.value(musicConfig)
                                       : msa::desktop::MusicController::defaultSettingsPath(),
             std::random_device{}()}};
        QQmlApplicationEngine engine;
        engine.setInitialProperties(
            {{"client", QVariant::fromValue(runtime.models())}, {"desktop", QVariant::fromValue(runtime.state())}});
        engine.loadFromModule("Msa.Ui.Desktop", "DesktopWindow");
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
