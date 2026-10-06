#include "actInternal.h"
#include "actTools.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/rand.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace act
{
	namespace detail
	{
		BnPtr NewBn()
		{
			BnPtr bn(BN_new());
			if (!bn)
				throw BadAllocException("BN_new failed", "detail::NewBn");
			return bn;
		}

		BnCtxPtr NewBnCtx()
		{
			BnCtxPtr ctx(BN_CTX_new());
			if (!ctx)
				throw BadAllocException("BN_CTX_new failed", "detail::NewBnCtx");
			return ctx;
		}

		BnPtr ParseNumber(const char *text)
		{
			if (text == 0)
				return BnPtr();
			std::string s;
			for (const char *p = text; *p; ++p)
				if (!std::isspace(static_cast<unsigned char>(*p)))
					s += *p;
			bool negative = false;
			size_t pos = 0;
			if (pos < s.size() && (s[pos] == '-' || s[pos] == '+'))
			{
				negative = s[pos] == '-';
				++pos;
			}
			bool hex = s.size() >= pos + 2 && s[pos] == '0' && (s[pos + 1] == 'x' || s[pos + 1] == 'X');
			if (hex)
				pos += 2;
			std::string digits = s.substr(pos);
			if (digits.empty())
				return BnPtr();
			for (size_t i = 0; i < digits.size(); ++i)
			{
				unsigned char c = static_cast<unsigned char>(digits[i]);
				if (hex ? !std::isxdigit(c) : !std::isdigit(c))
					return BnPtr();
			}
			BIGNUM *raw = 0;
			int n = hex ? BN_hex2bn(&raw, digits.c_str()) : BN_dec2bn(&raw, digits.c_str());
			BnPtr bn(raw);
			if (!bn || n != static_cast<int>(digits.size()))
				return BnPtr();
			BN_set_negative(bn.get(), negative ? 1 : 0);
			return bn;
		}

		Blob BnToTwosComplement(const BIGNUM *bn)
		{
			if (BN_is_zero(bn))
				return Blob(1, 0);
			BnPtr mag(BN_dup(bn));
			if (!mag)
				throw BadAllocException("BN_dup failed", "detail::BnToTwosComplement");
			BN_set_negative(mag.get(), 0);
			int len = BN_num_bytes(mag.get());
			Blob out;
			if (!BN_is_negative(bn))
			{
				out.resize(static_cast<size_t>(len));
				BN_bn2bin(mag.get(), &out[0]);
				if (out[0] & 0x80)
					out.insert(out.begin(), byte(0));
				return out;
			}
			BnPtr limit = NewBn();
			BN_set_bit(limit.get(), 8 * len - 1);
			if (BN_cmp(mag.get(), limit.get()) > 0)
				++len;
			BnPtr modulus = NewBn();
			BN_set_bit(modulus.get(), 8 * len);
			BnPtr t = NewBn();
			BN_sub(t.get(), modulus.get(), mag.get());
			out.resize(static_cast<size_t>(len));
			BN_bn2binpad(t.get(), &out[0], len);
			return out;
		}

		BnPtr BlobToBn(const Blob &b)
		{
			BnPtr bn(BN_bin2bn(b.empty() ? 0 : &b[0], static_cast<int>(b.size()), 0));
			if (!bn)
				throw BadAllocException("BN_bin2bn failed", "detail::BlobToBn");
			return bn;
		}

		Blob BnToPadded(const BIGNUM *bn, size_t len)
		{
			Blob out(len);
			if (len != 0 && BN_bn2binpad(bn, &out[0], static_cast<int>(len)) < 0)
				throw ArithmeticException("number too large", "detail::BnToPadded");
			return out;
		}

		void RandomBytes(byte *out, size_t n, IRNGAlg *prng)
		{
			if (n == 0)
				return;
			if (prng != 0)
			{
				if (prng->Read(out, n) != n)
					throw RuntimeException("random generator delivered too few bytes", "detail::RandomBytes");
				return;
			}
			if (RAND_bytes(out, static_cast<int>(n)) != 1)
				throw RuntimeException("RAND_bytes failed", "detail::RandomBytes");
		}

		void DerAppend(Blob &out, byte tag, const byte *content, size_t len)
		{
			out.push_back(tag);
			if (len < 0x80)
				out.push_back(static_cast<byte>(len));
			else
			{
				byte tmp[sizeof(size_t)];
				int n = 0;
				for (size_t v = len; v != 0; v >>= 8)
					tmp[n++] = static_cast<byte>(v & 0xff);
				out.push_back(static_cast<byte>(0x80 | n));
				while (n > 0)
					out.push_back(tmp[--n]);
			}
			out.insert(out.end(), content, content + len);
		}

		void DerAppend(Blob &out, byte tag, const Blob &content)
		{
			DerAppend(out, tag, content.begin(), content.size());
		}

		bool DerRead(const byte *buf, size_t size, size_t &pos, DerItem &item)
		{
			if (pos + 2 > size)
				return false;
			item.tag = buf[pos++];
			size_t len = buf[pos++];
			if (len & 0x80)
			{
				size_t n = len & 0x7f;
				if (n == 0 || n > 4 || pos + n > size)
					return false;
				len = 0;
				for (size_t i = 0; i < n; ++i)
					len = (len << 8) | buf[pos++];
			}
			if (len > size - pos)
				return false;
			item.data = buf + pos;
			item.len = len;
			pos += len;
			return true;
		}

		Blob Digest(const char *name, const Blob &data)
		{
			unsigned char md[EVP_MAX_MD_SIZE];
			size_t mdlen = 0;
			if (!EVP_Q_digest(0, name, 0, data.empty() ? 0 : data.begin(), data.size(), md, &mdlen))
				throw NoSuchAlgorithmException("hash algorithm not available", "detail::Digest");
			Blob out(md, md + mdlen);
			SecureZero(md, sizeof(md));
			return out;
		}

		Blob X963Kdf(const char *digest, const Blob &secret, const Blob &sharedInfo, size_t len)
		{
			Blob out(len);
			if (len == 0)
				return out;
			EVP_KDF *kdf = EVP_KDF_fetch(0, "X963KDF", 0);
			if (kdf == 0)
				throw NoSuchAlgorithmException("X963KDF not available", "detail::X963Kdf");
			EVP_KDF_CTX *ctx = EVP_KDF_CTX_new(kdf);
			EVP_KDF_free(kdf);
			if (ctx == 0)
				throw BadAllocException("EVP_KDF_CTX_new failed", "detail::X963Kdf");
			OSSL_PARAM params[4];
			int n = 0;
			params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, const_cast<char *>(digest), 0);
			params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
				const_cast<byte *>(secret.begin()), secret.size());
			if (!sharedInfo.empty())
				params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
					const_cast<byte *>(sharedInfo.begin()), sharedInfo.size());
			params[n] = OSSL_PARAM_construct_end();
			int ok = EVP_KDF_derive(ctx, &out[0], len, params);
			EVP_KDF_CTX_free(ctx);
			if (ok != 1)
				throw AlgorithmException("X9.63 key derivation failed", "detail::X963Kdf");
			return out;
		}

		Blob Hmac(const char *digest, const Blob &key, const Blob &data)
		{
			unsigned char mac[EVP_MAX_MD_SIZE];
			size_t maclen = 0;
			static const unsigned char dummy = 0;
			if (!EVP_Q_mac(0, "HMAC", 0, digest, 0, key.empty() ? &dummy : key.begin(), key.size(),
					data.empty() ? &dummy : data.begin(), data.size(), mac, sizeof(mac), &maclen))
				throw AlgorithmException("HMAC computation failed", "detail::Hmac");
			return Blob(mac, mac + maclen);
		}
	} // namespace detail

	Blob EncodeOID(const char *oid)
	{
		if (oid == 0)
			throw NullPointerException("null OID", "EncodeOID");
		std::vector<unsigned long long> arcs;
		const char *p = oid;
		while (*p)
		{
			if (!std::isdigit(static_cast<unsigned char>(*p)))
				throw BadException("invalid OID", "EncodeOID");
			unsigned long long v = 0;
			while (std::isdigit(static_cast<unsigned char>(*p)))
				v = v * 10 + static_cast<unsigned long long>(*p++ - '0');
			arcs.push_back(v);
			if (*p == '.')
			{
				++p;
				if (*p == 0)
					throw BadException("invalid OID", "EncodeOID");
			}
			else if (*p != 0)
				throw BadException("invalid OID", "EncodeOID");
		}
		if (arcs.size() < 2 || arcs[0] > 2 || (arcs[0] < 2 && arcs[1] > 39))
			throw BadException("invalid OID", "EncodeOID");
		std::vector<unsigned long long> values;
		values.push_back(arcs[0] * 40 + arcs[1]);
		values.insert(values.end(), arcs.begin() + 2, arcs.end());
		Blob out;
		for (size_t i = 0; i < values.size(); ++i)
		{
			byte tmp[10];
			int n = 0;
			unsigned long long v = values[i];
			do
			{
				tmp[n++] = static_cast<byte>(v & 0x7f);
				v >>= 7;
			} while (v != 0);
			while (n > 0)
			{
				--n;
				out.push_back(static_cast<byte>(tmp[n] | (n != 0 ? 0x80 : 0)));
			}
		}
		return out;
	}

	Blob EncodeNumber(const char *number)
	{
		detail::BnPtr bn = detail::ParseNumber(number);
		if (!bn)
			throw BadException("invalid number", "EncodeNumber");
		return detail::BnToTwosComplement(bn.get());
	}

	Blob EncodeNumber(int number)
	{
		return EncodeNumber(std::to_string(number).c_str());
	}

	Blob hex2blob(const char *hexnumber)
	{
		if (hexnumber == 0)
			throw NullPointerException("null string", "hex2blob");
		std::string digits;
		const char *p = hexnumber;
		while (std::isspace(static_cast<unsigned char>(*p)))
			++p;
		if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
			p += 2;
		for (; *p; ++p)
		{
			unsigned char c = static_cast<unsigned char>(*p);
			if (std::isspace(c))
				continue;
			if (!std::isxdigit(c))
				throw BadException("invalid hexadecimal digit", "hex2blob");
			digits += static_cast<char>(c);
		}
		if (digits.size() % 2 != 0)
			digits.insert(digits.begin(), '0');
		Blob out(digits.size() / 2);
		for (size_t i = 0; i < out.size(); ++i)
		{
			char pair[3] = {digits[2 * i], digits[2 * i + 1], 0};
			out[i] = static_cast<byte>(std::strtoul(pair, 0, 16));
		}
		return out;
	}

	void blob2hex(const Blob &b, char *hexnumber)
	{
		if (hexnumber == 0)
			throw NullPointerException("null buffer", "blob2hex");
		static const char digits[] = "0123456789abcdef";
		size_t j = 0;
		for (size_t i = 0; i < b.size(); ++i)
		{
			hexnumber[j++] = digits[b[i] >> 4];
			hexnumber[j++] = digits[b[i] & 0x0f];
		}
		hexnumber[j] = 0;
	}

	bool blob2file(const char *filename, const act::Blob &blob)
	{
		if (filename == 0)
			return false;
		std::FILE *f = std::fopen(filename, "wb");
		if (f == 0)
			return false;
		bool ok = blob.empty() || std::fwrite(blob.begin(), 1, blob.size(), f) == blob.size();
		ok = std::fclose(f) == 0 && ok;
		return ok;
	}

	bool file2blob(const char *filename, Blob &blob)
	{
		blob.clear();
		if (filename == 0)
			return false;
		std::FILE *f = std::fopen(filename, "rb");
		if (f == 0)
			return false;
		byte buf[8192];
		size_t n;
		while ((n = std::fread(buf, 1, sizeof(buf), f)) != 0)
			blob.insert(blob.end(), buf, buf + n);
		bool ok = !std::ferror(f);
		std::fclose(f);
		SecureZero(buf, sizeof(buf));
		return ok;
	}

	unsigned long CalculateCRC16(const act::byte *message, size_t message_len, unsigned long crc_init_value)
	{
		unsigned long crc = crc_init_value & 0xffff;
		for (size_t i = 0; i < message_len; ++i)
		{
			crc ^= message[i];
			for (int k = 0; k < 8; ++k)
				crc = (crc & 1) ? (crc >> 1) ^ 0xa001 : crc >> 1;
		}
		return crc;
	}

	unsigned long CalculateCRC16(const act::Blob &message, unsigned long crc_init_value)
	{
		return CalculateCRC16(message.begin(), message.size(), crc_init_value);
	}

	unsigned long CalculateCRC16CCITT(const act::byte *message, size_t message_len, unsigned long crc_init_value)
	{
		unsigned long crc = crc_init_value & 0xffff;
		for (size_t i = 0; i < message_len; ++i)
		{
			crc ^= static_cast<unsigned long>(message[i]) << 8;
			for (int k = 0; k < 8; ++k)
				crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
		}
		return crc;
	}

	unsigned long CalculateCRC16CCITT(const act::Blob &message, unsigned long crc_init_value)
	{
		return CalculateCRC16CCITT(message.begin(), message.size(), crc_init_value);
	}

	unsigned long CalculateCRC32(const act::byte *message, size_t message_len, unsigned long crc_init_value)
	{
		unsigned long crc = crc_init_value & 0xffffffffUL;
		for (size_t i = 0; i < message_len; ++i)
		{
			crc ^= message[i];
			for (int k = 0; k < 8; ++k)
				crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320UL : crc >> 1;
		}
		return crc ^ 0xffffffffUL;
	}

	unsigned long CalculateCRC32(const act::Blob &message, unsigned long crc_init_value)
	{
		return CalculateCRC32(message.begin(), message.size(), crc_init_value);
	}
} // namespace act
