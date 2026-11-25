#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app{ argc, argv };
    QUrl             fileUrl = u"file:///home/user/Documents/myDoc.pdf"_s;

    logInfo << FileMan::fileName(fileUrl);

    return 0;
}