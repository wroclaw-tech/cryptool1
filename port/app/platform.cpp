#include "mfcwx/app.h"
#include "mfcwx/rcdata.h"

extern const mfcwx::rc::Module g_rcModule_CrypTool;

namespace {

struct RegisterCrypTool {
    RegisterCrypTool() {
        mfcwx::RegisterResourceModule(&g_rcModule_CrypTool);
        mfcwx::SetDefaultDataDirectory(CRYPTOOL_DEV_DATA_DIR);
    }
} registerCrypTool;

} // namespace
