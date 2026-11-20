#include <fileman_p.hpp>

auto FileMan::getDataDir() const -> QString
{
    Q_D(const FileMan);
    return d->dataDir;
}