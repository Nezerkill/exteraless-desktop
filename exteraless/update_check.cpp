#include "exteraless/update_check.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

namespace Exteraless {
namespace {

constexpr auto kReleasesPage = "https://github.com/Nezerkill/exteraless-desktop/releases";
constexpr auto kReleasesApi = "https://api.github.com/repos/Nezerkill/exteraless-desktop/releases";
constexpr auto kVersionParts = 4;

struct Version {
	int parts[kVersionParts] = { 0, 0, 0, 0 };
};

[[nodiscard]] QString ReleasesPage() {
	return QString::fromUtf8(kReleasesPage);
}

[[nodiscard]] bool ParseVersion(const QString &tag, Version &result) {
	auto text = tag.trimmed();
	if (text.startsWith(u'v') || text.startsWith(u'V')) {
		text.remove(0, 1);
	}
	auto cut = text.size();
	const auto dash = text.indexOf(u'-');
	if (dash >= 0 && dash < cut) {
		cut = dash;
	}
	const auto plus = text.indexOf(u'+');
	if (plus >= 0 && plus < cut) {
		cut = plus;
	}
	const auto base = text.left(cut);
	if (base.isEmpty()) {
		return false;
	}
	const auto chunks = base.split(u'.');
	for (auto index = 0; index != chunks.size() && index != kVersionParts; ++index) {
		auto ok = false;
		const auto value = chunks[index].toInt(&ok);
		if (!ok || value < 0) {
			return false;
		}
		result.parts[index] = value;
	}
	return true;
}

[[nodiscard]] int CompareVersions(const Version &left, const Version &right) {
	for (auto index = 0; index != kVersionParts; ++index) {
		if (left.parts[index] == right.parts[index]) {
			continue;
		}
		return (left.parts[index] < right.parts[index]) ? -1 : 1;
	}
	return 0;
}

} // namespace

// WHY: fetching stays with the UI caller, so this module links into CLI tests.
UpdateInfo ParseReleasesJson(
		const QByteArray &data,
		const QString &current,
		bool includePrerelease) {
	auto info = UpdateInfo();
	info.url = ReleasesPage();

	auto currentVersion = Version();
	if (!ParseVersion(current, currentVersion)) {
		info.error = QStringLiteral("current version is not semver");
		return info;
	}
	auto parseError = QJsonParseError();
	const auto document = QJsonDocument::fromJson(data, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		info.error = parseError.errorString();
		return info;
	}
	if (!document.isArray()) {
		info.error = QStringLiteral("releases response is not an array");
		return info;
	}
	const auto releases = document.array();
	auto found = false;
	auto bestTag = QString();
	auto bestUrl = QString();
	auto bestVersion = Version();
	for (auto index = qsizetype(0); index != releases.size(); ++index) {
		const auto release = releases[index].toObject();
		if (release.value(QStringLiteral("draft")).toBool()) {
			continue;
		}
		if (!includePrerelease
			&& release.value(QStringLiteral("prerelease")).toBool()) {
			continue;
		}
		const auto tag = release
			.value(QStringLiteral("tag_name"))
			.toString()
			.trimmed();
		auto version = Version();
		if (!ParseVersion(tag, version)
			|| CompareVersions(version, currentVersion) <= 0) {
			continue;
		}
		if (found && CompareVersions(version, bestVersion) <= 0) {
			continue;
		}
		found = true;
		bestVersion = version;
		bestTag = tag;
		bestUrl = release
			.value(QStringLiteral("html_url"))
			.toString()
			.trimmed();
	}
	if (!found) {
		return info;
	}
	info.available = true;
	info.version = bestTag;
	if (!bestUrl.isEmpty()) {
		info.url = bestUrl;
	}
	return info;
}

QString ReleasesApiUrl() {
	return QString::fromUtf8(kReleasesApi);
}

} // namespace Exteraless
