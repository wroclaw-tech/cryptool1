#include "actInternal.h"
#include "actKey.h"

#include <cstring>

namespace act
{
	namespace
	{
		IKey *Checked(IKey *key)
		{
			if (key == 0)
				throw NullPointerException("key object not initialised", "Key");
			return key;
		}
	}

	Key::Key() : mKey(0) {}

	Key::Key(const Key &key) : mKey(key.mKey != 0 ? key.mKey->Clone() : 0) {}

	Key::Key(IKey *keyptr) : mKey(keyptr) {}

	Key::Key(const Blob &keyblob) : mKey(detail::ImportECKeyBlob(keyblob)) {}

	Key::Key(const char *keytype) : mKey(0)
	{
		if (keytype == 0)
			throw NullPointerException("null key type", "Key::Key");
		if (std::strcmp(keytype, "IES") == 0)
			mKey = detail::CreateIESKey();
		else if (std::strcmp(keytype, "BlockCipher") == 0)
			mKey = detail::CreateBlockCipherKey();
		else
			throw NoSuchAlgorithmException("unsupported key type", "Key::Key");
	}

	Key::~Key()
	{
		delete mKey;
	}

	Key &Key::operator=(const Key &key)
	{
		if (this != &key)
		{
			IKey *copy = key.mKey != 0 ? key.mKey->Clone() : 0;
			delete mKey;
			mKey = copy;
		}
		return *this;
	}

	void Key::Import(const Blob &keyblob)
	{
		if (mKey == 0)
			mKey = detail::ImportECKeyBlob(keyblob);
		else
			mKey->Import(keyblob);
	}

	void Key::Export(Blob &keyblob, export_t type) const { Checked(mKey)->Export(keyblob, type); }
	void Key::SetParam(paramid_t id, const Blob &blob) { Checked(mKey)->SetParam(id, blob); }
	void Key::SetParam(paramid_t id, int val) { Checked(mKey)->SetParam(id, val); }
	void Key::SetParam(paramid_t id, const char *cstr) { Checked(mKey)->SetParam(id, cstr); }
	int Key::GetParam(paramid_t id) const { return Checked(mKey)->GetParam(id); }
	void Key::GetParam(paramid_t id, Blob &blob) const { Checked(mKey)->GetParam(id, blob); }

	void Key::Generate(IAlgorithm *prng)
	{
		IRNGAlg *rng = 0;
		if (prng != 0)
		{
			rng = dynamic_cast<IRNGAlg *>(prng);
			if (rng == 0)
				throw InvalidAlgorithmParameterException("not a random number generator", "Key::Generate");
		}
		Checked(mKey)->Generate(rng);
	}

	void Key::Derive(const Blob &data, const Blob &salt) { Checked(mKey)->Derive(data, salt); }

	IAlgorithm *Key::CreateAlgorithm(mode_t Mode) const { return Checked(mKey)->CreateAlgorithm(Mode); }
	IAlgorithm *Key::CreateAlgorithm(mode_t Mode, const Blob &data) const { return Checked(mKey)->CreateAlgorithm(Mode, data); }

	IKey *Key::GetPointer() { return mKey; }
	const IKey *Key::GetPointer() const { return mKey; }
	Key::operator IKey *() { return mKey; }
	Key::operator const IKey *() const { return mKey; }

	IKey *Key::ReleasePointer()
	{
		IKey *p = mKey;
		mKey = 0;
		return p;
	}
} // namespace act
