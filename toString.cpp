#include <fileman_p.hpp>

// Static
auto FileMan::toString(const Category category) -> QString
{
    return FileManPrivate::categoryToString.value(category);
}

// Static
auto FileMan::toString(const Error error) -> QString
{
    return FileManPrivate::metaErrorEnum.valueToKey(qToUnderlying(error));
}