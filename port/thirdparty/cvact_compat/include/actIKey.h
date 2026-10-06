// Source-compatible replacement for the cv act library (subset used by CrypTool).
#ifndef ACT_IKEY_H
#define ACT_IKEY_H

#include "actBasics.h"
#include "actBlob.h"

namespace act
{
	class IAlgorithm;
	class IRNGAlg;

	class IKey
	{
	public:
		virtual IKey *Clone() const = 0;
		virtual void Import(const Blob &keyblob) = 0;
		virtual void Export(Blob &keyblob, export_t type = DEFAULT) const = 0;
		virtual void SetParam(paramid_t id, const Blob &blob) = 0;
		virtual void SetParam(paramid_t id, int val) = 0;
		virtual void SetParam(paramid_t id, const char *cstr) = 0;
		virtual int GetParam(paramid_t id) const = 0;
		virtual void GetParam(paramid_t id, Blob &blob) const = 0;
		virtual void Generate(IRNGAlg *prng = 0) = 0;
		virtual void Derive(const Blob &data, const Blob &salt = Blob()) = 0;
		virtual IAlgorithm *CreateAlgorithm(mode_t Mode) const = 0;
		virtual IAlgorithm *CreateAlgorithm(mode_t Mode, const Blob &data) const = 0;
		virtual void *GetCreatePointer() const = 0;
		virtual ~IKey() {}
	};
} // namespace act

#endif // ACT_IKEY_H
