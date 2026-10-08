// Compiled without mfcwx/crtcompat.h: app code sees MIRACL's rand overloads renamed to mfcwx_rand.
#include "BIG.H"

Big mfcwx_rand(const Big& w) { return rand(w); }
Big mfcwx_rand(int n, int b) { return rand(n, b); }
