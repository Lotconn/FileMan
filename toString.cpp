#include <fileman_p.hpp>

// Static
auto FileMan::categoryToString(Category category) -> QString
{
    return FileMan::toString(category);
}

// Static
auto FileMan::errorToString(Error error) -> QString
{
    return FileMan::toString(error);
}

// Static
auto FileMan::toString(const Category category) -> QString
{
    return FileManPrivate::sc_categoryToString.value(category);
}

// Static
auto FileMan::toString(const Error error) -> QString
{
    return FileManPrivate::sc_metaErrorEnum.valueToKey(qToUnderlying(error));
}
