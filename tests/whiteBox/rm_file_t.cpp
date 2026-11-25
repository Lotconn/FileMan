#include <test-common.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app{ argc, argv };
    QCoreApplication::setApplicationName("libTest"_L1);
    QCoreApplication::setOrganizationName("FileManTests"_L1);

    rm_file_t  var1     = { 0, 0 };
    rm_file_t  var2_cp  = var1;
    rm_file_t &var3_ref = var1;

    logInfo << "Copied: " << (&var1 != &var2_cp);
    logInfo << "Referenced: " << (&var1 == &var3_ref);

    rm_file_t var4_mv = std::move(var1);

    logInfo << "Moved: " << var1.isNull();
    logInfo << var1.status_l() << " " << var1.status_b();
    logInfo << var4_mv.status_l() << " " << var4_mv.status_b();

    return 0;
}