#include <QTimer>

#include <test-common.hpp>

static void onAboutToQuit()
{
    logInfo << FileMan::instance().storeDir().toDisplayString();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("libTest"_L1);
    QCoreApplication::setOrganizationName("FileManTests"_L1);
    FileMan fileman;

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &onAboutToQuit);

    QTimer::singleShot(0, &app, &QCoreApplication::quit);

    return QCoreApplication::exec();
}