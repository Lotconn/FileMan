#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app{ argc, argv };
    QCoreApplication::setApplicationName("libTest"_L1);
    QCoreApplication::setOrganizationName("FileManTests"_L1);
    FileMan tempMan;

    tempMan.~FileMan();

    logInfo << FileMan::instance().storeDir().toDisplayString();

    return 0;
}