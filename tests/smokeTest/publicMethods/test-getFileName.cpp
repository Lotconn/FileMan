#include <test-common.hpp>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("apiTest"_L1);
  FileMan fileman;

  QUrl fileUrl("file:///home/user/Documents/myDoc.pdf"_L1);

  logInfo << FileMan::getFileName(fileUrl);

  return 0;
}