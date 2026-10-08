#ifndef CRYPTOOL_COMPAT_IOSTREAM_H
#define CRYPTOOL_COMPAT_IOSTREAM_H

// Legacy code selects <iostream.h> on non-MSVC compilers; the MSVC branch it
// replaces includes <iostream> and imports namespace std, so do the same here.
#include <iostream>
using namespace std;

#endif
