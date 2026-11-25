#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app{ argc, argv };

    for (const FileMan::Category cat :
         { FileMan::None, FileMan::Audio, FileMan::Image, FileMan::Video, FileMan::All })
        logInfo << FileMan::toString(cat);

    return 0;
}