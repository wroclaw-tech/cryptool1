// Source-compatible replacement for the cv act library (subset used by CrypTool).
#ifndef ACT_TOOLS_H
#define ACT_TOOLS_H

#include "actMode.h"
#include "actBlob.h"
#include "actIKey.h"

namespace act
{
	// Encodes a binary object identifier, e.g. EncodeOID("1.2.840.10045.2.1").
	Blob EncodeOID(const char *oid);

	// Signed decimal or hexadecimal ("0x" / "-0x") number to big-endian two's complement.
	Blob EncodeNumber(const char *number);
	Blob EncodeNumber(int number);

	// Unsigned hexadecimal number (optional "0x" prefix) to Blob and back.
	// blob2hex writes 2 * b.size() lowercase digits plus a terminating NUL.
	Blob hex2blob(const char *hexnumber);
	void blob2hex(const Blob &b, char *hexnumber);

	bool blob2file(const char *filename, const act::Blob &blob);
	bool file2blob(const char *filename, Blob &blob);

	unsigned long CalculateCRC16(const act::byte *message, size_t message_len,
		unsigned long crc_init_value = 0x0000);
	unsigned long CalculateCRC16(const act::Blob &message,
		unsigned long crc_init_value = 0x0000);
	unsigned long CalculateCRC16CCITT(const act::byte *message, size_t message_len,
		unsigned long crc_init_value = 0xFFFF);
	unsigned long CalculateCRC16CCITT(const act::Blob &message,
		unsigned long crc_init_value = 0xFFFF);
	unsigned long CalculateCRC32(const act::byte *message, size_t message_len,
		unsigned long crc_init_value = 0xFFFFFFFF);
	unsigned long CalculateCRC32(const act::Blob &message,
		unsigned long crc_init_value = 0xFFFFFFFF);
} // namespace act

#endif // ACT_TOOLS_H
