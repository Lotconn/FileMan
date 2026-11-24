#include <fileman_p.hpp>

auto FileMan::storeDir() const -> QString
{
    Q_D(const FileMan);
    return d->m_storeDirPath;
}