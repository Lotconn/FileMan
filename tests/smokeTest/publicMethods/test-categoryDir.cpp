#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app{ argc, argv };
    QCoreApplication::setApplicationName("apiTest"_L1);
    QCoreApplication::setOrganizationName("FileManTests"_L1);

    for (const FileMan::Category cat :
         { FileMan::None, FileMan::Audio, FileMan::Image, FileMan::Video, FileMan::All })
        logInfo << FileMan{ }.categoryDir(cat).toDisplayString();

    return 0;
}