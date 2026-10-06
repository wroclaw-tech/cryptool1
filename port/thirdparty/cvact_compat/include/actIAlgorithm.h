// Source-compatible replacement for the cv act library (subset used by CrypTool).
#ifndef ACT_IALGORITHM_H
#define ACT_IALGORITHM_H

#include "actBasics.h"

namespace act
{
	class Blob;

	class IAlgorithm
	{
	public:
		virtual void Write(const Blob &input) = 0;
		virtual void Write(const byte *input, size_t insize) = 0;
		virtual void Finalize() = 0;
		virtual size_t GetAvailableSize() const = 0;
		// Moves up to max (0: all) available output bytes into output, replacing its contents.
		virtual size_t Read(Blob &output, size_t max = 0) = 0;
		virtual size_t Read(byte *outbuffer, size_t buffersize) = 0;
		virtual status_t GetStatus() const = 0;
		virtual ~IAlgorithm() {}
	};
} // namespace act

#endif // ACT_IALGORITHM_H
