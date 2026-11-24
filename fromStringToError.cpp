#include <fileman_p.hpp>

auto FileMan::fromStringToError(const QString &errorName) -> Error
{
    bool      ok  = false;
    const int val = FileManPrivate::sc_metaErrorEnum.keyToValue(qPrintable(errorName), &ok);

    return (ok) ? static_cast<Error>(val) : Error::Success;
}