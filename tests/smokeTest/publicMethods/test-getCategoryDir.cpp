#include <test-common.hpp>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("apiTest"_L1);
  FileMan fileman;

  for (const FileMan::Category cat :
       {FileMan::None, FileMan::Audio, FileMan::Image, FileMan::Video, FileMan::All})
    logInfo << fileman.getCategoryDir(cat);

  return 0;
}