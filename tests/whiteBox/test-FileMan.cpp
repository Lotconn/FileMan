#include <test-common.hpp>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("libTest"_L1);

  FileMan  testman;
  FileMan  testman2 = testman;
  FileMan &testman3 = testman;
  logInfo << "Copied: " << (&testman != &testman2);
  logInfo << "Referenced: " << (&testman == &testman3);

  FileMan testman4 = std::move(testman);
  logInfo << "Moved: " << (FileMan::instance() == nullptr);

  return 0;
}