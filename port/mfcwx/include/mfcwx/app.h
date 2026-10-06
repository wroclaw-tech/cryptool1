#pragma once

// Hooks for the platform glue of an application built on mfcwx.

#include <string>
#include <vector>

namespace mfcwx {

namespace rc {
struct Module;
}

void RegisterResourceModule(const rc::Module* module);
void SetResourceLanguage(const char* code);
const char* GetResourceLanguage();
std::vector<std::string> AvailableResourceLanguages();
std::string GetDataDirectory();
void SetDataDirectory(const std::string& dir);
// Used when neither MFCWX_DATA_DIR nor an installed resource directory is found (development builds).
void SetDefaultDataDirectory(const std::string& dir);

} // namespace mfcwx
