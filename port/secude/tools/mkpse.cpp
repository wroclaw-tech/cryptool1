// Creates the CrypTool key store (CA PSE, CA database and the sample keys
// shipped with the installer) in the format of the SECUDE compatibility layer.
#include <cstdio>
#include <cstring>

#include "secude_compat.h"

extern "C" char *th_get_last_error_text(void);

int main(int argc, char **argv)
{
    bool overwrite = false;
    const char *root = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--overwrite"))
            overwrite = true;
        else if (!root && argv[i][0] != '-')
            root = argv[i];
        else
            root = nullptr, i = argc;
    }
    if (!root) {
        std::fprintf(stderr,
                     "usage: %s [--overwrite] <key-store-root>\n"
                     "creates <key-store-root>/PSE (sample keys) and <key-store-root>/PSE/PSECA (CA)\n",
                     argv[0]);
        return 2;
    }
    if (secude_compat_create_sample_keystore(root, overwrite ? 1 : 0) != 0) {
        std::fprintf(stderr, "%s: %s\n", argv[0], th_get_last_error_text());
        return 1;
    }
    std::printf("key store created in %s (%s)\n", root, secude_compat_version());
    return 0;
}
