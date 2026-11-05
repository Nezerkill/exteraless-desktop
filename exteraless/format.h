#pragma once

#include <QLocale>
#include <QString>
#include <QTime>

namespace Exteraless {

[[nodiscard]] QString FormatClock(
	QTime time,
	bool withSeconds,
	const QLocale &locale = QLocale());

[[nodiscard]] QString FormatClock(
	QTime time,
	const QLocale &locale = QLocale());

} // namespace Exteraless
