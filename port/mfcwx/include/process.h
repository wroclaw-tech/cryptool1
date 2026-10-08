#pragma once
#include "mfcwx/crtcompat.h"

#define _P_WAIT 0
#define _P_NOWAIT 1
#define _P_OVERLAY 2
#define _P_NOWAITO 3
#define _P_DETACH 4
intptr_t _spawnl(int mode, const char* cmdname, const char* arg0, ...);
