#include <fileman.hpp>

// Static
auto FileMan::getFileName(const QUrl &fileUrl) -> QString
{
    return fileUrl.fileName();
}