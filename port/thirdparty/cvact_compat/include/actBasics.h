// Source-compatible replacement for the cv act library (subset used by CrypTool).
#ifndef ACT_Basics_h
#define ACT_Basics_h

#include <cstddef>
#include <cstdint>

namespace act
{
	typedef unsigned long ulong;
	typedef unsigned int uint;
	typedef unsigned short ushort;
	typedef unsigned char uchar;
	typedef unsigned char byte;
	typedef unsigned short word;
	typedef unsigned long dword;

	using std::size_t;
	using std::ptrdiff_t;

	typedef int paramid_t;
	typedef int status_t;
	typedef int mode_t;
	typedef int export_t;

	typedef std::uint16_t uint16;
	typedef std::uint32_t uint32;
	typedef std::uint64_t uint64;
#ifndef U64
#define U64(x) x##ULL
#endif

	const export_t DEFAULT = 0;
} // namespace act

#endif // ACT_Basics_h
