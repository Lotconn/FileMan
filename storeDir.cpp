#include <fileman_p.hpp>

auto FileMan::storeDir() const -> QUrl
{
    Q_D(const FileMan);
    return QUrl::fromLocalFile(d->m_storeDirPath + u'/');
}