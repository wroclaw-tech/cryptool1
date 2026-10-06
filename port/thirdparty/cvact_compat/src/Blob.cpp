#include "actBlob.h"

#include <openssl/crypto.h>

#include <cstdlib>
#include <cstring>
#include <new>
#include <ostream>

namespace act
{
	namespace
	{
		// Shared terminator for blobs without own storage; never written to by Blob itself.
		byte g_empty[1] = {0};
	}

	void SecureZero(void *p, size_t n) noexcept
	{
		if (p != 0 && n != 0)
			OPENSSL_cleanse(p, n);
	}

	Blob::Blob() noexcept : mFirst(g_empty), mSize(0), mCap(0) {}

	Blob::Blob(const char *str) : Blob()
	{
		if (str != 0)
			insert_raw(end(), reinterpret_cast<const byte *>(str), std::strlen(str));
	}

	Blob::Blob(std::string &str) : Blob()
	{
		insert_raw(end(), reinterpret_cast<const byte *>(str.data()), str.size());
	}

	Blob::Blob(size_type n, byte v) : Blob()
	{
		insert(end(), n, v);
	}

	Blob::Blob(const Blob &x) : Blob()
	{
		insert_raw(end(), x.mFirst, x.mSize);
	}

	Blob::Blob(Blob &&x) noexcept : Blob()
	{
		swap(x);
	}

	Blob::~Blob()
	{
		release();
	}

	Blob &Blob::operator=(const Blob &x)
	{
		if (this != &x)
		{
			if (x.mSize <= mCap)
			{
				if (x.mSize != 0)
					std::memmove(mFirst, x.mFirst, x.mSize);
				if (mSize > x.mSize)
					SecureZero(mFirst + x.mSize, mSize - x.mSize);
				mSize = x.mSize;
				if (mCap != 0)
					mFirst[mSize] = 0;
			}
			else
			{
				Blob tmp(x);
				swap(tmp);
			}
		}
		return *this;
	}

	Blob &Blob::operator=(Blob &&x) noexcept
	{
		if (this != &x)
		{
			release();
			swap(x);
		}
		return *this;
	}

	void Blob::assign(size_type n, byte x)
	{
		clear();
		insert(end(), n, x);
	}

	void Blob::release() noexcept
	{
		if (mCap != 0)
		{
			SecureZero(mFirst, mCap + 1);
			std::free(mFirst);
		}
		mFirst = g_empty;
		mSize = 0;
		mCap = 0;
	}

	void Blob::reallocate(size_type cap)
	{
		byte *p = static_cast<byte *>(std::malloc(cap + 1));
		if (p == 0)
			throw BadAllocException("out of memory", "Blob::reallocate");
		if (mSize != 0)
			std::memcpy(p, mFirst, mSize);
		std::memset(p + mSize, 0, cap + 1 - mSize);
		size_type size = mSize;
		release();
		mFirst = p;
		mSize = size;
		mCap = cap;
	}

	void Blob::reserve(size_type n)
	{
		if (n > mCap)
			reallocate(n);
	}

	Blob::iterator Blob::make_gap(iterator p, size_type m)
	{
		size_type off = static_cast<size_type>(p - mFirst);
		if (off > mSize)
			outofrange();
		if (m == 0)
			return mFirst + off;
		if (mSize + m > mCap)
		{
			size_type cap = mCap * 2;
			if (cap < mSize + m)
				cap = mSize + m;
			if (cap < 16)
				cap = 16;
			reallocate(cap);
		}
		std::memmove(mFirst + off + m, mFirst + off, mSize - off);
		mSize += m;
		mFirst[mSize] = 0;
		return mFirst + off;
	}

	Blob::iterator Blob::insert_raw(iterator p, const byte *src, size_type m)
	{
		iterator q = make_gap(p, m);
		if (m != 0)
			std::memcpy(q, src, m);
		return q;
	}

	Blob::iterator Blob::insert(iterator p, byte x)
	{
		return insert_raw(p, &x, 1);
	}

	void Blob::insert(iterator p, size_type m, byte x)
	{
		iterator q = make_gap(p, m);
		if (m != 0)
			std::memset(q, x, m);
	}

	void Blob::resize(size_type n, byte x)
	{
		if (n > mSize)
			insert(end(), n - mSize, x);
		else if (n < mSize)
			erase(begin() + n, end());
	}

	Blob::const_reference Blob::at(size_type p) const
	{
		if (p >= mSize)
			outofrange();
		return mFirst[p];
	}

	Blob::reference Blob::at(size_type p)
	{
		if (p >= mSize)
			outofrange();
		return mFirst[p];
	}

	void Blob::push_back(byte x)
	{
		insert_raw(end(), &x, 1);
	}

	void Blob::pop_back()
	{
		if (mSize != 0)
			erase(end() - 1);
	}

	void Blob::append(const Blob &x)
	{
		if (&x == this)
		{
			Blob tmp(x);
			insert_raw(end(), tmp.mFirst, tmp.mSize);
		}
		else
			insert_raw(end(), x.mFirst, x.mSize);
	}

	void Blob::append(size_type m, byte x)
	{
		insert(end(), m, x);
	}

	Blob::iterator Blob::erase(iterator p)
	{
		return erase(p, p + 1);
	}

	Blob::iterator Blob::erase(iterator f, iterator l)
	{
		size_type off = static_cast<size_type>(f - mFirst);
		size_type last = static_cast<size_type>(l - mFirst);
		if (off > last || last > mSize)
			outofrange();
		size_type m = last - off;
		if (m != 0)
		{
			std::memmove(mFirst + off, mFirst + last, mSize - last);
			SecureZero(mFirst + mSize - m, m);
			mSize -= m;
		}
		return mFirst + off;
	}

	void Blob::clear() noexcept
	{
		if (mCap != 0)
			SecureZero(mFirst, mSize);
		mSize = 0;
	}

	void Blob::swap(Blob &x) noexcept
	{
		std::swap(mFirst, x.mFirst);
		std::swap(mSize, x.mSize);
		std::swap(mCap, x.mCap);
	}

	bool Blob::_eq(const Blob &x) const
	{
		return mSize == x.mSize && (mSize == 0 || std::memcmp(mFirst, x.mFirst, mSize) == 0);
	}

	bool Blob::_lt(const Blob &x) const
	{
		size_type n = mSize < x.mSize ? mSize : x.mSize;
		int c = n == 0 ? 0 : std::memcmp(mFirst, x.mFirst, n);
		return c < 0 || (c == 0 && mSize < x.mSize);
	}

	void Blob::outofrange() const
	{
		throw OutOfRangeException("invalid Blob subscript", "Blob::outofrange");
	}

	std::ostream &operator<<(std::ostream &os, const Blob &blob)
	{
		static const char digits[] = "0123456789abcdef";
		for (Blob::const_iterator it = blob.begin(); it != blob.end(); ++it)
			os << digits[*it >> 4] << digits[*it & 0x0f];
		return os;
	}
} // namespace act
