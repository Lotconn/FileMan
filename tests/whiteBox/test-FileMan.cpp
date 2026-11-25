#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app{ argc, argv };
    QCoreApplication::setApplicationName("libTest"_L1);
    QCoreApplication::setOrganizationName("FileManTests"_L1);

    FileMan  testman;
    FileMan &testman2 = testman;

    logInfo << "Referenced: " << (&testman == &testman2);

    return 0;
}