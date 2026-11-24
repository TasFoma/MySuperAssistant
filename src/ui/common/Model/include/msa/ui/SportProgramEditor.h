#pragma once

#include "msa/ui/SportFormData.h"

#include <QObject>

namespace msa::ui
{
/// @brief Неполный редактор программы; сохранение выполняет apps по сигналам.
class SportProgramEditor final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QVariantList fields READ fields NOTIFY changed)
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY sessionsChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(int sessionIndex READ sessionIndex NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    explicit SportProgramEditor(QObject* parent = nullptr);
    /// @brief Открывает новый/восстановленный черновик с ожидаемой редакцией программы.
    void open(core::SportProgramCheckpoint checkpoint, std::optional<std::string> expectedRevision);
    [[nodiscard]] bool active() const;
    [[nodiscard]] bool saved() const;
    [[nodiscard]] QVariantList fields() const;
    [[nodiscard]] QVariantList sessions() const;
    [[nodiscard]] QVariantList items() const;
    [[nodiscard]] int sessionIndex() const;
    [[nodiscard]] QString status() const;
    [[nodiscard]] const core::SportProgramCheckpoint& checkpoint() const;
    [[nodiscard]] const std::optional<std::string>& expectedRevision() const;
    /// @brief Разбирает сырой ввод и проверяет готовность всех занятий.
    [[nodiscard]] core::SportResult<core::SportProgramRevision> candidate() const;
    void acknowledge(std::uint64_t sequence);
    void reportError(const QString& message);
    void addSession(const std::string& identifier);
    void addExercise(const core::SportExerciseRevision& exercise, const std::string& itemIdentifier,
                     const std::string& setIdentifier);
    void addSet(const QString& itemIdentifier, const std::string& setIdentifier);
    Q_INVOKABLE void selectSession(int index);
    Q_INVOKABLE void setField(const QString& key, const QString& value);
    Q_INVOKABLE void removeItem(const QString& identifier);
    Q_INVOKABLE void moveItem(const QString& identifier, int direction);
    Q_INVOKABLE void removeSet(const QString& itemIdentifier, const QString& setIdentifier);
    Q_INVOKABLE void removeSession();
    Q_INVOKABLE void moveSession(int direction);
    Q_INVOKABLE void close();
    Q_INVOKABLE void save();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void requestSession();
    Q_INVOKABLE void requestExercise(const QString& identifier);
    Q_INVOKABLE void requestSet(const QString& itemIdentifier);
    Q_INVOKABLE void showTechnique(const QString& identifier);
signals:
    void changed();
    void statusChanged();
    void sessionsChanged();
    void checkpointRequested();
    void saveRequested();
    void sessionRequested();
    void exerciseRequested(QString identifier);
    void setRequested(QString itemIdentifier);
    void techniqueRequested(QString identifier);

private:
    void edited(bool structural);

private:
    core::SportProgramCheckpoint m_checkpoint{};
    std::optional<std::string> m_expected{};
    int m_sessionIndex{0};
    bool m_active{false};
    bool m_saved{true};
    QString m_status{};
};
} // namespace msa::ui
