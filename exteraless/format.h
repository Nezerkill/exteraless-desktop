#pragma once

#include <QDate>
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

[[nodiscard]] QString FormatIdDc(
	quint64 bareId,
	int dcId,
	bool enabled);

[[nodiscard]] QString PersianDateText(
	const QDate &date,
	bool latin,
	bool withYear);

} // namespace Exteraless
