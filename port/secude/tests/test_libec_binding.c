/* Checks libec's statically initialised ECSecudeLib table (libec/sources/ECsecude.c). */
#include <string.h>

#include "ECsecude.h"

int secude_compat_test_libec(void)
{
    L_NUMBER a[MAXLGTH] = {1, 7}, b[MAXLGTH] = {1, 5}, m[MAXLGTH] = {1, 11}, r[MAXLGTH];
    OctetString msg, hash;
    char abc[] = "abc";

    if (!ECSecudeLib.arithmetic_mmult || !ECSecudeLib.sec_hash_all || !ECSecudeLib.sha1_aid || !ECSecudeLib.ripemd160_aid)
        return 1;
    if (ECSecudeLib.arithmetic_mmult(a, b, r, m) != 0 || r[0] != 1 || r[1] != 2)
        return 2;
    if (ECSecudeLib.arithmetic_comp(a, b) != 1 || ECSecudeLib.lngtouse(a) != 2)
        return 3;
    ECSecudeLib.arithmetic_div(a, b, r, b);
    if (r[1] != 1 || b[1] != 2)
        return 4;
    msg.noctets = 3;
    msg.octets = abc;
    hash.noctets = 0;
    hash.octets = NULL;
    if (ECSecudeLib.sec_hash_all(&msg, &hash, ECSecudeLib.sha1_aid, NULL) != 0 || hash.noctets != 20 ||
        (unsigned char)hash.octets[0] != 0xa9)
        return 5;
    return 0;
}
