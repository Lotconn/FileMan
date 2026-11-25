#include <fileman_p.hpp>

auto FileMan::categoryDir(FileMan::Category category) const -> QUrl
{
    Q_D(const FileMan);
    return QUrl::fromLocalFile(d->m_categoryToDirs.value(category));
}