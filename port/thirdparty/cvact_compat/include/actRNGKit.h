// Source-compatible replacement for the cv act library (subset used by CrypTool).
// All generators are backed by the OpenSSL CSPRNG; written data is mixed in as seed.
#ifndef ACT_RNGKit_h
#define ACT_RNGKit_h

namespace act
{
	class IRNGAlg;

	IRNGAlg *CreateDevRandomRNG();
	IRNGAlg *CreateBBS();
	IRNGAlg *CreateFIPS186();
} // namespace act

#endif // ACT_RNGKit_h
