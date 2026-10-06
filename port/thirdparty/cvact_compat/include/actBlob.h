// Source-compatible replacement for the cv act library (subset used by CrypTool).
// Blob behaves like std::vector<unsigned char>; released memory is wiped. The
// storage always keeps one zero byte behind the last element, so &blob[blob.size()]
// is addressable and the contents can be read as a C string.
#ifndef ACT_BLOB_H
#define ACT_BLOB_H

#include "actBasics.h"
#include "actException.h"

#include <iosfwd>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

namespace act
{
	void SecureZero(void *p, size_t n) noexcept;

	class Blob
	{
	public:
		typedef unsigned char byte;
		typedef byte &reference;
		typedef const byte &const_reference;
		typedef size_t size_type;
		typedef ptrdiff_t difference_type;
		typedef byte value_type;
		typedef byte *pointer;
		typedef const byte *const_pointer;
		typedef byte *iterator;
		typedef const byte *const_iterator;
		typedef std::reverse_iterator<iterator> reverse_iterator;
		typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

		static pointer get_base(iterator it) { return it; }
		static const_pointer get_base(const_iterator it) { return it; }

		explicit Blob(const char *str);
		explicit Blob(std::string &str);
		Blob() noexcept;
		Blob(size_type n, byte v = byte(0));
		Blob(const Blob &x);
		Blob(Blob &&x) noexcept;

		template<class InputIt, typename = typename std::enable_if<!std::is_integral<InputIt>::value>::type>
		Blob(InputIt f, InputIt l) : Blob()
		{
			insert(end(), f, l);
		}

		~Blob();

		Blob &operator=(const Blob &x);
		Blob &operator=(Blob &&x) noexcept;

		template<class InputIt, typename = typename std::enable_if<!std::is_integral<InputIt>::value>::type>
		void assign(InputIt f, InputIt l)
		{
			Blob tmp(f, l);
			swap(tmp);
		}
		void assign(size_type n, byte x = byte(0));

		iterator begin() noexcept { return mFirst; }
		const_iterator begin() const noexcept { return mFirst; }
		iterator end() noexcept { return mFirst + mSize; }
		const_iterator end() const noexcept { return mFirst + mSize; }
		reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
		const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
		reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
		const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

		size_type size() const noexcept { return mSize; }
		size_type max_size() const noexcept { return size_type(-1) / 2; }
		void resize(size_type n, byte x = byte(0));
		size_type capacity() const noexcept { return mCap; }
		bool empty() const noexcept { return mSize == 0; }
		void reserve(size_type n);

		const_reference operator[](size_type p) const { return mFirst[p]; }
		reference operator[](size_type p) { return mFirst[p]; }
		const_reference at(size_type p) const;
		reference at(size_type p);
		reference front() { return mFirst[0]; }
		const_reference front() const { return mFirst[0]; }
		reference back() { return mFirst[mSize - 1]; }
		const_reference back() const { return mFirst[mSize - 1]; }

		void push_back(byte x);
		void pop_back();
		iterator insert(iterator p, byte x = byte(0));
		void insert(iterator p, size_type m, byte x);

		template<class InputIt, typename = typename std::enable_if<!std::is_integral<InputIt>::value>::type>
		void insert(iterator p, InputIt f, InputIt l)
		{
			std::vector<byte> tmp;
			for (; f != l; ++f)
				tmp.push_back(static_cast<byte>(*f));
			insert_raw(p, tmp.empty() ? 0 : &tmp[0], tmp.size());
			if (!tmp.empty())
				SecureZero(&tmp[0], tmp.size());
		}

		void append(const Blob &x);
		void append(size_type m, byte x);
		template<class InputIt, typename = typename std::enable_if<!std::is_integral<InputIt>::value>::type>
		void append(InputIt f, InputIt l)
		{
			insert(end(), f, l);
		}

		iterator erase(iterator p);
		iterator erase(iterator f, iterator l);
		void clear() noexcept;
		void swap(Blob &x) noexcept;

		bool _eq(const Blob &x) const;
		bool _lt(const Blob &x) const;

	private:
		iterator insert_raw(iterator p, const byte *src, size_type m);
		iterator make_gap(iterator p, size_type m);
		void reallocate(size_type cap);
		void release() noexcept;
		void outofrange() const;

		byte *mFirst;
		size_type mSize;
		size_type mCap;
	};

	std::ostream &operator<<(std::ostream &os, const Blob &blob);

	inline bool operator==(const Blob &x, const Blob &y) { return x._eq(y); }
	inline bool operator!=(const Blob &x, const Blob &y) { return !x._eq(y); }
	inline bool operator<(const Blob &x, const Blob &y) { return x._lt(y); }
	inline bool operator>=(const Blob &x, const Blob &y) { return !x._lt(y); }
	inline bool operator<=(const Blob &x, const Blob &y) { return x._eq(y) || x._lt(y); }
	inline bool operator>(const Blob &x, const Blob &y) { return !(x._eq(y) || x._lt(y)); }
} // namespace act

#endif // ACT_BLOB_H
