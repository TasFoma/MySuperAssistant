# UI Redesign — журнал передачи контекста

## Текущая задача

Источник требований — [TZ.md](../../TZ.md). Перенос старого проекта приостановлен;
его открытые пункты сохранены в [плане переноса](../migration-plan.md).
Согласован и завершён только **1A — общая дизайн-система**. Работа остановлена
после сборки, тестов и визуальной проверки. Для 1B и 1C нужно отдельное указание
пользователя; они не начаты.

## 2026-11-21 — 1A

- Проведён аудит Theme.qml, всех общих QML-компонентов и существующей галереи.
  План согласован пользователем; требования доступности и границы — решение 64
  в [журнале решений](../decisions.md).
- Используются существующие компоненты. Изменены тема, оформление Controls,
  карточек, заголовков и состояний, внешний вид ComboBox общей формы.
  Поля получили необязательный errorText; прежний hasError остаётся рабочим.
- Галерея расширена ошибочными и отключёнными полями, тремя тонами кнопок,
  состояниями подключения. Проверяются ширины 360/412 px, фокус, подписи ошибок,
  читаемость отключённого текста и сохранение прежних действий.
- Кнопки и поля получили заметный фокус; у кнопок он виден и при обрезании
  внешнего кольца списком. Отключение и нажатие отображаются сразу независимо
  от перехода обычного/hover-цвета. ConnectionBanner уплотнён внутри прежнего
  размещения. Точные правила оформления описаны в CommonQml.

## Итог проверки 1A

- Выполнены штатные команды `cmake --preset windows-debug-local`,
  `cmake --build --preset windows-debug-local`,
  `ctest --test-dir out/build/windows-debug-local -C Debug --output-on-failure`.
  Конфигурация и сборка успешны, **536 из 536 тестов прошли**, последний полный
  прогон — 138,32 с. Добавлены три теста общих компонентов; существующие проверки
  переподключения, узкой галереи и сохранения рендера расширены.
- Для FetchContent понадобился сетевой доступ к существующим закреплённым
  зависимостям. В сборке остаются предупреждения MSVC C4702 в заголовках Qt
  qjsengine.h/qvariant.h; ошибок компиляции и QML-предупреждений в UI-тестах нет.
- Просмотрены свежие файлы в `out/build/windows-debug-local/`:
  `common-components-preview.png` (960 px), `common-components-360-preview.png`,
  `common-components-412-preview.png`, `common-focus-primaryButton-preview.png`,
  `common-focus-errorField-preview.png`; `desktop-screen-0.png` …
  `desktop-screen-3.png`; `planner-task-preview.png`, `planner-reminder-preview.png`,
  `speech-output-preview.png`, `voice-input-minimum-preview.png`;
  `mobile-screen-0.png` … `mobile-screen-3.png`, `mobile-planner-preview.png`,
  `mobile-keyboard-preview.png`.
- Галерея не переполняет ширины 360/412 px; текст ошибок переносится и не
  перекрывает ввод. Контраст отключённых подписей проверяется автоматически
  на уровне не ниже 4.5:1. На Desktop 1000×720 кнопка отправки помещается над
  музыкальной панелью. Полные экраны также просмотрены при 1200×900.
- Известных незакрытых отказов проверок 1A нет. Android APK в этом подэтапе
  не собирался; A55, настоящая клавиатура и системные области не проверялись.
  Offscreen-проверка уменьшенного окна не заменяет проверку устройства.

## Что потребуется для 1B

Отдельное указание пользователя, аудит текущего DesktopShell/MusicPanel и короткий
план с файлами до изменений. По TZ.md предстоят новая навигация Desktop,
минимальные «Сегодня»/«Тренировки» и компактная музыкальная панель. При принятии
навигации явно уточнить замену соответствующей части решения 53, сохранив правила
подключения. Проверить окна 1000×720, 1280×800 и 1600×900, чат, формы и музыку.
Это следующий самостоятельный подэтап; Android-компоновка остаётся для 1C.

## Файлы шага 1A

- `src/ui/common/UI/Theme.qml`
- `src/ui/common/UI/AppButton.qml`
- `src/ui/common/UI/AppTextField.qml`
- `src/ui/common/UI/AppTextArea.qml`
- `src/ui/common/UI/Utils/SurfaceCard.qml`
- `src/ui/common/UI/Utils/SectionHeader.qml`
- `src/ui/common/UI/EmptyState.qml`
- `src/ui/common/UI/ConnectionBanner.qml`
- `src/ui/common/UI/ErrorBanner.qml`
- `src/ui/common/UI/PlannerFormDialog.qml`
- `tests/infra/qml/CommonComponentsGallery.qml`
- `tests/infra/src/CommonComponentsTest.cpp`
- `tests/infra/src/DesktopScreensTest.cpp` — только ожидание перед рендером.
- `tests/infra/src/MobileScreensTest.cpp` — только ожидание перед рендером.
- `docs/modules/CommonQml.md`
- `docs/decisions.md`
- `docs/migration-plan.md`
- `docs/lessons.md` — поиск элементов галереи, ожидание рендера и отключение
  кнопки во время перехода цвета.
- `docs/new_tz/stage-1-log.md`

Структура UI, связи слоёв и команды сборки не изменились; архитектурный документ,
README и описания экранов не требуют изменения. Существующее ошибочное указание
в CommonQml о mobile без окна исправлено по фактическому MobileRuntime.

## Продолжение в следующем чате

Прочитать AGENTS.md, TZ.md, этот журнал, docs/decisions.md, docs/lessons.md,
docs/migration-plan.md и docs/modules/CommonQml.md. Проверить фактический статус
проверок, не считать наличие исходников подтверждением успешной проверки.
Оболочки, навигация и мини-плеер в 1A не перерабатываются.
