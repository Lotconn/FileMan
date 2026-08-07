#include "fileman_p.hpp"

//
// [Public] Translate Category enum to string. Returns the corresponding name
// from m_categoryList QHash dictionary as string, empty if nothing matched.
//
QString FileMan::categoryToString(const FileMan::Category category) const {

  if (!this->d->categoryList.contains(category))
    return {};

  return this->d->categoryList.value(category).name;
}