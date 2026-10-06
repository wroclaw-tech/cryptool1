#include "actInternal.h"
#include "actInit.h"
#include "actRNGKit.h"
#include "actDefaultRNG.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstring>
#include <string>

namespace act
{
	namespace detail
	{
		size_t BufferedAlgorithm::Read(Blob &output, size_t max)
		{
			size_t n = mOut.size();
			if (max != 0 && max < n)
				n = max;
			output.assign(mOut.begin(), mOut.begin() + n);
			mOut.erase(mOut.begin(), mOut.begin() + n);
			return n;
		}

		size_t BufferedAlgorithm::Read(byte *outbuffer, size_t buffersize)
		{
			size_t n = mOut.size() < buffersize ? mOut.size() : buffersize;
			if (n != 0)
				std::memcpy(outbuffer, mOut.begin(), n);
			mOut.erase(mOut.begin(), mOut.begin() + n);
			return n;
		}

		class HashAlgorithm : public BufferedAlgorithm
		{
		public:
			explicit HashAlgorithm(const EVP_MD *md) : mCtx(EVP_MD_CTX_new())
			{
				if (mCtx == 0 || EVP_DigestInit_ex(mCtx, md, 0) != 1)
				{
					EVP_MD_CTX_free(mCtx);
					throw AlgorithmException("hash initialisation failed", "HashAlgorithm");
				}
			}
			~HashAlgorithm() { EVP_MD_CTX_free(mCtx); }

			void Write(const Blob &input) { Write(input.begin(), input.size()); }
			void Write(const byte *input, size_t insize)
			{
				if (mStatus == IS_FINALIZED)
					throw AlgorithmException("algorithm already finalized", "HashAlgorithm::Write");
				if (insize != 0 && EVP_DigestUpdate(mCtx, input, insize) != 1)
					throw AlgorithmException("hash update failed", "HashAlgorithm::Write");
			}
			void Finalize()
			{
				if (mStatus == IS_FINALIZED)
					return;
				unsigned char md[EVP_MAX_MD_SIZE];
				unsigned int len = 0;
				if (EVP_DigestFinal_ex(mCtx, md, &len) != 1)
					throw AlgorithmException("hash finalisation failed", "HashAlgorithm::Finalize");
				mOut.insert(mOut.end(), md, md + len);
				mStatus = IS_FINALIZED;
			}

		private:
			EVP_MD_CTX *mCtx;
		};

		IAlgorithm *CreateHashAlgorithm(const char *name)
		{
			static const struct { const char *act; const char *ossl; } names[] = {
				{"MD5", "MD5"}, {"SHA1", "SHA1"}, {"SHA224", "SHA224"}, {"SHA256", "SHA256"},
				{"SHA384", "SHA384"}, {"SHA512", "SHA512"}, {"RIPEMD160", "RIPEMD160"},
			};
			for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
			{
				if (std::strcmp(name, names[i].act) != 0)
					continue;
				EVP_MD *md = EVP_MD_fetch(0, names[i].ossl, 0);
				if (md == 0)
					break;
				try
				{
					IAlgorithm *alg = new HashAlgorithm(md);
					EVP_MD_free(md);
					return alg;
				}
				catch (...)
				{
					EVP_MD_free(md);
					throw;
				}
			}
			return 0;
		}

		class OpenSSLRng : public IRNGAlg
		{
		public:
			explicit OpenSSLRng(CreateRNGPtr factory) : mFactory(factory) {}

			void Write(const Blob &input) { Write(input.begin(), input.size()); }
			void Write(const byte *input, size_t insize)
			{
				if (insize != 0)
					RAND_seed(input, static_cast<int>(insize));
			}
			void Finalize() {}
			size_t GetAvailableSize() const { return 0; }
			size_t Read(Blob &output, size_t max)
			{
				size_t n = max != 0 ? max : output.size();
				output.resize(n);
				RandomBytes(output.begin(), n, 0);
				return n;
			}
			size_t Read(byte *outbuffer, size_t buffersize)
			{
				RandomBytes(outbuffer, buffersize, 0);
				return buffersize;
			}
			status_t GetStatus() const { return READY; }

			IRNGAlg *Clone() const { return new OpenSSLRng(mFactory); }
			void Import(const Blob &keyblob) { Write(keyblob); }
			void Export(Blob &keyblob) const { keyblob.clear(); }
			void SetParam(paramid_t, const Blob &blob) { Write(blob); }
			void SetParam(paramid_t, int) {}
			void SetParam(paramid_t, const char *) {}
			int GetParam(paramid_t) const { return 0; }
			void GetParam(paramid_t, Blob &blob) const { blob.clear(); }
			void *GetCreatePointer() const { return reinterpret_cast<void *>(mFactory); }

		private:
			CreateRNGPtr mFactory;
		};
	} // namespace detail

	IRNGAlg *CreateBBS() { return new detail::OpenSSLRng(&CreateBBS); }
	IRNGAlg *CreateFIPS186() { return new detail::OpenSSLRng(&CreateFIPS186); }
	IRNGAlg *CreateDevRandomRNG() { return new detail::OpenSSLRng(&CreateDevRandomRNG); }

	CreateRNGPtr CreateFastRNG = &CreateFIPS186;
	CreateRNGPtr CreateStrongRNG = &CreateBBS;

	namespace
	{
		bool g_isInit = false;
	}

	const char *GetVersion()
	{
		return "cvact_compat 1.4.6 (OpenSSL backend)";
	}

	bool GetIsInit() noexcept { return g_isInit; }
	void SetIsInit(bool bIsInit) noexcept { g_isInit = bIsInit; }

	void Init(bool bAlwaysInit)
	{
		if (g_isInit && !bAlwaysInit)
			return;
		CreateFastRNG = &CreateFIPS186;
		CreateStrongRNG = &CreateBBS;
		g_isInit = true;
	}

	Algorithm::Algorithm(IAlgorithm *alg) : mAlg(alg)
	{
		if (mAlg == 0)
			throw NullPointerException("null algorithm", "Algorithm::Algorithm");
	}

	Algorithm::Algorithm(const IKey *key, mode_t mode) : mAlg(0)
	{
		if (key == 0)
			throw NullPointerException("null key", "Algorithm::Algorithm");
		mAlg = key->CreateAlgorithm(mode);
		if (mAlg == 0)
			throw InvalidAlgorithmParameterException("mode not supported by key", "Algorithm::Algorithm");
	}

	Algorithm::Algorithm(const IKey *key, mode_t mode, const Blob &data) : mAlg(0)
	{
		if (key == 0)
			throw NullPointerException("null key", "Algorithm::Algorithm");
		mAlg = key->CreateAlgorithm(mode, data);
		if (mAlg == 0)
			throw InvalidAlgorithmParameterException("mode not supported by key", "Algorithm::Algorithm");
	}

	Algorithm::Algorithm(const char *name) : mAlg(0)
	{
		if (name == 0)
			throw NullPointerException("null algorithm name", "Algorithm::Algorithm");
		if (std::strcmp(name, "BBS") == 0)
			mAlg = CreateBBS();
		else if (std::strcmp(name, "FIPS186") == 0)
			mAlg = CreateFIPS186();
		else if (std::strcmp(name, "DevRandom") == 0)
			mAlg = CreateDevRandomRNG();
		else
			mAlg = detail::CreateHashAlgorithm(name);
		if (mAlg == 0)
			throw NoSuchAlgorithmException("unknown algorithm", "Algorithm::Algorithm");
	}

	Algorithm::~Algorithm() { delete mAlg; }

	void Algorithm::Write(const Blob &indata) { mAlg->Write(indata); }
	void Algorithm::Write(const byte *indata, size_t insize) { mAlg->Write(indata, insize); }
	void Algorithm::Finalize() { mAlg->Finalize(); }
	size_t Algorithm::Read(Blob &outdata, size_t max) { return mAlg->Read(outdata, max); }
	size_t Algorithm::Read(byte *outbuffer, size_t buffersize) { return mAlg->Read(outbuffer, buffersize); }
	size_t Algorithm::GetAvailableSize() const { return mAlg->GetAvailableSize(); }
	status_t Algorithm::GetStatus() const { return mAlg->GetStatus(); }

	Algorithm::operator IAlgorithm *() { return mAlg; }
	Algorithm::operator const IAlgorithm *() const { return mAlg; }
	IAlgorithm *Algorithm::GetPointer() { return mAlg; }
	const IAlgorithm *Algorithm::GetPointer() const { return mAlg; }

	IAlgorithm *Algorithm::ReleasePointer()
	{
		IAlgorithm *p = mAlg;
		mAlg = 0;
		return p;
	}
} // namespace act
