// Source-compatible replacement for the cv act library (subset used by CrypTool).
#ifndef ACT_DefaultRNG_h
#define ACT_DefaultRNG_h

namespace act
{
	class IRNGAlg;

	typedef IRNGAlg *(*CreateRNGPtr)();
	extern CreateRNGPtr CreateFastRNG;
	extern CreateRNGPtr CreateStrongRNG;
} // namespace act

#endif // ACT_DefaultRNG_h
