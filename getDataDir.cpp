#include "fileman_p.hpp"

//
// [Public] Returns the absolute path to app's data dir as string; never returns
// null. Note this function doesn't guarranty existence.
//
QString FileMan::getDataDir() const { return d->dataDir; }