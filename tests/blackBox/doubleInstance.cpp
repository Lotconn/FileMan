#include <test-common.hpp>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("libTest"_L1);
  FileMan man1;
  FileMan man2;

  return 0;
}