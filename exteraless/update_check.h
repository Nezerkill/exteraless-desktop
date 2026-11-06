#pragma once

#include <QByteArray>
#include <QString>

namespace Exteraless {

struct UpdateInfo {
	bool available = false;
	QString version;
	QString url;
	QString error;
};

[[nodiscard]] UpdateInfo ParseReleasesJson(
	const QByteArray &data,
	const QString &current,
	bool includePrerelease);

[[nodiscard]] QString ReleasesApiUrl();

} // namespace Exteraless
