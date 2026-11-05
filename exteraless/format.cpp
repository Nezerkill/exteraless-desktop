#include "exteraless/format.h"

#include "exteraless/config.h"

namespace Exteraless {

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

} // namespace Exteraless
