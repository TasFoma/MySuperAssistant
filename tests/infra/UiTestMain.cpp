#include <gtest/gtest.h>

#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(MsaUiCommonPlugin)

int main(int argc, char* argv[])
{
    QGuiApplication application{argc, argv};
    // Offscreen не получает системный шрифт Windows от оконного плагина.
    if (QFontDatabase::families().contains(QStringLiteral("Segoe UI")))
    {
        QGuiApplication::setFont(QFont{QStringLiteral("Segoe UI")});
    }
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
