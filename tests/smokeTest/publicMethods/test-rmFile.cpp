#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("apiTest"_L1);
    FileMan fileman;

    const QString dataDir = fileman.getDataDir();
#ifdef Q_OS_WIN
    const QString fileComp = "/image/i00.png.lnk"_L1;
#else
    const QString fileComp = "/image/i00.png"_L1;
#endif
    QUrl fileUrl = QUrl::fromLocalFile(QDir::cleanPath(dataDir + fileComp));

    const rm_file_t &res = fileman.rmFile(fileUrl);
    logInfo << res.status_l() << " " << res.status_b();

    return res.status_l();
}