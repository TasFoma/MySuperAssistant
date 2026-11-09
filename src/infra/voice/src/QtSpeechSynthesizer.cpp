#include "msa/infra/QtSpeechSynthesizer.h"

#include <QLocale>
#include <QTimer>
#include <QVoice>

#include <utility>

namespace msa::infra
{
namespace
{
constexpr double kSpeechRate = 0.2;
constexpr double kSpeechPitch = 0.02;
} // namespace
QtSpeechSynthesizer::QtSpeechSynthesizer(QString engine, QObject* parent) :
    QObject{parent}, m_engineName{std::move(engine)}
{
}
QtSpeechSynthesizer::~QtSpeechSynthesizer()
{
    m_handler = {};
    stop();
}
void QtSpeechSynthesizer::setHandler(core::SpeechHandler handler)
{
    m_handler = std::move(handler);
}
void QtSpeechSynthesizer::notify(core::SpeechState state, const QString& error)
{
    if (m_handler)
    {
        m_handler({state, error.toStdString()});
    }
}
void QtSpeechSynthesizer::initialize()
{
    m_engine = std::make_unique<QTextToSpeech>(m_engineName);
    connect(m_engine.get(), &QTextToSpeech::errorOccurred, this,
            [this](QTextToSpeech::ErrorReason reason, const QString& message)
            {
                if (reason != QTextToSpeech::ErrorReason::NoError && m_requested)
                {
                    m_requested = false;
                    m_pending.clear();
                    notify(core::SpeechState::Failed, "Не удалось озвучить текст: " + message);
                }
            });
    connect(m_engine.get(), &QTextToSpeech::stateChanged, this,
            [this](QTextToSpeech::State state)
            {
                if (state == QTextToSpeech::Speaking && m_requested)
                {
                    m_playing = true;
                }
                if (state == QTextToSpeech::Ready && m_requested)
                {
                    if (m_playing && m_pending.isEmpty())
                    {
                        m_requested = false;
                        m_playing = false;
                        notify(core::SpeechState::Idle);
                        return;
                    }
                    // Не вызываем say() из сигнала Ready: движок может ещё завершать stop().
                    QTimer::singleShot(0, this, [this] { ready(); });
                }
            });
}
void QtSpeechSynthesizer::speak(const std::string& text)
{
    const auto phrase = QString::fromStdString(text).trimmed();
    if (phrase.isEmpty() || phrase.contains(QChar::Null))
    {
        stop();
        notify(core::SpeechState::Failed, "Нет корректного текста для озвучки.");
        return;
    }
    stop();
    if (!m_engine)
    {
        initialize();
    }
    if (m_engine->state() == QTextToSpeech::Error)
    {
        notify(core::SpeechState::Failed, "Не удалось включить озвучку: " + m_engine->errorString());
        return;
    }
    m_pending = phrase;
    m_requested = true;
    notify(core::SpeechState::Speaking);
    QTimer::singleShot(0, this, [this] { ready(); });
}
void QtSpeechSynthesizer::ready()
{
    if (!m_requested || !m_engine || m_engine->state() != QTextToSpeech::Ready)
    {
        return;
    }
    if (m_pending.isEmpty())
    {
        // Ready от настройки голоса не означает окончания ещё не начавшейся речи.
        return;
    }
    auto voices = m_engine->findVoices(QLocale::Russian, QVoice::Male);
    if (voices.isEmpty())
    {
        voices = m_engine->findVoices(QVoice::Male);
    }
    if (voices.isEmpty())
    {
        voices = m_engine->findVoices(QLocale::Russian);
    }
    if (voices.isEmpty())
    {
        voices = m_engine->findVoices();
    }
    if (voices.isEmpty())
    {
        m_requested = false;
        m_pending.clear();
        notify(core::SpeechState::Failed, "В системе нет доступного голоса для озвучки.");
        return;
    }
    const auto voice = voices.first();
    m_engine->setLocale(voice.locale());
    m_engine->setVoice(voice);
    m_engine->setRate(kSpeechRate);
    m_engine->setPitch(kSpeechPitch);
    if (!m_requested || m_engine->state() == QTextToSpeech::Error)
    {
        return;
    }
    const auto phrase = std::exchange(m_pending, {});
    m_playing = false;
    m_engine->say(phrase);
}
void QtSpeechSynthesizer::stop()
{
    const auto wasRequested = std::exchange(m_requested, false);
    m_playing = false;
    m_pending.clear();
    if (m_engine)
    {
        m_engine->stop(QTextToSpeech::BoundaryHint::Immediate);
    }
    if (wasRequested)
    {
        notify(core::SpeechState::Idle);
    }
}
} // namespace msa::infra
