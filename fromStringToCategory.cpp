#include <fileman_p.hpp>

auto FileMan::fromStringToCategory(const QString &categoryName) -> FileMan::Category
{
    return FileManPrivate::nameToCategory.value(categoryName, Category::None);
}