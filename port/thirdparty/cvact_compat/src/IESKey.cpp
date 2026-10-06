// "IES" key: EC-IES over prime curves (agreement ECDH, MAC HashMAC).
//
// Ciphertext = R || C || T with
//   R  ephemeral (sender) public key, uncompressed X9.62 point 04 || X || Y
//   Z  x-coordinate of d * Q (classic ECDH, no cofactor multiplication), field length
//   K  = ANSI X9.63 KDF with SHA-1 over (R || Z), SharedInfo = SHAREDDATA_1,
//        length |M| + 20; K = EK (|M| bytes) || MK (20 bytes)
//   C  = M xor EK
//   T  = HMAC-SHA1(MK, C || SHAREDDATA_2), 20 bytes
// Public keys are exported as DER SubjectPublicKeyInfo (id-ecPublicKey, named curve),
// private keys as DER ECPrivateKey (RFC 5915).
#include "actInternal.h"

#include <openssl/crypto.h>
#include <openssl/obj_mac.h>
#include <openssl/objects.h>

#include <cstring>
#include <string>

namespace act
{
	namespace detail
	{
		namespace
		{
			const char *const kKdfHash = "SHA1";
			const char *const kMacHash = "SHA1";
			const size_t kMacKeyLen = 20;
			const size_t kTagLen = 20;

			struct CurveDef
			{
				int id;
				int nid;
				const char *p, *a, *b, *gx, *gy, *n;
			};

			// Explicit parameters are a fallback for OpenSSL builds without these named
			// curves; they match the curve definitions in CrypTool's libec.
			const CurveDef kCurves[] = {
				{SECGp160r1, NID_secp160r1,
				 "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFF",
				 "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFC",
				 "1C97BEFC54BD7A8B65ACF89F81D4D4ADC565FA45",
				 "4A96B5688EF573284664698968C38BB913CBFC82",
				 "23A628553168947D59DCC912042351377AC5FB32",
				 "100000000000000000001F4C8F927AED3CA752257"},
				{SECGp160r2, NID_secp160r2, 0, 0, 0, 0, 0, 0},
				{SECGp160k1, NID_secp160k1, 0, 0, 0, 0, 0, 0},
				{ANSIp192r1, NID_X9_62_prime192v1,
				 "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFF",
				 "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFC",
				 "64210519E59C80E70FA7E9AB72243049FEB8DEECC146B9B1",
				 "188DA80EB03090F67CBF20EB43A18800F4FF0AFD82FF1012",
				 "07192B95FFC8DA78631011ED6B24CDD573F977A11E794811",
				 "FFFFFFFFFFFFFFFFFFFFFFFF99DEF836146BC9B1B4D22831"},
				{ANSIp192r2, NID_X9_62_prime192v2,
				 "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFF",
				 "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFC",
				 "CC22D6DFB95C6B25E49C0D6364A4E5980C393AA21668D953",
				 "EEA2BAE7E1497842F2DE7769CFE9C989C072AD696F48034A",
				 "6574D11D69B6EC7A672BB82A083DF2F2B0847DE970B2DE15",
				 "FFFFFFFFFFFFFFFFFFFFFFFE5FB1A724DC80418648D8DD31"},
				{ANSIp192r3, NID_X9_62_prime192v3,
				 "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFF",
				 "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFC",
				 "22123DC2395A05CAA7423DAECCC94760A7D462256BD56916",
				 "7D29778100C65A1DA1783716588DCE2B8B4AEE8E228F1896",
				 "38A90F22637337334B49DCB66A6DC8F9978ACA7648A943B0",
				 "FFFFFFFFFFFFFFFFFFFFFFFF7A62D031C83F4294F640EC13"},
				{NISTp192r1, NID_X9_62_prime192v1, 0, 0, 0, 0, 0, 0},
				{SECGp192r1, NID_X9_62_prime192v1, 0, 0, 0, 0, 0, 0},
				{SECGp192k1, NID_secp192k1, 0, 0, 0, 0, 0, 0},
				{NISTp224r1, NID_secp224r1, 0, 0, 0, 0, 0, 0},
				{SECGp224r1, NID_secp224r1, 0, 0, 0, 0, 0, 0},
				{SECGp224k1, NID_secp224k1, 0, 0, 0, 0, 0, 0},
				{ANSIp239r1, NID_X9_62_prime239v1,
				 "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFF8000000000007FFFFFFFFFFF",
				 "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFF8000000000007FFFFFFFFFFC",
				 "6B016C3BDCF18941D0D654921475CA71A9DB2FB27D1D37796185C2942C0A",
				 "0FFA963CDCA8816CCC33B8642BEDF905C3D358573D3F27FBBD3B3CB9AAAF",
				 "7DEBE8E4E90A5DAE6E4054CA530BA04654B36818CE226B39FCCB7B02F1AE",
				 "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFF9E5E9A9F5D9071FBD1522688909D0B"},
				{ANSIp239r2, NID_X9_62_prime239v2,
				 "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFF8000000000007FFFFFFFFFFF",
				 "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFF8000000000007FFFFFFFFFFC",
				 "617FAB6832576CBBFED50D99F0249C3FEE58B94BA0038C7AE84C8C832F2C",
				 "38AF09D98727705120C921BB5E9E26296A3CDCF2F35757A0EAFD87B830E7",
				 "5B0125E4DBEA0EC7206DA0FC01D9B081329FB555DE6EF460237DFF8BE4BA",
				 "7FFFFFFFFFFFFFFFFFFFFFFF800000CFA7E8594377D414C03821BC582063"},
				{ANSIp239r3, NID_X9_62_prime239v3,
				 "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFF8000000000007FFFFFFFFFFF",
				 "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFF8000000000007FFFFFFFFFFC",
				 "255705FA2A306654B1F4CB03D6A750A30C250102D4988717D9BA15AB6D3E",
				 "6768AE8E18BB92CFCF005C949AA2C6D94853D0E660BBF854B1C9505FE95A",
				 "1607E6898F390C06BC1D552BAD226F3B6FCFE48B6E818499AF18E3ED6CF3",
				 "7FFFFFFFFFFFFFFFFFFFFFFF7FFFFF975DEB41B3A6057C3C432146526551"},
				{ANSIp256r1, NID_X9_62_prime256v1,
				 "FFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFF",
				 "FFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFC",
				 "5AC635D8AA3A93E7B3EBBD55769886BC651D06B0CC53B0F63BCE3C3E27D2604B",
				 "6B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296",
				 "4FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5",
				 "FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551"},
				{NISTp256r1, NID_X9_62_prime256v1, 0, 0, 0, 0, 0, 0},
				{SECGp256r1, NID_X9_62_prime256v1, 0, 0, 0, 0, 0, 0},
				{SECGp256k1, NID_secp256k1, 0, 0, 0, 0, 0, 0},
				{NISTp384r1, NID_secp384r1, 0, 0, 0, 0, 0, 0},
				{NISTp521r1, NID_secp521r1, 0, 0, 0, 0, 0, 0},
			};

			const CurveDef *FindCurve(int id)
			{
				for (size_t i = 0; i < sizeof(kCurves) / sizeof(kCurves[0]); ++i)
					if (kCurves[i].id == id)
						return &kCurves[i];
				return 0;
			}

			BnPtr HexBn(const char *hex)
			{
				BIGNUM *bn = 0;
				if (!BN_hex2bn(&bn, hex))
					throw BadAllocException("BN_hex2bn failed", "IESKey");
				return BnPtr(bn);
			}

			GroupPtr ExplicitGroup(const CurveDef &def)
			{
				BnCtxPtr ctx = NewBnCtx();
				BnPtr p = HexBn(def.p), a = HexBn(def.a), b = HexBn(def.b);
				BnPtr gx = HexBn(def.gx), gy = HexBn(def.gy), n = HexBn(def.n);
				BnPtr h = NewBn();
				BN_one(h.get());
				GroupPtr group(EC_GROUP_new_curve_GFp(p.get(), a.get(), b.get(), ctx.get()));
				if (!group)
					return GroupPtr();
				PointPtr g(EC_POINT_new(group.get()));
				if (!g || EC_POINT_set_affine_coordinates(group.get(), g.get(), gx.get(), gy.get(), ctx.get()) != 1
					|| EC_GROUP_set_generator(group.get(), g.get(), n.get(), h.get()) != 1)
					return GroupPtr();
				EC_GROUP_set_curve_name(group.get(), def.nid);
				return group;
			}

			// Group for a curve id; ExplicitGroup() is used when preferExplicit is set or
			// the named curve is not built into the OpenSSL library.
			GroupPtr MakeGroup(const CurveDef &def, bool preferExplicit = false)
			{
				GroupPtr group;
				if (!preferExplicit)
					group.reset(EC_GROUP_new_by_curve_name(def.nid));
				if (!group && def.p != 0)
					group = ExplicitGroup(def);
				if (!group)
					throw NoSuchAlgorithmException("elliptic curve not available", "IESKey");
				return group;
			}

			Blob CurveOidTlv(int nid)
			{
				ASN1_OBJECT *obj = OBJ_nid2obj(nid);
				int len = obj != 0 ? i2d_ASN1_OBJECT(obj, 0) : 0;
				if (len <= 0)
					throw NoSuchAlgorithmException("unknown curve OID", "IESKey");
				Blob out(static_cast<size_t>(len));
				unsigned char *p = out.begin();
				i2d_ASN1_OBJECT(obj, &p);
				return out;
			}

			const byte kEcPublicKeyOid[] = {0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01};

			size_t FieldLen(const EC_GROUP *group)
			{
				return static_cast<size_t>((EC_GROUP_get_degree(group) + 7) / 8);
			}

			Blob PointToOctets(const EC_GROUP *group, const EC_POINT *pt, BN_CTX *ctx)
			{
				size_t len = EC_POINT_point2oct(group, pt, POINT_CONVERSION_UNCOMPRESSED, 0, 0, ctx);
				if (len == 0)
					throw InvalidKeyException("invalid point", "IESKey");
				Blob out(len);
				EC_POINT_point2oct(group, pt, POINT_CONVERSION_UNCOMPRESSED, out.begin(), len, ctx);
				return out;
			}

			PointPtr OctetsToPoint(const EC_GROUP *group, const byte *data, size_t len, BN_CTX *ctx)
			{
				PointPtr pt(EC_POINT_new(group));
				if (!pt || len == 0 || EC_POINT_oct2point(group, pt.get(), data, len, ctx) != 1
					|| EC_POINT_is_at_infinity(group, pt.get()) || EC_POINT_is_on_curve(group, pt.get(), ctx) != 1)
					return PointPtr();
				return pt;
			}

			// Length of the encoded point at the start of data, 0 if not a point encoding.
			size_t EncodedPointLen(const EC_GROUP *group, const byte *data, size_t len)
			{
				if (len == 0)
					return 0;
				size_t flen = FieldLen(group);
				if (data[0] == 0x04)
					return 1 + 2 * flen;
				if (data[0] == 0x02 || data[0] == 0x03)
					return 1 + flen;
				return 0;
			}

			Blob SharedSecret(const EC_GROUP *group, const BIGNUM *d, const EC_POINT *q, BN_CTX *ctx)
			{
				PointPtr s(EC_POINT_new(group));
				BnPtr x = NewBn();
				if (!s || EC_POINT_mul(group, s.get(), 0, q, d, ctx) != 1 || EC_POINT_is_at_infinity(group, s.get())
					|| EC_POINT_get_affine_coordinates(group, s.get(), x.get(), 0, ctx) != 1)
					throw InvalidKeyException("ECDH computation failed", "IESKey");
				return BnToPadded(x.get(), FieldLen(group));
			}

			void XorKdfStream(Blob &data, const Blob &key)
			{
				for (size_t i = 0; i < data.size(); ++i)
					data[i] ^= key[i];
			}

			class IESEncrypt : public BufferedAlgorithm
			{
			public:
				IESEncrypt(GroupPtr group, BnPtr ephemeralPriv, const Blob &ephemeralPub, PointPtr recipient,
					const Blob &shared1, const Blob &shared2)
					: mGroup(std::move(group)), mPriv(std::move(ephemeralPriv)), mR(ephemeralPub),
					  mRecipient(std::move(recipient)), mShared1(shared1), mShared2(shared2) {}

				void Write(const Blob &input) { mIn.append(input); }
				void Write(const byte *input, size_t insize) { mIn.insert(mIn.end(), input, input + insize); }

				void Finalize()
				{
					if (mStatus == IS_FINALIZED)
						return;
					BnCtxPtr ctx = NewBnCtx();
					Blob kdfIn(mR);
					kdfIn.append(SharedSecret(mGroup.get(), mPriv.get(), mRecipient.get(), ctx.get()));
					Blob k = X963Kdf(kKdfHash, kdfIn, mShared1, mIn.size() + kMacKeyLen);
					Blob c(mIn);
					XorKdfStream(c, k);
					Blob mk(k.begin() + mIn.size(), k.end());
					Blob macIn(c);
					macIn.append(mShared2);
					Blob tag = Hmac(kMacHash, mk, macIn);
					mOut.append(mR);
					mOut.append(c);
					mOut.insert(mOut.end(), tag.begin(), tag.begin() + kTagLen);
					mIn.clear();
					mStatus = IS_FINALIZED;
				}

			private:
				GroupPtr mGroup;
				BnPtr mPriv;
				Blob mR;
				PointPtr mRecipient;
				Blob mShared1, mShared2;
				Blob mIn;
			};

			class IESDecrypt : public BufferedAlgorithm
			{
			public:
				IESDecrypt(GroupPtr group, BnPtr priv, const Blob &shared1, const Blob &shared2)
					: mGroup(std::move(group)), mPriv(std::move(priv)), mShared1(shared1), mShared2(shared2) {}

				void Write(const Blob &input) { mIn.append(input); }
				void Write(const byte *input, size_t insize) { mIn.insert(mIn.end(), input, input + insize); }

				void Finalize()
				{
					if (mStatus == IS_FINALIZED)
						return;
					mStatus = DECRYPT_ERROR;
					BnCtxPtr ctx = NewBnCtx();
					size_t rlen = EncodedPointLen(mGroup.get(), mIn.begin(), mIn.size());
					if (rlen == 0 || mIn.size() < rlen + kTagLen)
						throw InvalidAlgorithmParameterException("invalid ECIES ciphertext", "IESDecrypt::Finalize");
					PointPtr r = OctetsToPoint(mGroup.get(), mIn.begin(), rlen, ctx.get());
					if (!r)
						throw InvalidAlgorithmParameterException("invalid ephemeral public key", "IESDecrypt::Finalize");
					size_t clen = mIn.size() - rlen - kTagLen;
					Blob kdfIn(mIn.begin(), mIn.begin() + rlen);
					kdfIn.append(SharedSecret(mGroup.get(), mPriv.get(), r.get(), ctx.get()));
					Blob k = X963Kdf(kKdfHash, kdfIn, mShared1, clen + kMacKeyLen);
					Blob c(mIn.begin() + rlen, mIn.begin() + rlen + clen);
					Blob mk(k.begin() + clen, k.end());
					Blob macIn(c);
					macIn.append(mShared2);
					Blob tag = Hmac(kMacHash, mk, macIn);
					if (CRYPTO_memcmp(tag.begin(), mIn.begin() + rlen + clen, kTagLen) != 0)
						throw AlgorithmException("ECIES MAC verification failed (wrong key or corrupted data)",
							"IESDecrypt::Finalize");
					XorKdfStream(c, k);
					mOut.append(c);
					mIn.clear();
					mStatus = IS_FINALIZED;
				}

			private:
				GroupPtr mGroup;
				BnPtr mPriv;
				Blob mShared1, mShared2;
				Blob mIn;
			};

			IKey *NewIESKey();

			class IESKey : public IKey
			{
			public:
				IESKey() : mCurve(SECGp160r1), mNewEphemeral(0) {}

				IESKey(const IESKey &o)
					: mCurve(o.mCurve), mShared1(o.mShared1), mShared2(o.mShared2), mNewEphemeral(o.mNewEphemeral)
				{
					if (o.mGroup)
						mGroup.reset(EC_GROUP_dup(o.mGroup.get()));
					if (o.mPriv)
						mPriv.reset(BN_dup(o.mPriv.get()));
					if (o.mPub)
						mPub.reset(EC_POINT_dup(o.mPub.get(), mGroup.get()));
					if (o.mPendingX)
						mPendingX.reset(BN_dup(o.mPendingX.get()));
					if (o.mPendingY)
						mPendingY.reset(BN_dup(o.mPendingY.get()));
				}

				IKey *Clone() const { return new IESKey(*this); }

				void Import(const Blob &keyblob)
				{
					const byte *buf = keyblob.begin();
					size_t size = keyblob.size(), pos = 0;
					DerItem top;
					if (!DerRead(buf, size, pos, top))
						throw InvalidKeyException("invalid key blob", "IESKey::Import");
					if (top.tag == 0x06)
					{
						SetCurve(CurveFromOid(buf, size));
						return;
					}
					if (top.tag != 0x30)
						throw InvalidKeyException("invalid key blob", "IESKey::Import");
					size_t ipos = 0;
					DerItem first;
					if (!DerRead(top.data, top.len, ipos, first))
						throw InvalidKeyException("invalid key blob", "IESKey::Import");
					if (first.tag == 0x30)
						ImportSpki(top, first, ipos);
					else if (first.tag == 0x02)
						ImportEcPrivateKey(top, ipos);
					else
						throw InvalidKeyException("invalid key blob", "IESKey::Import");
				}

				void Export(Blob &keyblob, export_t type) const
				{
					keyblob.clear();
					switch (type)
					{
					case PUBLIC:
						keyblob = Spki();
						break;
					case DEFAULT:
					case PRIVATE:
						keyblob = EcPrivateKey();
						break;
					case DOMAINPARAMS:
						keyblob = CurveOidTlv(FindCurve(mCurve)->nid);
						break;
					default:
						throw InvalidKeyException("unsupported export type", "IESKey::Export");
					}
				}

				void SetParam(paramid_t id, const Blob &blob)
				{
					switch (id)
					{
					case PRIVATEKEY:
						SetPrivate(BlobToBn(blob));
						break;
					case PUBLIC_X:
						mPendingX = BlobToBn(blob);
						TryBuildPublic();
						break;
					case PUBLIC_Y:
						mPendingY = BlobToBn(blob);
						TryBuildPublic();
						break;
					case PUBLICKEY:
					{
						BnCtxPtr ctx = NewBnCtx();
						PointPtr pt = OctetsToPoint(Group(), blob.begin(), blob.size(), ctx.get());
						if (!pt)
							throw InvalidKeyException("point not on curve", "IESKey::SetParam");
						mPriv.reset();
						mPub = std::move(pt);
						break;
					}
					case SHAREDDATA_1:
						mShared1 = blob;
						break;
					case SHAREDDATA_2:
						mShared2 = blob;
						break;
					default:
						throw InvalidKeyException("unsupported parameter", "IESKey::SetParam");
					}
				}

				void SetParam(paramid_t id, int val)
				{
					switch (id)
					{
					case CURVE:
						SetCurve(val);
						break;
					case NEWEPHEMERAL:
						mNewEphemeral = val;
						break;
					case COMPATIBLE:
						if (val == 0)
							throw InvalidKeyException("cofactor ECDH not supported", "IESKey::SetParam");
						break;
					default:
						throw InvalidKeyException("unsupported parameter", "IESKey::SetParam");
					}
				}

				void SetParam(paramid_t id, const char *cstr)
				{
					if (cstr == 0)
						throw NullPointerException("null parameter", "IESKey::SetParam");
					switch (id)
					{
					case PRIVATEKEY:
					case PUBLIC_X:
					case PUBLIC_Y:
					{
						BnPtr bn = ParseNumber(cstr);
						if (!bn || BN_is_negative(bn.get()))
							throw InvalidKeyException("invalid number", "IESKey::SetParam");
						if (id == PRIVATEKEY)
							SetPrivate(std::move(bn));
						else
						{
							(id == PUBLIC_X ? mPendingX : mPendingY) = std::move(bn);
							TryBuildPublic();
						}
						break;
					}
					case AGREEMENT:
						if (std::strcmp(cstr, "ECDH") != 0)
							throw NoSuchAlgorithmException("unsupported agreement", "IESKey::SetParam");
						break;
					case MAC:
						if (std::strcmp(cstr, "HashMAC") != 0)
							throw NoSuchAlgorithmException("unsupported MAC", "IESKey::SetParam");
						break;
					default:
						throw InvalidKeyException("unsupported parameter", "IESKey::SetParam");
					}
				}

				int GetParam(paramid_t id) const
				{
					switch (id)
					{
					case CURVE:
						return mCurve;
					case NEWEPHEMERAL:
						return mNewEphemeral;
					case COMPATIBLE:
						return 1;
					case KEYSIZE:
						return EC_GROUP_get_degree(Group());
					default:
						throw InvalidKeyException("unsupported parameter", "IESKey::GetParam");
					}
				}

				void GetParam(paramid_t id, Blob &blob) const
				{
					switch (id)
					{
					case PRIVATEKEY:
						if (!mPriv)
							throw InvalidKeyException("no private key", "IESKey::GetParam");
						blob = BnToTwosComplement(mPriv.get());
						break;
					case PUBLIC_X:
					case PUBLIC_Y:
					{
						if (!mPub)
							throw InvalidKeyException("no public key", "IESKey::GetParam");
						BnCtxPtr ctx = NewBnCtx();
						BnPtr x = NewBn(), y = NewBn();
						EC_POINT_get_affine_coordinates(Group(), mPub.get(), x.get(), y.get(), ctx.get());
						blob = BnToTwosComplement(id == PUBLIC_X ? x.get() : y.get());
						break;
					}
					case PUBLICKEY:
					{
						if (!mPub)
							throw InvalidKeyException("no public key", "IESKey::GetParam");
						BnCtxPtr ctx = NewBnCtx();
						blob = PointToOctets(Group(), mPub.get(), ctx.get());
						break;
					}
					case SHAREDDATA_1: blob = mShared1; break;
					case SHAREDDATA_2: blob = mShared2; break;
					case AGREEMENT: blob = Blob("ECDH"); break;
					case MAC: blob = Blob("HashMAC"); break;
					default:
						throw InvalidKeyException("unsupported parameter", "IESKey::GetParam");
					}
				}

				void Generate(IRNGAlg *prng)
				{
					BnPtr d = RandomScalar(Group(), prng);
					SetPrivate(std::move(d));
				}

				void Derive(const Blob &, const Blob &)
				{
					throw NotImplementedException("Derive is not supported by IES keys", "IESKey::Derive");
				}

				IAlgorithm *CreateAlgorithm(mode_t Mode) const
				{
					if (Mode == DECRYPT)
						return CreateDecrypt();
					if (Mode == ENCRYPT)
					{
						if (!mPub)
							throw InvalidKeyException("no public key", "IESKey::CreateAlgorithm");
						return CreateEncrypt(PointPtr(EC_POINT_dup(mPub.get(), Group())), true);
					}
					throw InvalidAlgorithmParameterException("unsupported mode", "IESKey::CreateAlgorithm");
				}

				IAlgorithm *CreateAlgorithm(mode_t Mode, const Blob &data) const
				{
					if (Mode == DECRYPT)
						return CreateDecrypt();
					if (Mode != ENCRYPT)
						throw InvalidAlgorithmParameterException("unsupported mode", "IESKey::CreateAlgorithm");
					return CreateEncrypt(ParsePeerPublicKey(data), mNewEphemeral != 0 || !mPriv);
				}

				void *GetCreatePointer() const { return reinterpret_cast<void *>(&NewIESKey); }

			private:
				EC_GROUP *Group() const
				{
					if (!mGroup)
						mGroup = MakeGroup(*FindCurve(mCurve));
					return mGroup.get();
				}

				void SetCurve(int id)
				{
					const CurveDef *def = FindCurve(id);
					if (def == 0)
						throw NoSuchAlgorithmException("unsupported curve", "IESKey::SetParam");
					GroupPtr group = MakeGroup(*def);
					mCurve = id;
					mGroup = std::move(group);
					mPriv.reset();
					mPub.reset();
					mPendingX.reset();
					mPendingY.reset();
				}

				int CurveFromOid(const byte *tlv, size_t len) const
				{
					for (size_t i = 0; i < sizeof(kCurves) / sizeof(kCurves[0]); ++i)
					{
						Blob oid = CurveOidTlv(kCurves[i].nid);
						if (oid.size() == len && std::memcmp(oid.begin(), tlv, len) == 0)
							return kCurves[i].id;
					}
					throw NoSuchAlgorithmException("unsupported curve", "IESKey::Import");
				}

				void SetPrivate(BnPtr d)
				{
					const BIGNUM *order = EC_GROUP_get0_order(Group());
					if (BN_is_zero(d.get()) || BN_is_negative(d.get()) || BN_cmp(d.get(), order) >= 0)
						throw InvalidKeyException("private key out of range", "IESKey::SetParam");
					BnCtxPtr ctx = NewBnCtx();
					PointPtr pub(EC_POINT_new(Group()));
					if (!pub || EC_POINT_mul(Group(), pub.get(), d.get(), 0, 0, ctx.get()) != 1)
						throw InvalidKeyException("public key computation failed", "IESKey::SetParam");
					mPriv = std::move(d);
					mPub = std::move(pub);
					mPendingX.reset();
					mPendingY.reset();
				}

				void TryBuildPublic()
				{
					if (!mPendingX || !mPendingY)
						return;
					BnCtxPtr ctx = NewBnCtx();
					BnPtr p = NewBn();
					EC_GROUP_get_curve(Group(), p.get(), 0, 0, ctx.get());
					PointPtr pt(EC_POINT_new(Group()));
					if (BN_is_negative(mPendingX.get()) || BN_is_negative(mPendingY.get())
						|| BN_cmp(mPendingX.get(), p.get()) >= 0 || BN_cmp(mPendingY.get(), p.get()) >= 0 || !pt
						|| EC_POINT_set_affine_coordinates(Group(), pt.get(), mPendingX.get(), mPendingY.get(), ctx.get()) != 1
						|| EC_POINT_is_on_curve(Group(), pt.get(), ctx.get()) != 1)
					{
						mPendingY.reset();
						throw InvalidKeyException("point not on curve", "IESKey::SetParam");
					}
					mPriv.reset();
					mPub = std::move(pt);
				}

				static BnPtr RandomScalar(const EC_GROUP *group, IRNGAlg *prng)
				{
					const BIGNUM *order = EC_GROUP_get0_order(group);
					BnPtr range(BN_dup(order));
					BnPtr d = NewBn();
					if (!range || !BN_sub_word(range.get(), 1))
						throw BadAllocException("BN failure", "IESKey::Generate");
					if (prng == 0)
					{
						if (BN_priv_rand_range(d.get(), range.get()) != 1)
							throw RuntimeException("random generation failed", "IESKey::Generate");
					}
					else
					{
						Blob bytes(static_cast<size_t>(BN_num_bytes(order)) + 8);
						RandomBytes(bytes.begin(), bytes.size(), prng);
						BnPtr r = BlobToBn(bytes);
						BnCtxPtr ctx = NewBnCtx();
						BN_mod(d.get(), r.get(), range.get(), ctx.get());
					}
					BN_add_word(d.get(), 1);
					return d;
				}

				PointPtr ParsePeerPublicKey(const Blob &data) const
				{
					BnCtxPtr ctx = NewBnCtx();
					size_t rlen = EncodedPointLen(Group(), data.begin(), data.size());
					if (rlen != 0 && rlen == data.size())
					{
						PointPtr pt = OctetsToPoint(Group(), data.begin(), data.size(), ctx.get());
						if (!pt)
							throw InvalidKeyException("peer public key not on curve", "IESKey::CreateAlgorithm");
						return pt;
					}
					IESKey peer;
					peer.Import(data);
					if (!peer.mPub)
						throw InvalidAlgorithmParameterException("no peer public key", "IESKey::CreateAlgorithm");
					if (EC_GROUP_cmp(Group(), peer.Group(), ctx.get()) != 0)
						throw InvalidAlgorithmParameterException("peer key uses a different curve", "IESKey::CreateAlgorithm");
					Blob oct = PointToOctets(peer.Group(), peer.mPub.get(), ctx.get());
					PointPtr pt = OctetsToPoint(Group(), oct.begin(), oct.size(), ctx.get());
					if (!pt)
						throw InvalidKeyException("peer public key not on curve", "IESKey::CreateAlgorithm");
					return pt;
				}

				IAlgorithm *CreateEncrypt(PointPtr recipient, bool newEphemeral) const
				{
					if (!recipient)
						throw InvalidKeyException("invalid recipient key", "IESKey::CreateAlgorithm");
					BnCtxPtr ctx = NewBnCtx();
					BnPtr d;
					PointPtr pub;
					if (newEphemeral)
					{
						d = RandomScalar(Group(), 0);
						pub.reset(EC_POINT_new(Group()));
						if (!pub || EC_POINT_mul(Group(), pub.get(), d.get(), 0, 0, ctx.get()) != 1)
							throw InvalidKeyException("ephemeral key generation failed", "IESKey::CreateAlgorithm");
					}
					else
					{
						d.reset(BN_dup(mPriv.get()));
						pub.reset(EC_POINT_dup(mPub.get(), Group()));
					}
					Blob r = PointToOctets(Group(), pub.get(), ctx.get());
					return new IESEncrypt(GroupPtr(EC_GROUP_dup(Group())), std::move(d), r, std::move(recipient),
						mShared1, mShared2);
				}

				IAlgorithm *CreateDecrypt() const
				{
					if (!mPriv)
						throw InvalidKeyException("no private key", "IESKey::CreateAlgorithm");
					return new IESDecrypt(GroupPtr(EC_GROUP_dup(Group())), BnPtr(BN_dup(mPriv.get())), mShared1, mShared2);
				}

				Blob Spki() const
				{
					if (!mPub)
						throw InvalidKeyException("no public key", "IESKey::Export");
					BnCtxPtr ctx = NewBnCtx();
					Blob alg(kEcPublicKeyOid, kEcPublicKeyOid + sizeof(kEcPublicKeyOid));
					alg.append(CurveOidTlv(FindCurve(mCurve)->nid));
					Blob bits(1, 0);
					bits.append(PointToOctets(Group(), mPub.get(), ctx.get()));
					Blob content;
					DerAppend(content, 0x30, alg);
					DerAppend(content, 0x03, bits);
					Blob out;
					DerAppend(out, 0x30, content);
					return out;
				}

				Blob EcPrivateKey() const
				{
					if (!mPriv)
						throw InvalidKeyException("no private key", "IESKey::Export");
					BnCtxPtr ctx = NewBnCtx();
					const BIGNUM *order = EC_GROUP_get0_order(Group());
					Blob content;
					const byte version[] = {0x01};
					DerAppend(content, 0x02, version, 1);
					DerAppend(content, 0x04, BnToPadded(mPriv.get(), static_cast<size_t>(BN_num_bytes(order))));
					DerAppend(content, 0xA0, CurveOidTlv(FindCurve(mCurve)->nid));
					Blob bits(1, 0);
					bits.append(PointToOctets(Group(), mPub.get(), ctx.get()));
					Blob bitString;
					DerAppend(bitString, 0x03, bits);
					DerAppend(content, 0xA1, bitString);
					Blob out;
					DerAppend(out, 0x30, content);
					return out;
				}

				void ImportSpki(const DerItem &top, const DerItem &alg, size_t pos)
				{
					size_t apos = 0;
					DerItem algOid, curveOid, bits;
					if (!DerRead(alg.data, alg.len, apos, algOid) || algOid.tag != 0x06
						|| algOid.len + 2 != sizeof(kEcPublicKeyOid)
						|| std::memcmp(algOid.data, kEcPublicKeyOid + 2, algOid.len) != 0)
						throw InvalidKeyException("not an EC public key", "IESKey::Import");
					size_t oidStart = apos;
					if (!DerRead(alg.data, alg.len, apos, curveOid) || curveOid.tag != 0x06)
						throw InvalidKeyException("only named curves are supported", "IESKey::Import");
					if (!DerRead(top.data, top.len, pos, bits) || bits.tag != 0x03 || bits.len < 2 || bits.data[0] != 0)
						throw InvalidKeyException("invalid public key encoding", "IESKey::Import");
					SetCurve(CurveFromOid(alg.data + oidStart, apos - oidStart));
					SetParam(PUBLICKEY, Blob(bits.data + 1, bits.data + bits.len));
				}

				void ImportEcPrivateKey(const DerItem &top, size_t pos)
				{
					DerItem key, item;
					if (!DerRead(top.data, top.len, pos, key) || key.tag != 0x04)
						throw InvalidKeyException("invalid private key encoding", "IESKey::Import");
					bool haveCurve = false;
					while (pos < top.len)
					{
						if (!DerRead(top.data, top.len, pos, item))
							throw InvalidKeyException("invalid private key encoding", "IESKey::Import");
						if (item.tag == 0xA0)
						{
							SetCurve(CurveFromOid(item.data, item.len));
							haveCurve = true;
						}
					}
					if (!haveCurve)
						throw InvalidKeyException("private key without curve", "IESKey::Import");
					SetPrivate(BlobToBn(Blob(key.data, key.data + key.len)));
				}

				int mCurve;
				mutable GroupPtr mGroup;
				BnPtr mPriv;
				PointPtr mPub;
				BnPtr mPendingX, mPendingY;
				Blob mShared1, mShared2;
				int mNewEphemeral;
			};

			IKey *NewIESKey() { return new IESKey(); }
		} // namespace

		IKey *CreateIESKey() { return NewIESKey(); }

		IKey *ImportECKeyBlob(const Blob &keyblob)
		{
			IESKey *key = new IESKey();
			try
			{
				key->Import(keyblob);
			}
			catch (...)
			{
				delete key;
				throw;
			}
			return key;
		}

		// Test hook: true if the explicit fallback parameters of a curve equal OpenSSL's named curve.
		bool CurveFallbackMatches(int id)
		{
			const CurveDef *def = FindCurve(id);
			if (def == 0 || def->p == 0)
				return false;
			GroupPtr named(EC_GROUP_new_by_curve_name(def->nid));
			if (!named)
				return true;
			GroupPtr expl = MakeGroup(*def, true);
			BnCtxPtr ctx = NewBnCtx();
			BnPtr v[2][7];
			const EC_GROUP *groups[2] = {named.get(), expl.get()};
			for (int g = 0; g < 2; ++g)
			{
				for (int i = 0; i < 7; ++i)
					v[g][i] = NewBn();
				if (EC_GROUP_get_curve(groups[g], v[g][0].get(), v[g][1].get(), v[g][2].get(), ctx.get()) != 1
					|| EC_POINT_get_affine_coordinates(groups[g], EC_GROUP_get0_generator(groups[g]),
						v[g][3].get(), v[g][4].get(), ctx.get()) != 1
					|| !BN_copy(v[g][5].get(), EC_GROUP_get0_order(groups[g]))
					|| !BN_copy(v[g][6].get(), EC_GROUP_get0_cofactor(groups[g])))
					return false;
			}
			for (int i = 0; i < 7; ++i)
				if (BN_cmp(v[0][i].get(), v[1][i].get()) != 0)
					return false;
			return true;
		}
	} // namespace detail
} // namespace act
