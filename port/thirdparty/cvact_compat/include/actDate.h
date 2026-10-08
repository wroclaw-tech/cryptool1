// Source-compatible replacement for the cv act library (subset used by CrypTool).
// Dates are UTC; Encode() yields a DER UTCTime, EncodeToGeneralizedTime() a DER
// GeneralizedTime.
#ifndef ACT_Date_h
#define ACT_Date_h

#include <time.h>

#include "actBasics.h"

namespace act
{
	class Blob;

	class Date
	{
	public:
		Date();
		Date(int day, int month, int year);
		Date(int day, int month, int year, int hour, int minute, int sec);
		Date(const Blob &der);
		Date(const unsigned char *ch, const unsigned int size);

		Date &SetToday();

		int GetDay() const { return m_day; }
		int GetMonth() const { return m_month; }
		int GetYear() const { return m_year; }
		int GetHour() const { return m_hour; }
		int GetMinute() const { return m_min; }
		int GetSecond() const { return m_sec; }

		void IgnoreTime(bool b);

		const Date &operator=(const Date &);
		bool operator>(const Date &) const;
		bool operator>=(const Date &) const;
		bool operator<(const Date &) const;
		bool operator<=(const Date &) const;
		bool operator==(const Date &) const;
		bool operator!=(const Date &) const;

		const Date &AddMonths(int m);
		const Date &SubMonths(int m);
		const Date &AddYears(int y);
		const Date &SubYears(int y);
		const Date &AddDays(int d);
		const Date &SubDays(int d);
		const Date &AddHours(int h);
		const Date &SubHours(int h);
		const Date &AddMinutes(int m);
		const Date &SubMinutes(int m);
		const Date &AddSecond(int m);
		const Date &SubSecond(int m);

		int DayOfWeek() const; // 0: Sunday ... 6: Saturday
		int IsLeap(int y) const;
		int DaysPerMonth(int m, int y) const;
		long GetDifference(const Date &d2) const; // in days
		long GetJulian() const;
		long GetJulian(int d, int m, int y) const;
		void ConvertFromJulian(long jd, int &d, int &m, int &y);

		Blob Encode() const;
		Blob EncodeToGeneralizedTime() const;
		operator Blob() const;

		// Day of month of the which-th (1-based, 5 = last) given weekday in month/year.
		int GetDayFromWeekDay(int weekday, int year, int month, int which);

		static void copyDatetm(act::Date a, struct tm &b);
		static void copytmDate(struct tm a, act::Date &b);

	private:
		int m_day, m_month, m_year;
		int m_hour, m_min, m_sec;
		bool m_ignore_time;

		Date &SetYear(int year) { m_year = year; return *this; }
		Date &SetMonth(int month) { m_month = month; return *this; }
		Date &SetDay(int day) { m_day = day; return *this; }
		Date &SetHour(int hour) { m_hour = hour; return *this; }
		Date &SetMinute(int minute) { m_min = minute; return *this; }
		Date &SetSecond(int sec) { m_sec = sec; return *this; }
		bool IsValid() const;
		void AdjustDays();
		void Decode(const unsigned char *ch, size_t size);
		long long SecondsSinceEpochDay() const;
		void SetFromSeconds(long long s);
		int Compare(const Date &other) const;
	};
} // namespace act

#endif // ACT_Date_h
