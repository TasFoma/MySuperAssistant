#pragma once

#include "msa/core/SportWorkoutRepository.h"
#include "msa/core/SportWorkoutService.h"
#include "msa/ui/SportFormData.h"

#include <QObject>

namespace msa::ui
{
/// @brief Форма плана/факта и результата; хранение и жизненный цикл принадлежат apps.
class SportWorkoutEditor final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool editing READ editing NOTIFY changed)
    Q_PROPERTY(bool correcting READ correcting NOTIFY changed)
    Q_PROPERTY(bool discardRequested READ discardRequested NOTIFY changed)
    Q_PROPERTY(QVariantList fields READ fields NOTIFY changed)
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(QVariantList wellbeing READ wellbeing NOTIFY changed)
    Q_PROPERTY(QVariantList absences READ absences NOTIFY changed)
    Q_PROPERTY(QVariantMap timeSummary READ timeSummary NOTIFY timeChanged)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString restrictionsSnapshot READ restrictionsSnapshot NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    explicit SportWorkoutEditor(QObject* parent = nullptr);
    /// @brief Восстанавливает только переданное состояние; отсутствие учитывает apps отдельно.
    void openDraft(core::SportCheckpoint checkpoint);
    void openResult(core::SportWorkoutRevision revision);
    [[nodiscard]] bool visible() const;
    [[nodiscard]] bool active() const;
    [[nodiscard]] bool editing() const;
    [[nodiscard]] bool correcting() const;
    [[nodiscard]] bool discardRequested() const;
    [[nodiscard]] bool saved() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QString restrictionsSnapshot() const;
    [[nodiscard]] QString status() const;
    [[nodiscard]] QVariantList fields() const;
    [[nodiscard]] QVariantList items() const;
    [[nodiscard]] QVariantList wellbeing() const;
    [[nodiscard]] QVariantList absences() const;
    [[nodiscard]] QVariantMap timeSummary() const;
    [[nodiscard]] const core::SportCheckpoint& checkpoint() const;
    [[nodiscard]] const std::optional<core::SportWorkoutRevision>& original() const;
    /// @brief Разбирает итоговые поля, но не объявляет тренировку сохранённой.
    [[nodiscard]] core::SportResult<core::SportWorkoutDraft> candidate() const;
    void acknowledge(std::uint64_t sequence);
    void reportError(const QString& message);
    void reportStorageError(const QString& message);
    /// @brief Применяет наблюдение/решение таймера без перезагрузки полей формы.
    void applyTimer(core::SportTimerState timer);
    void showElapsed(core::SportDuration elapsed);
    void applyPrepared(core::SportWorkoutDraft draft);
    void addSet(const QString& itemIdentifier, const std::string& setIdentifier);
    void replaceExercise(const QString& itemIdentifier, const core::SportProgramItem& replacement);
    Q_INVOKABLE void setField(const QString& key, const QString& value);
    Q_INVOKABLE void recordSet(const QString& itemIdentifier, const QString& setIdentifier);
    Q_INVOKABLE void skipSet(const QString& itemIdentifier, const QString& setIdentifier);
    Q_INVOKABLE void skipExercise(const QString& itemIdentifier);
    Q_INVOKABLE void removeItem(const QString& itemIdentifier);
    Q_INVOKABLE void removeSet(const QString& itemIdentifier, const QString& setIdentifier);
    Q_INVOKABLE void toggleArea(const QString& prefix, const QString& area);
    Q_INVOKABLE void pauseOrResume();
    Q_INVOKABLE void resolveAbsence(const QString& identifier, const QString& decision);
    Q_INVOKABLE void requestSet(const QString& itemIdentifier);
    Q_INVOKABLE void requestReplacement(const QString& itemIdentifier, const QString& exerciseIdentifier);
    Q_INVOKABLE void showTechnique(const QString& exerciseIdentifier);
    Q_INVOKABLE void retry();
    Q_INVOKABLE void finish();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void close();
    Q_INVOKABLE void beginCorrection();
    Q_INVOKABLE void keepEditing();
    Q_INVOKABLE void discardCorrection();
signals:
    void changed();
    void timeChanged();
    void statusChanged();
    void checkpointRequested();
    void finishRequested();
    void pauseRequested();
    void absenceRequested(QString identifier, QString decision);
    void setRequested(QString itemIdentifier);
    void replacementRequested(QString itemIdentifier, QString exerciseIdentifier);
    void techniqueRequested(QString exerciseIdentifier);

private:
    void edited(bool structural);
    bool apply(core::SportResult<core::SportWorkoutDraft> result);

private:
    core::SportCheckpoint m_checkpoint{};
    core::SportDuration m_liveElapsed{};
    std::optional<core::SportWorkoutRevision> m_original{};
    bool m_visible{false};
    bool m_correcting{false};
    bool m_saved{true};
    bool m_discardRequested{false};
    QString m_status{};
    QString m_error{};
};
} // namespace msa::ui
