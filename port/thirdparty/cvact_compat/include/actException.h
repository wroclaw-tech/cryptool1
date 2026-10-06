// Source-compatible replacement for the cv act library (subset used by CrypTool).
#ifndef ACT_EXCEPTION_H
#define ACT_EXCEPTION_H

#include <type_traits>
#include <typeinfo>
#include <utility>

namespace act
{
	class FileAndLine
	{
	public:
		FileAndLine(const char *file = 0, int line = 0) noexcept : m_line(line), m_file(file) {}
		FileAndLine(const FileAndLine &other) noexcept : m_line(other.line()), m_file(other.file()) {}
		virtual ~FileAndLine() {}

		virtual int line() const noexcept { return m_line; }
		virtual const char *file() const noexcept { return m_file; }

		FileAndLine &operator=(const FileAndLine &other) noexcept
		{
			m_line = other.line();
			m_file = other.file();
			return *this;
		}

	private:
		int m_line;
		const char *m_file;
	};

	// Messages are not copied: only string literals or otherwise static strings may be passed.
	class Exception
	{
	public:
		explicit Exception(const char *msg = 0, const char *where = 0) noexcept : m_what(msg), m_where(where) {}

		template<typename T,
			typename = decltype(std::declval<const T &>().what()),
			typename = decltype(std::declval<const T &>().where()),
			typename = typename std::enable_if<!std::is_base_of<Exception, T>::value>::type>
		Exception(const T &other) : m_what(other.what()), m_where(other.where()) {}

		virtual ~Exception() noexcept {}

		virtual const char *what() const noexcept { return m_what != 0 ? m_what : ""; }
		virtual const char *where() const noexcept { return m_where != 0 ? m_where : ""; }

	private:
		const char *m_what;
		const char *m_where;
	};

#define ACT_COMPAT_EXCEPTION(name, base) \
	class name : public base \
	{ \
	public: \
		name(const char *msg = 0, const char *where = 0) : base(msg, where) {} \
	};

	ACT_COMPAT_EXCEPTION(BadException, Exception)
	ACT_COMPAT_EXCEPTION(NullPointerException, BadException)
	ACT_COMPAT_EXCEPTION(OutOfRangeException, BadException)

	ACT_COMPAT_EXCEPTION(LogicalException, Exception)
	ACT_COMPAT_EXCEPTION(ASN1Exception, LogicalException)
	ACT_COMPAT_EXCEPTION(NoSuchDLLException, LogicalException)
	ACT_COMPAT_EXCEPTION(PasswordException, LogicalException)
	ACT_COMPAT_EXCEPTION(AlgorithmException, LogicalException)
	ACT_COMPAT_EXCEPTION(InvalidAlgorithmParameterException, AlgorithmException)
	ACT_COMPAT_EXCEPTION(NoSuchAlgorithmException, AlgorithmException)
	ACT_COMPAT_EXCEPTION(ArithmeticException, LogicalException)
	ACT_COMPAT_EXCEPTION(CertificateException, LogicalException)
	ACT_COMPAT_EXCEPTION(CertificateEncodingException, CertificateException)
	ACT_COMPAT_EXCEPTION(CertificateExpiredException, CertificateException)
	ACT_COMPAT_EXCEPTION(CertificateNotYetValidException, CertificateException)
	ACT_COMPAT_EXCEPTION(CertificateParsingException, CertificateException)
	ACT_COMPAT_EXCEPTION(KeyException, LogicalException)
	ACT_COMPAT_EXCEPTION(InvalidKeyException, KeyException)
	ACT_COMPAT_EXCEPTION(KeyManagementException, KeyException)
	ACT_COMPAT_EXCEPTION(MessageDigestException, LogicalException)
	ACT_COMPAT_EXCEPTION(PaddingException, LogicalException)
	ACT_COMPAT_EXCEPTION(SignatureException, LogicalException)
	ACT_COMPAT_EXCEPTION(SubsystemException, LogicalException)
	ACT_COMPAT_EXCEPTION(NoSuchSubsystemException, SubsystemException)

	ACT_COMPAT_EXCEPTION(RuntimeException, Exception)
	ACT_COMPAT_EXCEPTION(BadAllocException, RuntimeException)

#undef ACT_COMPAT_EXCEPTION

	class SmartcardException : public LogicalException
	{
	public:
		SmartcardException(const char *msg = 0, const char *where = 0, long code = 0)
			: LogicalException(msg, where), m_code(code) {}

		virtual long code() const noexcept { return m_code; }

	private:
		long m_code;
	};

#define ACT_COMPAT_SC_EXCEPTION(name) \
	class name : public SmartcardException \
	{ \
	public: \
		name(const char *msg = 0, const char *where = 0, long code = 0) : SmartcardException(msg, where, code) {} \
	};

	ACT_COMPAT_SC_EXCEPTION(InvalidPinException)
	ACT_COMPAT_SC_EXCEPTION(PinLockedException)
	ACT_COMPAT_SC_EXCEPTION(PinExpiredException)
	ACT_COMPAT_SC_EXCEPTION(PinLenRangeException)
	ACT_COMPAT_SC_EXCEPTION(InvalidCardException)
	ACT_COMPAT_SC_EXCEPTION(CardOutOfMemoryException)

#undef ACT_COMPAT_SC_EXCEPTION

	class NotImplementedException : public FileAndLine, public RuntimeException
	{
	public:
		explicit NotImplementedException(const char *msg = 0, const char *where = 0)
			: RuntimeException(msg, where) {}

		NotImplementedException(const char *msg, const char *where, const char *file, int line)
			: FileAndLine(file, line), RuntimeException(msg, where) {}
	};
} // namespace act

#endif // ACT_EXCEPTION_H
