#include <test-common.hpp>

static void onAboutToQuit() { logInfo << FileMan::instance().getDataDir(); }

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("libTest"_L1);
  FileMan fileman;

  QObject::connect(&app, &QCoreApplication::aboutToQuit, &onAboutToQuit);

  QTimer::singleShot(0, &app, &QCoreApplication::quit);

  return QCoreApplication::exec();
}