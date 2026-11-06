#include "exteraless/format.h"

#include "exteraless/config.h"

#include <QCalendar>

#include <array>

namespace Exteraless {
namespace {

struct PersianMonth {
	const char *persian;
	const char *latin;
};

constexpr auto kPersianMonths = std::array<PersianMonth, 12>{ {
	{ "فروردین", "Farvardin" },
	{ "اردیبهشت", "Ordibehesht" },
	{ "خرداد", "Khordad" },
	{ "تیر", "Tir" },
	{ "مرداد", "Mordad" },
	{ "شهریور", "Shahrivar" },
	{ "مهر", "Mehr" },
	{ "آبان", "Aban" },
	{ "آذر", "Azar" },
	{ "دی", "Dey" },
	{ "بهمن", "Bahman" },
	{ "اسفند", "Esfand" },
} };

[[nodiscard]] QString PersianNumber(int value, bool latin) {
	const auto text = QString::number(value);
	if (latin) {
		return text;
	}
	auto result = QString();
	result.reserve(text.size());
	for (const auto &ch : text) {
		const auto digit = ch.unicode() - u'0';
		if ((digit < 0) || (digit > 9)) {
			result += ch;
			continue;
		}
		result += QChar(char16_t(0x06F0 + digit));
	}
	return result;
}

} // namespace

QString FormatClock(
		QTime time,
		bool withSeconds,
		const QLocale &locale) {
	if (!withSeconds) {
		return locale.toString(time, QLocale::ShortFormat);
	}
	auto format = locale.timeFormat(QLocale::ShortFormat);
	const auto index = format.indexOf(QStringLiteral("mm"));
	if (index >= 0) {
		format.insert(index + 2, QStringLiteral(":ss"));
	} else {
		const auto singleM = format.indexOf(QChar(u'm'));
		if (singleM >= 0) {
			format.insert(singleM + 1, QStringLiteral(":ss"));
		} else {
			return locale.toString(time, QStringLiteral("HH:mm:ss"));
		}
	}
	return locale.toString(time, format);
}

QString FormatClock(QTime time, const QLocale &locale) {
	return FormatClock(
		time,
		Config::Instance().showSeconds(),
		locale);
}

QString FormatIdDc(quint64 bareId, int dcId, bool enabled) {
	if (!enabled) {
		return {};
	}
	return QStringLiteral("id: %1 · DC %2")
		.arg(bareId)
		.arg(dcId);
}

QString PersianDateText(const QDate &date, bool latin, bool withYear) {
	if (!date.isValid()) {
		return {};
	}
	const auto calendar = QCalendar(QCalendar::System::Jalali);
	const auto parts = calendar.partsFromDate(date);
	if (!parts.isValid()
		|| (parts.month < 1)
		|| (parts.month > 12)) {
		return {};
	}
	const auto &month = kPersianMonths[parts.month - 1];
	auto result = PersianNumber(parts.day, latin)
		+ QChar(u' ')
		+ QString::fromUtf8(latin ? month.latin : month.persian);
	if (withYear) {
		result += QChar(u' ') + PersianNumber(parts.year, latin);
	}
	return result;
}

} // namespace Exteraless
