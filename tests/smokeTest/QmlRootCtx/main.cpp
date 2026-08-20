#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml/qqml.h>
#include <fileman>

int main(int argc, char *argv[]) {

  using Qt::StringLiterals::operator""_L1;

  QCoreApplication app(argc, argv);
  QCoreApplication::setOrganizationName("RootCtxTest"_L1);
  QCoreApplication::setApplicationName(APPNAME);

  FileMan fileman;
  qmlRegisterUncreatableMetaObject(FileMan::staticMetaObject, "Lotconn.FileMan.Enums", 0, 0,
                                   "FileMan", "Enum/flag access only");

  QQmlApplicationEngine engine;
  const QString         qmlUrl = "qrc:/"_L1 + APPNAME + "/Main.qml"_L1;

  engine.rootContext()->setContextProperty("fileman", &fileman);
  engine.load(QUrl("qrc:/"_L1 + APPNAME + "/Main.qml"_L1));

  if (engine.rootObjects().isEmpty()) {
    qCritical("Failed to load QML root object; aborting");
    return 1;
  }

  return QCoreApplication::exec();
}
