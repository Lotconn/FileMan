#include <fileman_p.hpp>

// Static
auto FileMan::toString(const Category category) -> QString
{
    return (category == Category::None || category == Category::All)
            ? QString{ }
            : QString{ FileManPrivate::metaCategoryEnum.valueToKey(qToUnderlying(category)) }
                      .toLower();
}

// Static
auto FileMan::toString(const Error error) -> QString
{
    return FileManPrivate::metaErrorEnum.valueToKey(qToUnderlying(error));
}