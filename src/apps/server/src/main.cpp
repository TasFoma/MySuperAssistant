#include "NetworkServices.h"
#include "ServerRuntime.h"
#include "ServerSettings.h"

#include "msa/core/Clock.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QTimeZone>

#include <chrono>
#include <exception>
#include <utility>

namespace {
class ServerClock final : public msa::core::Clock {
public:
    explicit ServerClock(QTimeZone zone);
    [[nodiscard]] std::chrono::local_seconds now() const override;

private:
    QTimeZone m_zone;
};

ServerClock::ServerClock(QTimeZone zone) : m_zone{std::move(zone)}
{
}

std::chrono::local_seconds ServerClock::now() const
{
    const auto current = QDateTime::currentDateTime(m_zone);

    return std::chrono::local_seconds{std::chrono::seconds{current.toSecsSinceEpoch() + current.offsetFromUtc()}};
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application{argc, argv};
    QCoreApplication::setApplicationName("msa_server");
    QCommandLineParser parser;
    parser.setApplicationDescription("Личный сервер MySuperAssistant");
    parser.addHelpOption();
    const QCommandLineOption config{"config", "Путь к JSON-файлу настроек.", "path"};
    parser.addOption(config);
    parser.process(application);
    if (!parser.isSet(config) || !parser.positionalArguments().isEmpty()) {
        qCritical("Укажите файл настроек: msa_server --config <path>");
        return 1;
    }
    try {
        const auto settings = msa::server::readServerSettings(parser.value(config));
        const ServerClock clock{QTimeZone{settings.timeZone.toUtf8()}};
        msa::server::NetworkServices network{settings};
        msa::server::ServerRuntime server{settings, clock,
            {network, network, network, network, network, network.geocodingSourceKey()}};
        if (!server.start()) {
            qCritical("Не удалось открыть TCP-порт сервера.");
            return 1;
        }
        qInfo("Сервер запущен, TCP-порт: %u", static_cast<unsigned>(server.port()));

        return application.exec();
    } catch (const std::exception&) {
        qCritical("Не удалось запустить сервер. Проверьте настройки, доступность файла БД и версию схемы.");
        return 1;
    }
}

