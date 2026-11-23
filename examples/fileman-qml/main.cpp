/*

                            MIT License

                        Copyright Lotconn 2026

This is an example app demonstrating modern use of the FileMan library as a QML
module. This approach is recommended over setting context properties from C++
backend to QML namespace.

The key directives to extract from this example for library integration are
highlighted with [KEY] inline comment on the preceeding line. For a subsequent
block, the highlighted part starts with [KEY:BEGIN] and ends with [KEY:END].
Additional notes are appended to the specifier like [KEY:BEGIN:extra comments]
or [KEY::extra comments].

*/

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QSocketNotifier>

#ifdef FM_STATIC_PLUGIN

//[KEY:BEGIN:Required for static linking]
#  include <QtQml/QQmlExtensionPlugin>
Q_IMPORT_QML_PLUGIN(Lotconn_FileManPlugin)
//[KEY:END]

#endif

#ifdef Q_OS_UNIX
#  include <csignal>
#  include <sys/socket.h>
#  include <unistd.h>

static std::array<int, 2> sigFd = { -1, -1 };
#endif

int main(int argc, char *argv[])
{
    using Qt::StringLiterals::operator""_L1;

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("FileMan-QML"_L1);
    QGuiApplication::setOrganizationName("Lotconn"_L1);

    QQmlApplicationEngine engine;

    engine.load(QUrl{ "qrc:/qt/qml/FilemanQml/Main.qml"_L1 });

#ifdef Q_OS_UNIX
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigFd.data()) != 0)
        qCritical() << "Failed to create signal socket pair";

    QSocketNotifier sigNotifier(sigFd[0], QSocketNotifier::Read);

    QObject::connect(&sigNotifier, &QSocketNotifier::activated, &app,
                     [](QSocketDescriptor) -> void {
                         char                  tmp   = 0;
                         [[maybe_unused]] auto trash = ::read(sigFd[0], &tmp, sizeof(tmp));
                         QGuiApplication::quit();
                     });

    struct sigaction sa{ };
    sa.sa_handler = [](int) -> void {
        char                  a     = 'A';
        [[maybe_unused]] auto trash = ::write(sigFd[1], &a, sizeof(a));
        // just to avoid compiler warnings
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
#endif

    int ret = QGuiApplication::exec();

#ifdef Q_OS_UNIX
    ::close(sigFd[0]);
    ::close(sigFd[1]);
#endif

    return ret;
}
