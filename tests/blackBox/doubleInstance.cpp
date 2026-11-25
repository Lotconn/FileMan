#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app{ argc, argv };
    QCoreApplication::setApplicationName("libTest"_L1);
    QCoreApplication::setOrganizationName("FileManTests"_L1);
    FileMan man1;
    FileMan man2;

    logInfo << man1.storeDir().toDisplayString();
    logInfo << man2.storeDir().toDisplayString();

    return 0;
}