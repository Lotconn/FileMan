#include <fileman_p.hpp>

auto FileMan::fromStringToCategory(const QString &categoryName) -> FileMan::Category
{
    return FileManPrivate::stringToCategory.value(categoryName, Category::None);
}