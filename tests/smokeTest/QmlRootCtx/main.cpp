#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml/qqml.h>

#include <FileMan>

int main(int argc, char *argv[])
{

    using Qt::StringLiterals::operator""_L1;
    using Qt::StringLiterals::operator""_s;

    QCoreApplication app{ argc, argv };
    QCoreApplication::setOrganizationName("FileManRootCtxTests"_L1);
    QCoreApplication::setApplicationName(QStringLiteral(APPNAME));

    FileMan fileman;
    qmlRegisterUncreatableMetaObject(FileMan::staticMetaObject, "Lotconn.FileMan.Enums", 0, 0,
                                     "FileMan", u"Enum/flag access only"_s);

    QQmlApplicationEngine engine;
    const QUrl            qmlUrl = u"qrc:/"_s + QStringLiteral(APPNAME) + u"/Main.qml"_s;

    engine.rootContext()->setContextProperty(u"fileman"_s, &fileman);
    engine.load(qmlUrl);

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML root object; aborting";
        return 1;
    }

    return QCoreApplication::exec();
}
