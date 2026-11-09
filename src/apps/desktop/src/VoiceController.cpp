#include "msa/desktop/VoiceController.h"

#include "msa/infra/JsonVoicePreferences.h"

#include <QDir>
#include <QFileInfo>

#include <exception>
#include <utility>

namespace msa::desktop
{
VoiceController::VoiceController(ui::VoiceModel& model, core::SpeechRecognizer& recognizer, QString settingsPath,
                                 QObject* parent) :
    QObject{parent}, m_model{model}, m_recognizer{recognizer}, m_settingsPath{std::move(settingsPath)}
{
    recognizer.setHandler([this](const core::RecognitionEvent& event) { m_model.applyEvent(event); });
    connect(&model, &ui::VoiceModel::saveRequested, this, [this](const QString& path) { save(path); });
    connect(&model, &ui::VoiceModel::commandRequested, this,
            [this](core::RecognitionCommand command)
            {
                switch (command)
                {
                case core::RecognitionCommand::Start:
                    m_recognizer.start();
                    break;
                case core::RecognitionCommand::Finish:
                    m_recognizer.finish();
                    break;
                case core::RecognitionCommand::Cancel:
                    m_recognizer.cancel();
                    break;
                }
            });
}
VoiceController::~VoiceController()
{
    m_recognizer.setHandler({});
}
void VoiceController::restore()
{
    try
    {
        const auto path = infra::JsonVoicePreferences::load(m_settingsPath).value_or(QString{});
        m_model.applyModelPath(path);
        m_recognizer.configure(path.toStdString());
        m_model.setError({});
    }
    catch (const std::exception&)
    {
        m_model.setError("Не удалось прочитать параметры голоса. Выберите модель заново.");
    }
}
void VoiceController::save(const QString& modelPath)
{
    try
    {
        if (!QDir{}.mkpath(QFileInfo{m_settingsPath}.absolutePath()))
        {
            m_model.setError("Не удалось создать папку параметров голоса.");
            return;
        }
        infra::JsonVoicePreferences::save(m_settingsPath, modelPath);
        m_model.applyModelPath(modelPath);
        m_recognizer.configure(modelPath.toStdString());
        m_model.setError({});
    }
    catch (const std::exception&)
    {
        m_model.setError("Не удалось сохранить параметры голоса. Прежние значения сохранены.");
    }
}
} // namespace msa::desktop
