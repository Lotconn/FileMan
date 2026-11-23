#include <fileman.hpp>

// Static
auto FileMan::fileName(const QUrl &fileUrl) -> QString
{
    return fileUrl.fileName();
}