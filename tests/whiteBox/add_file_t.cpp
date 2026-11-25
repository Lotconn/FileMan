#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app{ argc, argv };
    QCoreApplication::setApplicationName("libTest"_L1);
    QCoreApplication::setOrganizationName("FileManTests"_L1);

    add_file_t  var1     = { 0, u"file://dummy/path/to/file.txt"_s, u""_s };
    add_file_t  var2_cp  = var1;
    add_file_t &var3_ref = var1;

    logInfo << "Copied: " << (&var1 != &var2_cp);
    logInfo << "Referenced: " << (&var1 == &var3_ref);

    add_file_t var4_mv = std::move(var1);

    logInfo << "Moved: " << var1.isNull();
    logInfo << var1.status() << " " << var1.location();
    logInfo << var4_mv.status() << " " << var4_mv.location();

    return 0;
}