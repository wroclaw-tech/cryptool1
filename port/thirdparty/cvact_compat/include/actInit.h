// Source-compatible replacement for the cv act library (subset used by CrypTool).
#ifndef actInit_h
#define actInit_h

#include "actBasics.h"
#include "actDefaultRNG.h"
#include "actRNGKit.h"

namespace act
{
	const char *GetVersion();
	bool GetIsInit() noexcept;
	void SetIsInit(bool bIsInit) noexcept;

	void Init(bool bAlwaysInit = false);
} // namespace act

#endif // actInit_h
