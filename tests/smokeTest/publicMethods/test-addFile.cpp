#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app{ argc, argv };
    QCoreApplication::setApplicationName("apiTest"_L1);
    QCoreApplication::setOrganizationName("FileManTests"_L1);
    FileMan fileman;

    QUrl a00 = QUrl::fromLocalFile("data/a00.ac3"_L1);
    QUrl i00 = u"file:data/i00.png"_s;
    QUrl v00 = u"file:data/v00.mp4"_s;
    QUrl a01 = QUrl::fromLocalFile("data/a01.flac"_L1);

    int e = 0;
    for (const add_file_t &res :
         { fileman.addFile(a00, FileMan::Audio),
           fileman.addFile(i00, FileMan::Image | FileMan::Video),
           fileman.addFile(a01, FileMan::Audio | FileMan::Video), fileman.addFile(v00) }) {
        logInfo << res.status() << " " << res.location().toDisplayString();
        e += res.status();
    }

    return e;
}