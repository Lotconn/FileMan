#include <fileman_p.hpp>

auto FileMan::categoryDir(FileMan::Category category) const -> QString
{
    Q_D(const FileMan);
    return (d->categoryList.contains(category)) ? d->categoryList.value(category).location
                                                : QString{ };
}