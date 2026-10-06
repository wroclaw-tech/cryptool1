/* Reports the PWDICT layout as seen by the cracklib sources. */
#include <stddef.h>
#include CT_CRACKLIB_PACKER_H

void ct_cracklib_layout(size_t *size, size_t *numwords_offset, size_t *count_offset)
{
    *size = sizeof(PWDICT);
    *numwords_offset = offsetof(PWDICT, header.pih_numwords);
    *count_offset = offsetof(PWDICT, count);
}

char *ct_cracklib_getpw(PWDICT *pwp, unsigned long number)
{
    return GetPW(pwp, (int32) number);
}

char *ct_cracklib_fascistcheck(char *password, char *path)
{
    return FascistCheck(password, path);
}
