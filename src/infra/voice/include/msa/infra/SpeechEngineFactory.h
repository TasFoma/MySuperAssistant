#pragma once

#include <QAudioDevice>
#include <QString>
#include <QTextToSpeech>

#include <memory>

namespace msa::infra
{
/// @brief Создаёт платформенный движок с выбранным выводом; неподдерживаемый выбор отклоняется исключением.
/// @param engine Штатное имя движка для системного вывода, пустое означает выбор Qt.
/// @param output Явно выбранное устройство, используется при systemDefault=false.
/// @param systemDefault Сохранить выбор движка/вывода операционной системой.
[[nodiscard]] std::unique_ptr<QTextToSpeech> createSpeechEngine(const QString& engine, const QAudioDevice& output,
                                                                bool systemDefault);
} // namespace msa::infra
