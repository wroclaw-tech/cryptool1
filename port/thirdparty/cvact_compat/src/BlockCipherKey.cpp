// "BlockCipher" key: AES (alias Rijndael) or TripleDES in ECB/CBC/CFB/OFB/CTR mode,
// PKCS#5 padding (or NOPAD) for ECB/CBC. Without an explicit IV a random IV is
// generated on encryption and prepended to the ciphertext; decryption without an
// explicit IV takes the IV from the first block of the ciphertext.
#include "actInternal.h"

#include <openssl/evp.h>

#include <cstring>
#include <string>

namespace act
{
	namespace detail
	{
		namespace
		{
			struct CipherInfo
			{
				const char *name;
				size_t blockSize;
				size_t keySizes[3];
			};

			const CipherInfo kCiphers[] = {
				{"AES", 16, {16, 24, 32}},
				{"TripleDES", 8, {16, 24, 0}},
			};

			const CipherInfo *FindCipher(const std::string &name)
			{
				std::string n = name == "Rijndael" ? "AES" : name;
				for (size_t i = 0; i < sizeof(kCiphers) / sizeof(kCiphers[0]); ++i)
					if (n == kCiphers[i].name)
						return &kCiphers[i];
				return 0;
			}

			bool IsSupportedMode(const std::string &mode)
			{
				return mode == "ECB" || mode == "CBC" || mode == "CFB" || mode == "OFB" || mode == "CTR";
			}

			std::string EvpCipherName(const CipherInfo &info, const std::string &mode, size_t keySize)
			{
				if (std::strcmp(info.name, "AES") == 0)
					return "AES-" + std::to_string(keySize * 8) + "-" + mode;
				return std::string(keySize == 16 ? "DES-EDE-" : "DES-EDE3-") + mode;
			}

			std::string ToString(const Blob &b)
			{
				return std::string(reinterpret_cast<const char *>(b.begin()), b.size());
			}

			class BlockCipherAlgorithm : public BufferedAlgorithm
			{
			public:
				BlockCipherAlgorithm(const std::string &evpName, const Blob &key, const Blob &iv,
					bool explicitIV, bool encrypt, bool padding)
					: mCtx(EVP_CIPHER_CTX_new()), mCipher(EVP_CIPHER_fetch(0, evpName.c_str(), 0)),
					  mKey(key), mEncrypt(encrypt), mPadding(padding), mStarted(false), mIVLen(0)
				{
					if (mCtx == 0 || mCipher == 0)
					{
						Cleanup();
						throw NoSuchAlgorithmException("cipher not available", "BlockCipherAlgorithm");
					}
					try
					{
						mIVLen = static_cast<size_t>(EVP_CIPHER_get_iv_length(mCipher));
						if (mIVLen == 0 || explicitIV)
						{
							if (mIVLen != 0 && iv.size() != mIVLen)
								throw InvalidAlgorithmParameterException("invalid IV length", "BlockCipherAlgorithm");
							Start(iv);
						}
						else if (mEncrypt)
						{
							Blob random(mIVLen);
							RandomBytes(random.begin(), random.size(), 0);
							mOut.append(random);
							Start(random);
						}
					}
					catch (...)
					{
						Cleanup();
						throw;
					}
				}

				~BlockCipherAlgorithm() { Cleanup(); }

				void Write(const Blob &input) { Write(input.begin(), input.size()); }

				void Write(const byte *input, size_t insize)
				{
					if (mStatus == IS_FINALIZED)
						throw AlgorithmException("algorithm already finalized", "BlockCipherAlgorithm::Write");
					if (!mStarted)
					{
						size_t take = mIVLen - mPendingIV.size();
						if (take > insize)
							take = insize;
						mPendingIV.insert(mPendingIV.end(), input, input + take);
						input += take;
						insize -= take;
						if (mPendingIV.size() < mIVLen)
							return;
						Start(mPendingIV);
					}
					if (insize == 0)
						return;
					size_t pos = mOut.size();
					mOut.resize(pos + insize + EVP_MAX_BLOCK_LENGTH);
					int outl = 0;
					if (EVP_CipherUpdate(mCtx, mOut.begin() + pos, &outl, input, static_cast<int>(insize)) != 1)
					{
						mOut.resize(pos);
						throw AlgorithmException("cipher update failed", "BlockCipherAlgorithm::Write");
					}
					mOut.resize(pos + static_cast<size_t>(outl));
				}

				void Finalize()
				{
					if (mStatus == IS_FINALIZED)
						return;
					if (!mStarted)
					{
						mStatus = DECRYPT_ERROR;
						throw AlgorithmException("ciphertext too short", "BlockCipherAlgorithm::Finalize");
					}
					size_t pos = mOut.size();
					mOut.resize(pos + EVP_MAX_BLOCK_LENGTH);
					int outl = 0;
					if (EVP_CipherFinal_ex(mCtx, mOut.begin() + pos, &outl) != 1)
					{
						mOut.clear();
						mStatus = DECRYPT_ERROR;
						throw PaddingException("bad padding, wrong key or wrong data length", "BlockCipherAlgorithm::Finalize");
					}
					mOut.resize(pos + static_cast<size_t>(outl));
					mStatus = IS_FINALIZED;
				}

			private:
				void Start(const Blob &iv)
				{
					if (EVP_CipherInit_ex2(mCtx, mCipher, mKey.begin(), iv.empty() ? 0 : iv.begin(),
							mEncrypt ? 1 : 0, 0) != 1
						|| EVP_CIPHER_CTX_set_padding(mCtx, mPadding ? 1 : 0) != 1)
						throw AlgorithmException("cipher initialisation failed", "BlockCipherAlgorithm");
					mStarted = true;
				}

				void Cleanup()
				{
					EVP_CIPHER_CTX_free(mCtx);
					EVP_CIPHER_free(mCipher);
					mCtx = 0;
					mCipher = 0;
				}

				EVP_CIPHER_CTX *mCtx;
				EVP_CIPHER *mCipher;
				Blob mKey;
				Blob mPendingIV;
				bool mEncrypt;
				bool mPadding;
				bool mStarted;
				size_t mIVLen;
			};

			IKey *NewBlockCipherKey();

			class BlockCipherKey : public IKey
			{
			public:
				BlockCipherKey()
					: mCipher("AES"), mMode("CBC"), mPadding("PKCS5"), mDerivator("KDF1"), mHash("SHA512"),
					  mKeySize(10), mHasKey(false), mHasIV(false) {}

				IKey *Clone() const { return new BlockCipherKey(*this); }

				void Import(const Blob &keyblob) { SetParam(RAWKEY, keyblob); }

				void Export(Blob &keyblob, export_t type) const
				{
					if (type != DEFAULT && type != SECRET)
						throw InvalidKeyException("unsupported export type", "BlockCipherKey::Export");
					GetParam(RAWKEY, keyblob);
				}

				void SetParam(paramid_t id, const Blob &blob)
				{
					switch (id)
					{
					case RAWKEY:
						if (!IsSupportedKeySize(blob.size()))
							throw InvalidKeyException("unsupported key size", "BlockCipherKey::SetParam");
						mRawKey = blob;
						mKeySize = static_cast<int>(blob.size());
						mHasKey = true;
						break;
					case IV:
						if (blob.size() != Info().blockSize)
							throw InvalidAlgorithmParameterException("invalid IV length", "BlockCipherKey::SetParam");
						mIV = blob;
						mHasIV = true;
						break;
					case CIPHER:
					case BCMODE:
					case PADDING:
					case DERIVATOR:
					case HASH:
						SetParam(id, ToString(blob).c_str());
						break;
					default:
						throw InvalidKeyException("unsupported parameter", "BlockCipherKey::SetParam");
					}
				}

				void SetParam(paramid_t id, int val)
				{
					if (id != KEYSIZE || val <= 0)
						throw InvalidKeyException("unsupported parameter", "BlockCipherKey::SetParam");
					mKeySize = val;
				}

				void SetParam(paramid_t id, const char *cstr)
				{
					if (cstr == 0)
						throw NullPointerException("null parameter", "BlockCipherKey::SetParam");
					std::string value(cstr);
					switch (id)
					{
					case CIPHER:
						if (FindCipher(value) == 0)
							throw NoSuchAlgorithmException("unsupported block cipher", "BlockCipherKey::SetParam");
						mCipher = value == "Rijndael" ? "AES" : value;
						mRawKey.clear();
						mHasKey = false;
						mIV.clear();
						mHasIV = false;
						break;
					case BCMODE:
						if (!IsSupportedMode(value))
							throw NoSuchAlgorithmException("unsupported block cipher mode", "BlockCipherKey::SetParam");
						mMode = value;
						break;
					case PADDING:
						if (value != "PKCS5" && value != "NOPAD")
							throw NoSuchAlgorithmException("unsupported padding", "BlockCipherKey::SetParam");
						mPadding = value;
						break;
					case DERIVATOR:
						if (value != "KDF1" && value != "KDF2" && value != "X963KDF")
							throw NoSuchAlgorithmException("unsupported derivator", "BlockCipherKey::SetParam");
						mDerivator = value;
						break;
					case HASH:
						Digest(value.c_str(), Blob());
						mHash = value;
						break;
					default:
						throw InvalidKeyException("unsupported parameter", "BlockCipherKey::SetParam");
					}
				}

				int GetParam(paramid_t id) const
				{
					switch (id)
					{
					case KEYSIZE:
						return mHasKey ? static_cast<int>(mRawKey.size()) : static_cast<int>(EffectiveKeySize());
					case BLOCKSIZE:
						return static_cast<int>(Info().blockSize);
					default:
						throw InvalidKeyException("unsupported parameter", "BlockCipherKey::GetParam");
					}
				}

				void GetParam(paramid_t id, Blob &blob) const
				{
					switch (id)
					{
					case RAWKEY:
						if (!mHasKey)
							throw InvalidKeyException("no key available", "BlockCipherKey::GetParam");
						blob = mRawKey;
						break;
					case IV:
						blob = mIV;
						break;
					case CIPHER: blob = Blob(mCipher.c_str()); break;
					case BCMODE: blob = Blob(mMode.c_str()); break;
					case PADDING: blob = Blob(mPadding.c_str()); break;
					case DERIVATOR: blob = Blob(mDerivator.c_str()); break;
					case HASH: blob = Blob(mHash.c_str()); break;
					default:
						throw InvalidKeyException("unsupported parameter", "BlockCipherKey::GetParam");
					}
				}

				void Generate(IRNGAlg *prng)
				{
					Blob key(EffectiveKeySize());
					RandomBytes(key.begin(), key.size(), prng);
					SetParam(RAWKEY, key);
				}

				// KDF1 (IEEE 1363): Hash(data || salt); KDF2/X963KDF: ANSI X9.63 KDF with salt as SharedInfo.
				void Derive(const Blob &data, const Blob &salt)
				{
					size_t len = EffectiveKeySize();
					Blob key;
					if (mDerivator == "KDF1")
					{
						Blob input(data);
						input.append(salt);
						key = Digest(mHash.c_str(), input);
						SecureZero(input.begin(), input.size());
						if (key.size() < len)
							throw InvalidAlgorithmParameterException("KDF1 output too short for key size", "BlockCipherKey::Derive");
						key.resize(len);
					}
					else
						key = X963Kdf(mHash.c_str(), data, salt, len);
					SetParam(RAWKEY, key);
				}

				IAlgorithm *CreateAlgorithm(mode_t Mode) const
				{
					IAlgorithm *alg = Create(Mode, mIV, mHasIV);
					if (Mode == ENCRYPT)
					{
						mIV.clear();
						mHasIV = false;
					}
					return alg;
				}

				IAlgorithm *CreateAlgorithm(mode_t Mode, const Blob &data) const
				{
					return Create(Mode, data, true);
				}

				void *GetCreatePointer() const { return reinterpret_cast<void *>(&NewBlockCipherKey); }

			private:
				const CipherInfo &Info() const { return *FindCipher(mCipher); }

				bool IsSupportedKeySize(size_t n) const
				{
					const CipherInfo &info = Info();
					for (size_t i = 0; i < 3; ++i)
						if (info.keySizes[i] != 0 && info.keySizes[i] == n)
							return true;
					return false;
				}

				size_t EffectiveKeySize() const
				{
					const CipherInfo &info = Info();
					size_t largest = 0;
					for (size_t i = 0; i < 3; ++i)
					{
						size_t s = info.keySizes[i];
						if (s == 0)
							continue;
						if (s >= static_cast<size_t>(mKeySize))
							return s;
						largest = s;
					}
					return largest;
				}

				IAlgorithm *Create(mode_t Mode, const Blob &iv, bool explicitIV) const
				{
					if (Mode != ENCRYPT && Mode != DECRYPT)
						throw InvalidAlgorithmParameterException("unsupported mode", "BlockCipherKey::CreateAlgorithm");
					if (!mHasKey)
						throw InvalidKeyException("no key available", "BlockCipherKey::CreateAlgorithm");
					bool padding = (mMode == "CBC" || mMode == "ECB") && mPadding == "PKCS5";
					return new BlockCipherAlgorithm(EvpCipherName(Info(), mMode, mRawKey.size()),
						mRawKey, iv, explicitIV, Mode == ENCRYPT, padding);
				}

				std::string mCipher;
				std::string mMode;
				std::string mPadding;
				std::string mDerivator;
				std::string mHash;
				int mKeySize;
				Blob mRawKey;
				mutable Blob mIV;
				bool mHasKey;
				mutable bool mHasIV;
			};

			IKey *NewBlockCipherKey() { return new BlockCipherKey(); }
		} // namespace

		IKey *CreateBlockCipherKey() { return NewBlockCipherKey(); }
	} // namespace detail
} // namespace act
