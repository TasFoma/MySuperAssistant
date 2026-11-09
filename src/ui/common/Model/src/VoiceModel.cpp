#include "msa/ui/VoiceModel.h"

#include <QDir>
#include <QUrl>

namespace msa::ui
{
VoiceModel::VoiceModel(QObject* parent) : QObject{parent} {}
QString VoiceModel::modelPath() const
{
    return m_modelPath;
}
QString VoiceModel::draft() const
{
    return m_draft;
}
QString VoiceModel::errorText() const
{
    return m_error;
}
bool VoiceModel::canStart() const
{
    return !m_modelPath.isEmpty() &&
           (m_state == core::RecognitionState::Idle || m_state == core::RecognitionState::Ready ||
            m_state == core::RecognitionState::Failed);
}
bool VoiceModel::canFinish() const
{
    return m_state == core::RecognitionState::Recording;
}
bool VoiceModel::canCancel() const
{
    return m_state == core::RecognitionState::Loading || m_state == core::RecognitionState::Recording ||
           m_state == core::RecognitionState::Finishing;
}
QString VoiceModel::statusText() const
{
    if (m_speaking)
    {
        return "Идёт озвучка сообщения";
    }
    using State = core::RecognitionState;
    switch (m_state)
    {
    case State::Unconfigured:
        return "Выберите папку модели Vosk в настройках.";
    case State::Idle:
        return "Микрофон выключен";
    case State::Loading:
        return "Загрузка модели Vosk…";
    case State::Ready:
        return "Готово к новой записи";
    case State::Recording:
        return "Идёт запись одной фразы, до 30 секунд";
    case State::Finishing:
        return "Завершение записи…";
    case State::Failed:
        return "Распознавание остановлено";
    }
    return {};
}
void VoiceModel::applyModelPath(const QString& path)
{
    if (m_modelPath == path)
    {
        return;
    }
    m_modelPath = path;
    m_state = path.isEmpty() ? core::RecognitionState::Unconfigured : core::RecognitionState::Idle;
    emit modelChanged();
    emit statusChanged();
}
void VoiceModel::setError(const QString& message)
{
    m_error = message;
    emit statusChanged();
}
void VoiceModel::setDraft(const QString& text)
{
    if (m_draft == text)
    {
        return;
    }
    m_draft = text;
    emit draftChanged();
}
void VoiceModel::applyEvent(const core::RecognitionEvent& event)
{
    m_state = event.state;
    if (event.kind == core::RecognitionEvent::Kind::Error)
    {
        m_error = QString::fromStdString(event.text);
    }
    if (event.kind == core::RecognitionEvent::Kind::Text)
    {
        const auto text = QString::fromStdString(event.text).trimmed();
        if (!text.isEmpty())
        {
            setDraft(m_draft.isEmpty() ? text : m_draft + '\n' + text);
        }
    }
    emit statusChanged();
}
void VoiceModel::chooseModel(const QString& path)
{
    auto normalized = path;
    if (!normalized.isEmpty() && !QDir::isAbsolutePath(normalized))
    {
        const QUrl url{normalized};
        if (!url.isLocalFile())
        {
            setError("Выберите локальную папку модели Vosk.");
            return;
        }
        normalized = url.toLocalFile();
    }
    if ((!normalized.isEmpty() && !QDir::isAbsolutePath(normalized)) || normalized.contains(QChar::Null))
    {
        setError("Укажите абсолютный путь к папке модели.");
        return;
    }
    emit saveRequested(normalized.isEmpty() ? QString{} : QDir::cleanPath(normalized));
}
void VoiceModel::start()
{
    if (!canStart())
    {
        return;
    }
    setError({});
    emit commandRequested(core::RecognitionCommand::Start);
}
void VoiceModel::finish()
{
    if (canFinish())
    {
        emit commandRequested(core::RecognitionCommand::Finish);
    }
}
void VoiceModel::cancel()
{
    if (canCancel())
    {
        emit commandRequested(core::RecognitionCommand::Cancel);
    }
}
bool VoiceModel::speaking() const
{
    return m_speaking;
}
void VoiceModel::applySpeechEvent(const core::SpeechEvent& event)
{
    m_speaking = event.state == core::SpeechState::Speaking;
    if (!event.error.empty())
    {
        m_error = QString::fromStdString(event.error);
    }
    emit statusChanged();
}
void VoiceModel::speak(const QString& text)
{
    if (text.trimmed().isEmpty())
    {
        return;
    }
    setError({});
    emit speechRequested(text);
}
void VoiceModel::stopSpeech()
{
    emit stopSpeechRequested();
}
} // namespace msa::ui
