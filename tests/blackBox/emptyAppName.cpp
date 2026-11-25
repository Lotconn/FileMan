#include <test-common.hpp>

int main(int /*argc*/, char * /*argv*/[])
{
    logInfo << FileMan{ }.storeDir().toDisplayString();

    return 0;
}