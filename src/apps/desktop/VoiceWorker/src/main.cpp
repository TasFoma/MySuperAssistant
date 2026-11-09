#include "msa/desktop/VoiceWorker.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

int main(int argc, char* argv[])
{
    QCoreApplication application{argc, argv};
    QCoreApplication::setApplicationName("msa_voice_worker");
    QCommandLineParser parser;
    const QCommandLineOption channel{"channel", "Локальный канал клиента.", "name"};
    parser.addOption(channel);
    parser.process(application);
    if (!parser.isSet(channel) || parser.value(channel).isEmpty() || !parser.positionalArguments().isEmpty())
    {
        return 1;
    }
    msa::desktop::VoiceWorker worker;
    worker.connectTo(parser.value(channel));

    return application.exec();
}
