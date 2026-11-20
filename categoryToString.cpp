#include <fileman_p.hpp>

auto FileMan::categoryToString(const FileMan::Category category) const -> QString
{
    Q_D(const FileMan);
    return (d->categoryList.contains(category)) ? d->categoryList.value(category).name : QString{ };
}