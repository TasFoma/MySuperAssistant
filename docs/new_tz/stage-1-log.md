# UI Redesign — журнал передачи контекста

## Текущая задача

Источник требований — [TZ.md](../../TZ.md). Перенос старого проекта приостановлен;
его открытые пункты сохранены в [плане переноса](../migration-plan.md).
**1A и 1B завершены и проверены. Работа остановлена после 1B.**
1C — Android не разрешён и не начат; требуется отдельное указание пользователя.

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

## Выполненный план 1B

Пользователь отдельно разрешил 1B и согласовал план с уточнениями 2026-11-22.
Выполнены новая навигация Desktop, четыре вкладки планов, минимальные
«Сегодня»/«Тренировки» и компактная музыкальная панель. Навигационная часть
решения 53 заменена решением 65; правила подключения сохранены.
Проверены окна 1000×720, 1280×800 и 1600×900, чат, формы и музыка.
Android-компоновка остаётся для отдельного 1C.

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

## 2026-11-22 — 1B завершён

- Принятые требования и границы — решение 65. Шесть разделов и четыре вкладки
  планов представлены enum в DesktopState; числа больше не задают навигацию.
- Добавлены TodayScreen, WorkoutsScreen, MusicScreen и AudioOutputSelector;
  последний используется также в настройках. Общий MusicPanel поддерживает
  компактный и полный режимы на существующей единственной MusicModel.
- AppIcon рисует иконки Canvas; AppButton использует существующее icon.name.
  Зависимости и прежние QML URI сохранены.
- Обновлены проверки начального экрана, конфигурации, переключений, черновика,
  музыки и аудиовыхода. Снимки всех разделов сохранены для трёх размеров окна.
- При высокой панели ошибки чат прокручивается в оставшейся области над
  мини-плеером. Проверяется достижимость черновика, отправки и явного retry.
  Формы открываются и отменяются из каждой из четырёх вкладок планов без записи.
- Заголовки страниц используют существующий SectionHeader с prominent=true
  (26 px), внутренние разделы сохраняют 20 px. Размеры остаются в Theme.
- Штатные конфигурация и сборка windows-debug-local успешны. Заключительный
  полный CTest: **540/540**, 157,22 с. Добавлены четыре теста UI; проверки
  конфигурации, аудиовыхода и существующих действий адаптированы к именованным
  разделам. QML-предупреждений в UI-тестах нет.
- При сборке встречаются C4702 в заголовках Qt. Исправлены проблемы регистрации
  QML и цикл высоты чата; причины/правила находятся в docs/lessons.md.
- Визуально проверены все 18 снимков `desktop-<раздел>-<размер>-preview.png`,
  где раздел — today/plans/workouts/music/assistant/settings, размеры —
  1000x720, 1280x800, 1600x900. Дополнительно просмотрены четыре снимка
  `desktop-plans-<tasks/calendar/habits/reminders>-1000x720-preview.png`,
  `desktop-assistant-error-1000x720-preview.png`, `planner-task-preview.png`,
  `planner-reminder-preview.png`, `speech-output-preview.png`,
  `audio-devices-preview.png` и `music-panel-preview.png`.
  Все файлы находятся в out/build/windows-debug-local.
- Длинное название трека в размерных снимках — стрессовые данные теста,
  а не стартовая запись приложения. Исполнитель/новые метаданные не добавлены.
- На проверенных размерах нет наложений. Чат с высокой ошибкой прокручивается
  над фиксированным мини-плеером; черновик и явный retry остаются достижимы.
  Четыре вкладки открывают существующие формы, отмена не отправляет запись.
- Сервер, SQLite, протокол, предметные модели и музыкальный контроллер не менялись.
  Исполняемый Desktop проверен также существующими offscreen-интеграционными
  тестами с временным сервером/БД и без Qt в PATH. Видимое окно, личная музыка
  и слышимость на реальном оборудовании в этом шаге вручную не проверялись.

## Что осталось для 1C

Только после отдельного указания: новая нижняя навигация Android
«Сегодня / Планы / Спорт / Ещё», минимальные новые страницы, чат/настройки в «Ещё»,
доступ к напоминаниям, сохранение черновика, клавиатура/Назад/системные области.
Использовать общую тему и компоненты, не переносить Desktop простым уменьшением.
Потребуются Android Debug APK и ручная проверка на A55. Музыку Android не
показывать работающей функцией. Решение 60 о прежней мобильной навигации пока
остаётся действующим; при принятии новой явно отметить его замену.
Старый перенос остаётся приостановленным; его открытые пункты не закрывались.

## Файлы шага 1B

- `src/ui/common/CMakeLists.txt`
- `src/ui/common/UI/Theme.qml`
- `src/ui/common/UI/AppButton.qml`
- `src/ui/common/UI/AppIcon.qml` (новый)
- `src/ui/common/UI/Utils/SectionHeader.qml`
- `src/ui/common/UI/MusicPanel.qml`
- `src/ui/desktop/CMakeLists.txt`
- `src/ui/desktop/Model/include/msa/ui/DesktopState.h`
- `src/ui/desktop/Model/src/DesktopState.cpp`
- `src/ui/desktop/UI/DesktopShell.qml`
- `src/ui/desktop/UI/PlansScreen.qml`
- `src/ui/desktop/UI/ChatScreen.qml`
- `src/ui/desktop/UI/SettingsScreen.qml`
- `src/ui/desktop/UI/TodayScreen.qml` (новый)
- `src/ui/desktop/UI/WorkoutsScreen.qml` (новый)
- `src/ui/desktop/UI/MusicScreen.qml` (новый)
- `src/ui/desktop/UI/AudioOutputSelector.qml` (новый)
- `src/apps/desktop/src/DesktopRuntime.cpp`
- `src/apps/desktop/src/DesktopPreferencesController.cpp`
- `tests/infra/src/DesktopScreensTest.cpp`
- `tests/infra/src/DesktopRuntimeTest.cpp`
- `tests/infra/src/ClientPreferencesTest.cpp`
- `docs/architecture.md`
- `docs/decisions.md`
- `docs/lessons.md`
- `docs/migration-plan.md`
- `docs/modules/CommonQml.md`
- `docs/modules/DesktopScreens.md`
- `docs/modules/DesktopRuntime.md`
- `docs/modules/MusicPlayer.md`
- `docs/new_tz/stage-1-log.md`
