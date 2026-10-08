// Source-compatible replacement for the cv act library (subset used by CrypTool).
// Supported key types: "IES" (EC-IES over prime curves) and "BlockCipher".
#ifndef ACT_KEY_H
#define ACT_KEY_H

#include "actBasics.h"
#include "actBlob.h"

namespace act
{
	class IRNGAlg;
	class IAlgorithm;
	class IKey;

	class Key
	{
	public:
		Key();
		Key(const Key &key);
		Key(IKey *keyptr);
		Key(const Blob &keyblob);
		Key(const char *keytype);

		void Import(const Blob &keyblob);
		void Export(Blob &keyblob, export_t type = DEFAULT) const;

		void SetParam(paramid_t id, const Blob &blob);
		void SetParam(paramid_t id, int val);
		void SetParam(paramid_t id, const char *cstr);
		int GetParam(paramid_t id) const;
		void GetParam(paramid_t id, Blob &blob) const;

		void Generate(IAlgorithm *prng = 0);
		void Derive(const Blob &data, const Blob &salt = Blob());

		IAlgorithm *CreateAlgorithm(mode_t Mode) const;
		IAlgorithm *CreateAlgorithm(mode_t Mode, const Blob &data) const;

		IKey *GetPointer();
		const IKey *GetPointer() const;
		operator IKey *();
		operator const IKey *() const;
		IKey *ReleasePointer();

		Key &operator=(const Key &key);
		~Key();

	private:
		IKey *mKey;
	};
} // namespace act

#endif // ACT_KEY_H
