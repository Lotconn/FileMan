#include <fileman_p.hpp>

auto FileMan::categoryDir(FileMan::Category category) const -> QString
{
    Q_D(const FileMan);
    return d->categoryToDirs.value(category);
}