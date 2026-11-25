#include <QCoreApplication>
#include <QQmlApplicationEngine>

#ifdef FM_STATIC_PLUGIN
#  include <QtQml/QQmlExtensionPlugin>
Q_IMPORT_QML_PLUGIN(Lotconn_FileManPlugin)
#endif

int main(int argc, char *argv[])
{

    using Qt::StringLiterals::operator""_L1;
    using Qt::StringLiterals::operator""_s;

    QCoreApplication app{ argc, argv };
    QCoreApplication::setOrganizationName("FileManModuleTests"_L1);
    QCoreApplication::setApplicationName(QStringLiteral(APPNAME));

    QQmlApplicationEngine engine;

    engine.loadFromModule(APPNAME, "Main");

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML root object; aborting";
        return 1;
    }

    return QCoreApplication::exec();
}