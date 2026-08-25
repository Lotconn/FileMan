#include <test-common.hpp>

int main(int /*argc*/, char * /*argv*/[]) {
  FileMan failman;

  logInfo << FileMan::instance().getDataDir();

  return 0;
}