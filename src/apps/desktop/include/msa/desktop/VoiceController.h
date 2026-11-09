#pragma once

#include "msa/core/SpeechRecognizer.h"
#include "msa/ui/VoiceModel.h"

#include <QObject>
#include <QString>

namespace msa::desktop
{
/// @brief Соединяет интерфейс распознавания, модель UI и отдельный JSON папки модели.
class VoiceController final : public QObject
{
public:
    /// @brief Переданные модель и распознаватель должны пережить контроллер.
    VoiceController(ui::VoiceModel& model, core::SpeechRecognizer& recognizer, QString settingsPath,
                    QObject* parent = nullptr);
    ~VoiceController() override;
    /// @brief Читает параметры без загрузки модели Vosk и открытия микрофона.
    void restore();

private:
    void save(const QString& modelPath);

private:
    ui::VoiceModel& m_model;
    core::SpeechRecognizer& m_recognizer;
    QString m_settingsPath;
};
} // namespace msa::desktop
