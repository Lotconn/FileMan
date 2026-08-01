/*

                            MIT License

                        Copyright Lotconn 2026

This is an example app demonstrating legacy use of embedding the FileMan object
in QML with context properties. Although this approach is discouraged in favor
of QML modules, most apps still follow this procedure.

The key directives to extract from this example for library integration are
highlighted with [KEY] inline comment on the preceeding line. For a subsequent
block, the highlighted part starts with [KEY:BEGIN] and ends with [KEY:END].
Additional notes are appended to the specifier like [KEY:BEGIN:extra comments]
or [KEY::extra comments].

*/

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QSocketNotifier>
#include <QStyleFactory>

//[KEY:BEGIN:Change <fileman.cpp> to <Lotconn/FileMan/fileman.cpp> for your app]
#include <QQmlContext>
#include <QtQml/qqml.h>
#include <fileman>
//[KEY:END]

#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID)
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

static std::array<int, 2> sigFd = {-1, -1};

static void unixSignalHandler(int /*unused*/) {
  char a = 'A';
  [[maybe_unused]] auto trash = ::write(sigFd[1], &a, sizeof(a));
  // just to avoid compiler warnings
}
#endif

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QApplication::setOrganizationDomain("lotconn");
  QApplication::setApplicationName("FileMan-QML-legacy");

  QApplication::setStyle(QStyleFactory::create("fusion"));

  //[KEY:BEGIN:Registers enums, flags & types inside QML namespaces]
  FileMan fileman;
  qmlRegisterUncreatableMetaObject(FileMan::staticMetaObject,
                                   "Lotconn.FileMan.Enums", 0, 0, "FileMan",
                                   "Enum/flag access only");
  //[KEY:END]

  QQmlApplicationEngine engine;
  //[KEY]
  engine.rootContext()->setContextProperty("fileman", &fileman);
  engine.load(QUrl(QLatin1String("qrc:/main.qml")));
  if (engine.rootObjects().isEmpty()) {
    qCritical("Failed to load QML root object; aborting");
    return 1;
  }

#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID)
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigFd.data()) != 0)
    qFatal("Failed to create signal socket pair");

  QSocketNotifier sigNotifier(sigFd[0], QSocketNotifier::Read);
  QObject::connect(
      &sigNotifier, &QSocketNotifier::activated, &app, [](QSocketDescriptor) {
        char tmp = 0;
        [[maybe_unused]] auto trash = ::read(sigFd[0], &tmp, sizeof(tmp));
        // just to avoid compiler warnings
        QCoreApplication::quit();
      });

  struct sigaction sa{};
  sa.sa_handler = unixSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
#endif

  int ret = QApplication::exec();

#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID)
  ::close(sigFd[0]);
  ::close(sigFd[1]);
#endif

  return ret;
}
