#include "ECsecude.h"

#ifndef LINK_SECUDE

#ifdef _WIN32
struct Secude_s ECSecudeLib;
#else
/* bound at link time to the SECUDE compatibility library (port/secude) */
#define DoOneFn(a, b, c, d) (c##_t) c,
#define DoOneData(a, b) &b,
struct Secude_s ECSecudeLib = { DoECAll };
#undef DoOneFn
#undef DoOneData
#endif

#endif
