#include <test-common.hpp>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("apiTest"_L1);
  FileMan fileman;

  logInfo << fileman.getDataDir();

  return 0;
}