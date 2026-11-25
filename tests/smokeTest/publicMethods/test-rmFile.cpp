#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app{ argc, argv };
    QCoreApplication::setApplicationName("apiTest"_L1);
    QCoreApplication::setOrganizationName("FileManTests"_L1);
    FileMan fileman;

    const QString storeDir = fileman.storeDir().toLocalFile();
#ifdef Q_OS_WIN
    const QString fileComp = u"/image/i00.png.lnk"_s;
#else
    const QString fileComp = u"/image/i00.png"_s;
#endif
    QUrl fileUrl = QUrl::fromLocalFile(QDir::cleanPath(storeDir + fileComp));

    const rm_file_t res = fileman.rmFile(fileUrl);
    logInfo << res.status_l() << " " << res.status_b();

    return res.status_l();
}