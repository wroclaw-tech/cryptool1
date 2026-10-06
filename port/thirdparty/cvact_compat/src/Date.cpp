#include "actDate.h"

#include "actBlob.h"
#include "actException.h"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace act
{
	namespace
	{
		long long FloorDiv(long long a, long long b)
		{
			long long q = a / b;
			if ((a % b != 0) && ((a < 0) != (b < 0)))
				--q;
			return q;
		}

		int Digits(const unsigned char *p, int n, bool &ok)
		{
			int v = 0;
			for (int i = 0; i < n; ++i)
			{
				if (p[i] < '0' || p[i] > '9')
					ok = false;
				v = v * 10 + (p[i] - '0');
			}
			return v;
		}
	}

	Date::Date() : m_day(1), m_month(1), m_year(1970), m_hour(0), m_min(0), m_sec(0), m_ignore_time(false)
	{
		SetToday();
	}

	Date::Date(int day, int month, int year)
		: m_day(day), m_month(month), m_year(year), m_hour(0), m_min(0), m_sec(0), m_ignore_time(false)
	{
		if (!IsValid())
			throw BadException("invalid date", "Date::Date");
	}

	Date::Date(int day, int month, int year, int hour, int minute, int sec)
		: m_day(day), m_month(month), m_year(year), m_hour(hour), m_min(minute), m_sec(sec), m_ignore_time(false)
	{
		if (!IsValid())
			throw BadException("invalid date", "Date::Date");
	}

	Date::Date(const Blob &der)
		: m_day(1), m_month(1), m_year(1970), m_hour(0), m_min(0), m_sec(0), m_ignore_time(false)
	{
		Decode(der.begin(), der.size());
	}

	Date::Date(const unsigned char *ch, const unsigned int size)
		: m_day(1), m_month(1), m_year(1970), m_hour(0), m_min(0), m_sec(0), m_ignore_time(false)
	{
		Decode(ch, size);
	}

	void Date::Decode(const unsigned char *ch, size_t size)
	{
		if (ch == 0)
			throw NullPointerException("null date encoding", "Date::Decode");
		if (size >= 2 && (ch[0] == 0x17 || ch[0] == 0x18) && ch[1] == size - 2)
		{
			ch += 2;
			size -= 2;
		}
		bool ok = true;
		if (size == 13 && ch[12] == 'Z')
		{
			int yy = Digits(ch, 2, ok);
			m_year = yy < 50 ? 2000 + yy : 1900 + yy;
			ch += 2;
		}
		else if (size == 15 && ch[14] == 'Z')
		{
			m_year = Digits(ch, 4, ok);
			ch += 4;
		}
		else
			throw ASN1Exception("unsupported time encoding", "Date::Decode");
		m_month = Digits(ch, 2, ok);
		m_day = Digits(ch + 2, 2, ok);
		m_hour = Digits(ch + 4, 2, ok);
		m_min = Digits(ch + 6, 2, ok);
		m_sec = Digits(ch + 8, 2, ok);
		if (!ok || !IsValid())
			throw ASN1Exception("invalid time encoding", "Date::Decode");
	}

	Date &Date::SetToday()
	{
		std::time_t now = std::time(0);
		struct tm t;
#if defined(_WIN32)
		gmtime_s(&t, &now);
#else
		gmtime_r(&now, &t);
#endif
		copytmDate(t, *this);
		return *this;
	}

	void Date::IgnoreTime(bool b)
	{
		m_ignore_time = b;
	}

	const Date &Date::operator=(const Date &other)
	{
		m_day = other.m_day;
		m_month = other.m_month;
		m_year = other.m_year;
		m_hour = other.m_hour;
		m_min = other.m_min;
		m_sec = other.m_sec;
		m_ignore_time = other.m_ignore_time;
		return *this;
	}

	int Date::Compare(const Date &other) const
	{
		long a = GetJulian();
		long b = other.GetJulian();
		if (a != b)
			return a < b ? -1 : 1;
		if (m_ignore_time || other.m_ignore_time)
			return 0;
		long sa = m_hour * 3600L + m_min * 60L + m_sec;
		long sb = other.m_hour * 3600L + other.m_min * 60L + other.m_sec;
		return sa < sb ? -1 : (sa > sb ? 1 : 0);
	}

	bool Date::operator>(const Date &d) const { return Compare(d) > 0; }
	bool Date::operator>=(const Date &d) const { return Compare(d) >= 0; }
	bool Date::operator<(const Date &d) const { return Compare(d) < 0; }
	bool Date::operator<=(const Date &d) const { return Compare(d) <= 0; }
	bool Date::operator==(const Date &d) const { return Compare(d) == 0; }
	bool Date::operator!=(const Date &d) const { return Compare(d) != 0; }

	const Date &Date::AddMonths(int m)
	{
		long long total = static_cast<long long>(m_year) * 12 + (m_month - 1) + m;
		m_year = static_cast<int>(FloorDiv(total, 12));
		m_month = static_cast<int>(total - static_cast<long long>(m_year) * 12) + 1;
		AdjustDays();
		return *this;
	}

	const Date &Date::SubMonths(int m) { return AddMonths(-m); }
	const Date &Date::AddYears(int y) { m_year += y; AdjustDays(); return *this; }
	const Date &Date::SubYears(int y) { return AddYears(-y); }
	const Date &Date::AddDays(int d) { SetFromSeconds(SecondsSinceEpochDay() + 86400LL * d); return *this; }
	const Date &Date::SubDays(int d) { return AddDays(-d); }
	const Date &Date::AddHours(int h) { SetFromSeconds(SecondsSinceEpochDay() + 3600LL * h); return *this; }
	const Date &Date::SubHours(int h) { return AddHours(-h); }
	const Date &Date::AddMinutes(int m) { SetFromSeconds(SecondsSinceEpochDay() + 60LL * m); return *this; }
	const Date &Date::SubMinutes(int m) { return AddMinutes(-m); }
	const Date &Date::AddSecond(int m) { SetFromSeconds(SecondsSinceEpochDay() + m); return *this; }
	const Date &Date::SubSecond(int m) { return AddSecond(-m); }

	int Date::DayOfWeek() const
	{
		return static_cast<int>((GetJulian() + 1) % 7);
	}

	int Date::IsLeap(int y) const
	{
		return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0 ? 1 : 0;
	}

	int Date::DaysPerMonth(int m, int y) const
	{
		static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
		if (m < 1 || m > 12)
			return 0;
		return m == 2 ? 28 + IsLeap(y) : days[m - 1];
	}

	long Date::GetDifference(const Date &d2) const
	{
		return GetJulian() - d2.GetJulian();
	}

	long Date::GetJulian() const
	{
		return GetJulian(m_day, m_month, m_year);
	}

	long Date::GetJulian(int d, int m, int y) const
	{
		long a = (14 - m) / 12;
		long yy = y + 4800L - a;
		long mm = m + 12L * a - 3;
		return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
	}

	void Date::ConvertFromJulian(long jd, int &d, int &m, int &y)
	{
		long a = jd + 32044;
		long b = (4 * a + 3) / 146097;
		long c = a - 146097 * b / 4;
		long dd = (4 * c + 3) / 1461;
		long e = c - 1461 * dd / 4;
		long mm = (5 * e + 2) / 153;
		d = static_cast<int>(e - (153 * mm + 2) / 5 + 1);
		m = static_cast<int>(mm + 3 - 12 * (mm / 10));
		y = static_cast<int>(100 * b + dd - 4800 + mm / 10);
	}

	long long Date::SecondsSinceEpochDay() const
	{
		return static_cast<long long>(GetJulian()) * 86400LL + m_hour * 3600LL + m_min * 60LL + m_sec;
	}

	void Date::SetFromSeconds(long long s)
	{
		long long days = FloorDiv(s, 86400);
		long long rest = s - days * 86400;
		ConvertFromJulian(static_cast<long>(days), m_day, m_month, m_year);
		m_hour = static_cast<int>(rest / 3600);
		m_min = static_cast<int>((rest / 60) % 60);
		m_sec = static_cast<int>(rest % 60);
	}

	Blob Date::Encode() const
	{
		if (m_year < 1950 || m_year >= 2050)
			return EncodeToGeneralizedTime();
		char buf[16];
		std::snprintf(buf, sizeof(buf), "%02d%02d%02d%02d%02d%02dZ",
			m_year % 100, m_month, m_day, m_hour, m_min, m_sec);
		Blob out;
		out.push_back(0x17);
		out.push_back(static_cast<Blob::byte>(std::strlen(buf)));
		out.insert(out.end(), buf, buf + std::strlen(buf));
		return out;
	}

	Blob Date::EncodeToGeneralizedTime() const
	{
		char buf[20];
		std::snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d%02dZ",
			m_year, m_month, m_day, m_hour, m_min, m_sec);
		Blob out;
		out.push_back(0x18);
		out.push_back(static_cast<Blob::byte>(std::strlen(buf)));
		out.insert(out.end(), buf, buf + std::strlen(buf));
		return out;
	}

	Date::operator Blob() const
	{
		return Encode();
	}

	int Date::GetDayFromWeekDay(int weekday, int year, int month, int which)
	{
		int first = static_cast<int>((GetJulian(1, month, year) + 1) % 7);
		int day = 1 + ((weekday - first) % 7 + 7) % 7;
		int last = DaysPerMonth(month, year);
		day += 7 * (which > 0 ? which - 1 : 0);
		while (day > last)
			day -= 7;
		return day;
	}

	bool Date::IsValid() const
	{
		return m_month >= 1 && m_month <= 12 && m_day >= 1 && m_day <= DaysPerMonth(m_month, m_year)
			&& m_hour >= 0 && m_hour <= 23 && m_min >= 0 && m_min <= 59 && m_sec >= 0 && m_sec <= 59;
	}

	void Date::AdjustDays()
	{
		int last = DaysPerMonth(m_month, m_year);
		if (m_day > last)
			m_day = last;
	}

	void Date::copyDatetm(act::Date a, struct tm &b)
	{
		std::memset(&b, 0, sizeof(b));
		b.tm_year = a.m_year - 1900;
		b.tm_mon = a.m_month - 1;
		b.tm_mday = a.m_day;
		b.tm_hour = a.m_hour;
		b.tm_min = a.m_min;
		b.tm_sec = a.m_sec;
		b.tm_wday = a.DayOfWeek();
		b.tm_yday = static_cast<int>(a.GetJulian() - a.GetJulian(1, 1, a.m_year));
		b.tm_isdst = 0;
	}

	void Date::copytmDate(struct tm a, act::Date &b)
	{
		b.m_year = a.tm_year + 1900;
		b.m_month = a.tm_mon + 1;
		b.m_day = a.tm_mday;
		b.m_hour = a.tm_hour;
		b.m_min = a.tm_min;
		b.m_sec = a.tm_sec > 59 ? 59 : a.tm_sec;
	}
} // namespace act
