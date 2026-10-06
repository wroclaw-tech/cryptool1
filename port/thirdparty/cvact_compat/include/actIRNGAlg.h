// Source-compatible replacement for the cv act library (subset used by CrypTool).
#ifndef ACT_IRNGAlg_h
#define ACT_IRNGAlg_h

#include "actIAlgorithm.h"

namespace act
{
	class Blob;

	class IRNGAlg : public IAlgorithm
	{
	public:
		virtual IRNGAlg *Clone() const = 0;
		virtual void Import(const Blob &keyblob) = 0;
		virtual void Export(Blob &keyblob) const = 0;
		virtual void SetParam(paramid_t id, const Blob &blob) = 0;
		virtual void SetParam(paramid_t id, int val) = 0;
		virtual void SetParam(paramid_t id, const char *cstr) = 0;
		virtual int GetParam(paramid_t id) const = 0;
		virtual void GetParam(paramid_t id, Blob &blob) const = 0;
		virtual void *GetCreatePointer() const = 0;
	};
} // namespace act

#endif // ACT_IRNGAlg_h
