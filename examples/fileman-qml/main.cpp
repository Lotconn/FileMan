/*

                            MIT License

                        Copyright HasanEsa 2026

This is an example app demonstrating modern use of the FileMan library as a QML
module. This approach is recommended over setting context properties from C++
backend to QML namespace.

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

#ifdef FM_STATIC_PLUGIN

//[KEY:BEGIN:Required for static linking]
#include <QtQml/QQmlExtensionPlugin>
Q_IMPORT_QML_PLUGIN(HasanEsa_FileManPlugin)
//[KEY:END]

#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID)
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

static int sigFd[2] = {-1, -1};

static void unixSignalHandler(int)
{
    char a = 'A';
    [[maybe_unused]] auto trash = ::write(sigFd[1], &a, sizeof(a));
    // just to avoid compiler warnings
}
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationDomain("hasanesa");
    app.setApplicationName("FileMan-App");

    QApplication::setStyle(QStyleFactory::create("fusion"));

    QQmlApplicationEngine engine;

    engine.load(QUrl(QLatin1String("qrc:/qt/qml/FilemanQml/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical("Failed to load QML root object; aborting");
        return 1;
    }

#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID)
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigFd) != 0)
        qFatal("Failed to create signal socket pair");

    QSocketNotifier sigNotifier(sigFd[0], QSocketNotifier::Read);
    QObject::connect(&sigNotifier, &QSocketNotifier::activated, &app, [](QSocketDescriptor) {
        char tmp;
        [[maybe_unused]] auto trash = ::read(sigFd[0], &tmp, sizeof(tmp));
        // just to avoid compiler warnings
        QCoreApplication::quit();
    });

    struct sigaction sa {};
    sa.sa_handler = unixSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
#endif

    int ret = app.exec();

#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID)
    ::close(sigFd[0]);
    ::close(sigFd[1]);
#endif

    return ret;
}
