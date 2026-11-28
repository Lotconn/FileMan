#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app{ argc, argv };
    QCoreApplication::setApplicationName("apiTest"_L1);
    QCoreApplication::setOrganizationName("FileManTests"_L1);
    FileMan fileman;

#ifdef Q_OS_WIN
    const QString fileComp = u"./images/i00.png.lnk"_s;
#else
    const QString fileComp = u"./images/i00.png"_s;
#endif
    const QUrl fileUrl = fileman.storeDir().resolved(fileComp);

    const rm_file_t res = fileman.rmFile(fileUrl);
    logInfo << res.status_l() << " " << res.status_b();

    return res.status_l();
}