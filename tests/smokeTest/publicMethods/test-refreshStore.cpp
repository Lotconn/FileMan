#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app{ argc, argv };
    QCoreApplication::setApplicationName("apiTest"_L1);
    QCoreApplication::setOrganizationName("FileManTests"_L1);

    return FileMan{ }.refreshStore();
}