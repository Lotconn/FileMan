#include <fileman_p.hpp>

auto FileMan::fromStringToCategory(const QString &categoryName) -> FileMan::Category
{
    return FileManPrivate::sc_stringToCategory.value(categoryName, Category::None);
}