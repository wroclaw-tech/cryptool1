#ifndef ACT_COMPAT_INTERNAL_H
#define ACT_COMPAT_INTERNAL_H

#include "actAlgorithm.h"
#include "actBlob.h"
#include "actException.h"
#include "actIAlgorithm.h"
#include "actIKey.h"
#include "actIRNGAlg.h"
#include "actMode.h"
#include "actState.h"

#include <openssl/bn.h>
#include <openssl/ec.h>

#include <memory>
#include <string>

namespace act
{
	namespace detail
	{
		struct BnFree { void operator()(BIGNUM *p) const { BN_clear_free(p); } };
		struct BnCtxFree { void operator()(BN_CTX *p) const { BN_CTX_free(p); } };
		struct GroupFree { void operator()(EC_GROUP *p) const { EC_GROUP_free(p); } };
		struct PointFree { void operator()(EC_POINT *p) const { EC_POINT_clear_free(p); } };

		typedef std::unique_ptr<BIGNUM, BnFree> BnPtr;
		typedef std::unique_ptr<BN_CTX, BnCtxFree> BnCtxPtr;
		typedef std::unique_ptr<EC_GROUP, GroupFree> GroupPtr;
		typedef std::unique_ptr<EC_POINT, PointFree> PointPtr;

		BnPtr NewBn();
		BnCtxPtr NewBnCtx();

		// Parses "[-]0x<hex>" or "[-]<decimal>"; whitespace is ignored. Returns null on error.
		BnPtr ParseNumber(const char *text);
		// Big-endian two's complement, minimal length (at least one byte).
		Blob BnToTwosComplement(const BIGNUM *bn);
		// Big-endian unsigned or two's complement with a leading zero byte.
		BnPtr BlobToBn(const Blob &b);
		Blob BnToPadded(const BIGNUM *bn, size_t len);

		void RandomBytes(byte *out, size_t n, IRNGAlg *prng);

		// Minimal DER helpers.
		void DerAppend(Blob &out, byte tag, const Blob &content);
		void DerAppend(Blob &out, byte tag, const byte *content, size_t len);
		struct DerItem
		{
			byte tag;
			const byte *data;
			size_t len;
		};
		// Reads one TLV at pos (advancing pos). Returns false on malformed input.
		bool DerRead(const byte *buf, size_t size, size_t &pos, DerItem &item);

		Blob Digest(const char *name, const Blob &data);
		Blob X963Kdf(const char *digest, const Blob &secret, const Blob &sharedInfo, size_t len);
		Blob Hmac(const char *digest, const Blob &key, const Blob &data);

		// Base for algorithms whose output is collected in a buffer and drained by Read().
		class BufferedAlgorithm : public IAlgorithm
		{
		public:
			BufferedAlgorithm() : mStatus(READY) {}

			size_t GetAvailableSize() const { return mOut.size(); }
			size_t Read(Blob &output, size_t max);
			size_t Read(byte *outbuffer, size_t buffersize);
			status_t GetStatus() const { return mStatus; }

		protected:
			Blob mOut;
			status_t mStatus;
		};

		IKey *CreateIESKey();
		IKey *CreateBlockCipherKey();
		IKey *ImportECKeyBlob(const Blob &keyblob);
		IAlgorithm *CreateHashAlgorithm(const char *name);
	} // namespace detail
} // namespace act

#endif // ACT_COMPAT_INTERNAL_H
