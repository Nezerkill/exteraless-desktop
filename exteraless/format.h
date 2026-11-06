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

[[nodiscard]] QString FormatIdDc(
	quint64 bareId,
	int dcId,
	bool enabled);

} // namespace Exteraless
