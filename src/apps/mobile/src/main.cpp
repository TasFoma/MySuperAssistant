#include <QGuiApplication>
#include <QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(MsaUiCommonPlugin)

int main(int argc, char* argv[])
{
    QGuiApplication application{argc, argv};

    return application.exec();
}
