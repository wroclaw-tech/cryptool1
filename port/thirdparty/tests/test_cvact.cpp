// Smoke test for cvact_compat: mirrors the call sequence of CrypTool/ECIESMain.cpp.
#include "actInit.h"
#include "actAlgorithm.h"
#include "actTools.h"
#include "actDate.h"
#include "actIRNGAlg.h"
#include "actException.h"
#include "actKey.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace act
{
	namespace detail
	{
		Blob X963Kdf(const char *digest, const Blob &secret, const Blob &sharedInfo, size_t len);
		bool CurveFallbackMatches(int id);
	}
}

static int g_failures = 0;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

static std::string Hex(const act::Blob &b)
{
	std::string s(b.size() * 2 + 1, '\0');
	act::blob2hex(b, &s[0]);
	s.resize(b.size() * 2);
	return s;
}

// Mimics the strings CrypTool obtains from libec: "0X<HEX>" with the X lowered to x.
static std::string CrypToolNumber(const act::Blob &twosComplement)
{
	std::string h = Hex(twosComplement);
	size_t nz = h.find_first_not_of('0');
	h = nz == std::string::npos ? "0" : h.substr(nz);
	for (size_t i = 0; i < h.size(); ++i)
		h[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(h[i])));
	return "0x" + h;
}

static int importKey(act::Key &key, int curve, const char *x, const char *y, const char *s)
{
	try
	{
		key.SetParam(act::CURVE, curve);
		if (std::string(s) != "")
			key.SetParam(act::PRIVATEKEY, s);
		else
		{
			key.SetParam(act::PUBLIC_X, x);
			key.SetParam(act::PUBLIC_Y, y);
		}
		return 0;
	}
	catch (act::InvalidKeyException &)
	{
		return 2;
	}
	catch (act::Exception &)
	{
		return 2;
	}
}

static bool encryptSessionKey(const act::Key &pubRecECIES, const act::Key &privSendECIES,
	const act::Blob &sessionKey, act::Blob &encryptedSessionKey)
{
	try
	{
		act::Blob publicBlob;
		pubRecECIES.Export(publicBlob, act::PUBLIC);
		act::Algorithm Encrypt(privSendECIES, act::ENCRYPT, publicBlob);
		Encrypt.Write(sessionKey);
		Encrypt.Finalize();
		Encrypt.Read(encryptedSessionKey);
		return true;
	}
	catch (act::Exception &e)
	{
		std::cerr << "Exception: " << typeid(e).name() << " what: " << e.what() << " where: " << e.where() << "\n";
		return false;
	}
}

static int decryptSessionKey(const act::Key &privRecECIES, const act::Blob &encryptedSessionKey, act::Blob &sessionKey)
{
	try
	{
		act::Algorithm Decrypt(privRecECIES, act::DECRYPT);
		Decrypt.Write(encryptedSessionKey);
		Decrypt.Finalize();
		Decrypt.Read(sessionKey);
		return 0;
	}
	catch (act::Exception &)
	{
		return 42;
	}
}

static bool encryptData(const act::Blob &plaintext, act::Blob &sessionKey, act::Blob &ciphertext)
{
	try
	{
		act::Key blockCipher("BlockCipher");
		blockCipher.SetParam(act::CIPHER, "AES");
		blockCipher.SetParam(act::BCMODE, "CBC");
		blockCipher.SetParam(act::KEYSIZE, 32);
		act::Blob salt;
		act::Algorithm saltGen(act::CreateBBS());
		saltGen.Write(act::Date());
		saltGen.Read(salt, 256);

		blockCipher.Derive(plaintext, salt);
		salt.clear();
		blockCipher.GetParam(act::RAWKEY, sessionKey);

		act::Algorithm encrypt(blockCipher, act::ENCRYPT);
		encrypt.Write(plaintext);
		encrypt.Finalize();
		encrypt.Read(ciphertext);
		return true;
	}
	catch (act::Exception &e)
	{
		std::cerr << "Exception: " << typeid(e).name() << " what: " << e.what() << " where: " << e.where() << "\n";
		return false;
	}
}

static int decryptData(const act::Blob &ciphertext, const act::Blob &sessionKey, act::Blob &plaintext)
{
	try
	{
		act::Key blockCipher("BlockCipher");
		blockCipher.SetParam(act::CIPHER, "AES");
		blockCipher.SetParam(act::BCMODE, "CBC");
		blockCipher.SetParam(act::RAWKEY, sessionKey);

		act::Algorithm decrypt(blockCipher, act::DECRYPT);
		decrypt.Write(ciphertext);
		decrypt.Finalize();
		decrypt.Read(plaintext);
		return 0;
	}
	catch (act::Exception &)
	{
		return 5;
	}
}

// Same Blob manipulations as writeEncFile()/readEncFile() in ECIESMain.cpp.
static act::Blob writeEncFile(const act::Blob &encryptedSessionKey, const act::Blob &ciphertext, size_t &keyStart)
{
	std::ostringstream header;
	header << "Receiver:Bob, Test|Curve ID:EC-prime256v1|Length of the ECIES encrypted AES session key:"
	       << encryptedSessionKey.size() << "Byte|Ciphertext length:" << ciphertext.size()
	       << "Byte|ECIES encrypted AES session key:";
	std::string message = header.str();
	act::Blob output(message.c_str());
	keyStart = output.size();
	output.insert(output.end(), encryptedSessionKey.begin(), encryptedSessionKey.end());
	act::Blob header07("|AES ciphertext:");
	output.insert(output.end(), header07.begin(), header07.end());
	header07.clear();
	output.insert(output.end(), ciphertext.begin(), ciphertext.end());
	return output;
}

static bool readEncFile(const act::Blob &input, size_t keyStart, size_t keyLength, size_t ctStart,
	act::Blob &encryptedSessionKey, act::Blob &ciphertext)
{
	act::Blob in(input);
	std::string message = reinterpret_cast<char *>(&in[0]);
	act::Blob key;
	act::Blob ctext;
	key.insert(key.begin(), &in[keyStart], &in[keyStart + keyLength]);
	ctext.insert(ctext.begin(), &in[ctStart], &in[in.size()]);
	encryptedSessionKey = key;
	ciphertext = ctext;
	return !message.empty();
}

static void TestBlobAndTools()
{
	act::Blob a("hello");
	CHECK(a.size() == 5);
	CHECK(a[a.size()] == 0);
	CHECK(std::strcmp(reinterpret_cast<const char *>(&a[0]), "hello") == 0);
	act::Blob empty;
	CHECK(empty.empty() && reinterpret_cast<const char *>(&empty[0])[0] == '\0');
	std::string s("world");
	act::Blob w(s);
	a.insert(a.end(), w.begin(), w.end());
	CHECK(a == act::Blob("helloworld"));
	a.insert(a.begin() + 5, a.begin(), a.begin() + 5);
	CHECK(a == act::Blob("hellohelloworld"));
	a.erase(a.begin(), a.begin() + 5);
	CHECK(a == act::Blob("helloworld"));
	a.resize(3);
	CHECK(a == act::Blob("hel") && a[3] == 0);
	CHECK(act::Blob("abc") < act::Blob("abd") && act::Blob("ab") < act::Blob("abc"));
	act::Blob filled(4, 0xab);
	CHECK(filled.size() == 4 && filled[3] == 0xab);
	bool threw = false;
	try { filled.at(4); } catch (act::OutOfRangeException &) { threw = true; }
	CHECK(threw);
	std::ostringstream os;
	os << act::hex2blob("0x00ff10");
	CHECK(os.str() == "00ff10");

	act::Blob h = act::hex2blob("0xDEADbeef01");
	CHECK(h.size() == 5 && h[0] == 0xde && h[4] == 0x01);
	CHECK(Hex(h) == "deadbeef01");
	CHECK(Hex(act::hex2blob("abc")) == "0abc");

	CHECK(Hex(act::EncodeNumber("128")) == "0080");
	CHECK(Hex(act::EncodeNumber("-128")) == "80");
	CHECK(Hex(act::EncodeNumber("-129")) == "ff7f");
	CHECK(Hex(act::EncodeNumber("0x7f")) == "7f");
	CHECK(Hex(act::EncodeNumber(0)) == "00");
	CHECK(Hex(act::EncodeOID("0.2.262.1.10.1.3.8")) == "028206010a010308");
	CHECK(Hex(act::EncodeOID("1.2.840.10045.2.1")) == "2a8648ce3d0201");

	act::Blob check("123456789");
	CHECK(act::CalculateCRC32(check) == 0xCBF43926UL);
	CHECK(act::CalculateCRC16(check) == 0xBB3DUL);
	CHECK(act::CalculateCRC16CCITT(check) == 0x29B1UL);

	act::Date d(31, 1, 2024, 23, 59, 30);
	CHECK(Hex(d.Encode()) == Hex(act::Blob("\x17\x0d" "240131235930Z")));
	act::Date d2(d.Encode());
	CHECK(d2 == d);
	d2.AddMonths(1);
	CHECK(d2.GetMonth() == 2 && d2.GetDay() == 29);
	d2.AddSecond(31);
	CHECK(d2.GetDay() == 1 && d2.GetMonth() == 3 && d2.GetHour() == 0 && d2.GetMinute() == 0 && d2.GetSecond() == 1);
	CHECK(act::Date(1, 1, 2000).DayOfWeek() == 6);
	act::Blob now = act::Date();
	CHECK(now.size() == 15 && now[0] == 0x17);
}

static void TestKdfVectors()
{
	// NIST CAVP SP 800-135 ANSI X9.63 KDF vectors.
	CHECK(Hex(act::detail::X963Kdf("SHA1", act::hex2blob("1c7d7b5f0597b03d06a018466ed1a93e30ed4b04dc64ccdd"),
		act::Blob(), 16)) == "bf71dffd8f4d99223936beb46fee8ccc");
	CHECK(Hex(act::detail::X963Kdf("SHA256", act::hex2blob("96c05619d56c328ab95fe84b18264b08725b85e33fd34f08"),
		act::Blob(), 16)) == "443024c3dae66b95e6f5670601558f71");
}

static void TestKnownAnswer()
{
	// NIST CAVP ECC CDH P-256 vector #0 used as sender (ephemeral) key and recipient key;
	// the expected ciphertext was computed independently (SHA-1 X9.63 KDF over R || Z, HMAC-SHA1).
	act::Key sender("IES");
	CHECK(importKey(sender, act::ANSIp256r1, "", "", "0x7d7dc5f71eb29ddaf80d6214632eeae03d9058af1fb6d22ed80badb62bc1a534") == 0);
	act::Blob x, y;
	sender.GetParam(act::PUBLIC_X, x);
	sender.GetParam(act::PUBLIC_Y, y);
	CHECK(Hex(x) == "00ead218590119e8876b29146ff89ca61770c4edbbf97d38ce385ed281d8a6b230");
	CHECK(Hex(y) == "28af61281fd35e2fa7002523acc85a429cb06ee6648325389f59edfce1405141");

	act::Key recipient("IES");
	CHECK(importKey(recipient, act::ANSIp256r1,
		"0x700c48f77f56584c5cc632ca65640db91b6bacce3a4df6b42ce7cc838833d287",
		"0xdb71e509e3fd9b060ddb20ba5c51dcc5948d46fbf640dfe0441782cab85fa4ac", "") == 0);

	act::Blob message;
	for (int i = 0; i < 32; ++i)
		message.push_back(static_cast<act::byte>(i));
	act::Blob encrypted;
	CHECK(encryptSessionKey(recipient, sender, message, encrypted));
	CHECK(Hex(encrypted) ==
		"04ead218590119e8876b29146ff89ca61770c4edbbf97d38ce385ed281d8a6b230"
		"28af61281fd35e2fa7002523acc85a429cb06ee6648325389f59edfce1405141"
		"29603457fcb66a714e976d89526304033bf6cb06d4641f63244b7d3a122ae698"
		"59bc42f5ec7d495c7047f63bb3df2de71bf77d07");
}

struct CurveCase
{
	int id;
	const char *name;
	size_t fieldLen;
};

static void TestCurve(const CurveCase &c)
{
	CHECK(act::detail::CurveFallbackMatches(c.id));

	// Receiver key pair, as created with CrypTool's key generation and read back from the PSE.
	act::Key receiver("IES");
	receiver.SetParam(act::CURVE, c.id);
	receiver.Generate();
	act::Blob rx, ry, rs;
	receiver.GetParam(act::PUBLIC_X, rx);
	receiver.GetParam(act::PUBLIC_Y, ry);
	receiver.GetParam(act::PRIVATEKEY, rs);
	std::string xR = CrypToolNumber(rx), yR = CrypToolNumber(ry), sR = CrypToolNumber(rs);

	// encrypt()
	act::Key privSendECIES("IES");
	privSendECIES.SetParam(act::CURVE, c.id);
	privSendECIES.Generate();
	act::Key pubRecECIES("IES");
	CHECK(importKey(pubRecECIES, c.id, xR.c_str(), yR.c_str(), "") == 0);

	act::Blob plaintext("The quick brown fox jumps over the lazy dog. ECIES/AES hybrid test for ");
	plaintext.append(act::Blob(c.name));
	act::Blob sessionKey, ciphertext, encryptedSessionKey;
	CHECK(encryptData(plaintext, sessionKey, ciphertext));
	CHECK(sessionKey.size() == 32);
	CHECK(ciphertext.size() == 16 + (plaintext.size() / 16 + 1) * 16);
	CHECK(encryptSessionKey(pubRecECIES, privSendECIES, sessionKey, encryptedSessionKey));
	CHECK(encryptedSessionKey.size() == 1 + 2 * c.fieldLen + sessionKey.size() + 20);

	size_t keyStart = 0;
	act::Blob file = writeEncFile(encryptedSessionKey, ciphertext, keyStart);
	size_t ctStart = keyStart + encryptedSessionKey.size() + std::strlen("|AES ciphertext:");

	// decrypt()
	act::Key privRecECIES("IES");
	CHECK(importKey(privRecECIES, c.id, "", "", sR.c_str()) == 0);
	act::Blob readKey, readCipher, decryptedSessionKey, decrypted;
	CHECK(readEncFile(file, keyStart, encryptedSessionKey.size(), ctStart, readKey, readCipher));
	CHECK(readKey == encryptedSessionKey && readCipher == ciphertext);
	CHECK(decryptSessionKey(privRecECIES, readKey, decryptedSessionKey) == 0);
	CHECK(decryptedSessionKey == sessionKey);
	CHECK(decryptData(readCipher, decryptedSessionKey, decrypted) == 0);
	CHECK(decrypted == plaintext);

	// Tampering and wrong keys are reported as in ECIESMain.cpp.
	for (size_t pos : {size_t(1 + 2 * c.fieldLen), encryptedSessionKey.size() - 1})
	{
		act::Blob tampered(encryptedSessionKey);
		tampered[pos] ^= 0x01;
		act::Blob out;
		CHECK(decryptSessionKey(privRecECIES, tampered, out) == 42);
	}
	act::Blob badPoint(encryptedSessionKey);
	badPoint[2] ^= 0x01;
	act::Blob out;
	CHECK(decryptSessionKey(privRecECIES, badPoint, out) == 42);
	CHECK(decryptSessionKey(privSendECIES, encryptedSessionKey, out) == 42);
	CHECK(decryptSessionKey(privRecECIES, act::Blob(5, 4), out) == 42);
	act::Blob truncated(readCipher.begin(), readCipher.end() - 1);
	CHECK(decryptData(truncated, decryptedSessionKey, out) == 5);

	act::Key invalid("IES");
	CHECK(importKey(invalid, c.id, xR.c_str(), "0x1234", "") == 2);
	act::Key infinity("IES");
	CHECK(importKey(infinity, c.id, "inf", "inf", "") == 2);

	// Key blob export/import round trip.
	act::Blob pub, priv;
	receiver.Export(pub, act::PUBLIC);
	receiver.Export(priv, act::PRIVATE);
	act::Key fromPub(pub), fromPriv(priv);
	CHECK(fromPub.GetParam(act::CURVE) == c.id && fromPriv.GetParam(act::CURVE) == c.id);
	act::Blob px;
	fromPub.GetParam(act::PUBLIC_X, px);
	CHECK(px == rx);
	act::Blob roundtrip;
	{
		act::Algorithm enc(fromPub, act::ENCRYPT);
		enc << plaintext << act::final;
		act::Blob ct;
		enc >> ct;
		act::Algorithm dec(fromPriv, act::DECRYPT);
		dec << ct << act::final;
		dec >> roundtrip;
	}
	CHECK(roundtrip == plaintext);
}

int main()
{
	act::Init();
	act::Init();

	try
	{
		TestBlobAndTools();
		TestKdfVectors();
		TestKnownAnswer();

		const CurveCase curves[] = {
			{act::ANSIp192r1, "EC-prime192v1", 24}, {act::ANSIp192r2, "EC-prime192v2", 24},
			{act::ANSIp192r3, "EC-prime192v3", 24}, {act::ANSIp239r1, "EC-prime239v1", 30},
			{act::ANSIp239r2, "EC-prime239v2", 30}, {act::ANSIp239r3, "EC-prime239v3", 30},
			{act::ANSIp256r1, "EC-prime256v1", 32}, {act::SECGp160r1, "secp160r1", 20},
		};
		for (const CurveCase &c : curves)
			TestCurve(c);

		act::Algorithm sha1("SHA1");
		sha1 << act::Blob("abc") << act::final;
		act::Blob md;
		sha1 >> md;
		CHECK(Hex(md) == "a9993e364706816aba3e25717850c26c9cd0d89d");

		bool threw = false;
		try { act::Key unknown("RSA"); } catch (act::NoSuchAlgorithmException &) { threw = true; }
		CHECK(threw);
	}
	catch (act::Exception &e)
	{
		std::fprintf(stderr, "unexpected act::Exception %s: %s (%s)\n", typeid(e).name(), e.what(), e.where());
		return 1;
	}

	if (g_failures != 0)
	{
		std::fprintf(stderr, "%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("cvact_compat: all checks passed\n");
	return 0;
}
