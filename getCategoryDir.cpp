#include "fileman_p.hpp"

//
// [Public] Returns the absolute path to category directory in store as string.
// Empty if category doesn't match. Note this function doesn't guarranty
// existence.
//
QString FileMan::getCategoryDir(FileMan::Category category) const {

  if (!this->d->categoryList.contains(category))
    return {};

  return this->d->categoryList.value(category).location;
}