#include "exteraless/config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QStandardPaths>

namespace Exteraless {
namespace {

constexpr auto kVersion = 1;
constexpr auto kRelativeLastSeen = "OERelativeLastSeen";
constexpr auto kAddCommaAfterMention = "OEAddCommaAfterMention";
constexpr auto kProxyDisableConditions = "OEProxyDisableConditions";
constexpr auto kProxyAutoDisabled = "OEProxyAutoDisabled";
constexpr auto kGhostSendReadPackets = "OEGhostSendReadPackets";
constexpr auto kGhostSendReadStories = "OEGhostSendReadStories";
constexpr auto kGhostSendOnline = "OEGhostSendOnline";
constexpr auto kGhostSendUploadProgress = "OEGhostSendUploadProgress";
constexpr auto kGhostSendOfflineAfterOnline = "OEGhostSendOfflineAfterOnline";
constexpr auto kGhostReadExclusions = "OEGhostReadExclusions";
constexpr auto kGhostTypingExclusions = "OEGhostTypingExclusions";
constexpr auto kSaveDeletedMessages = "OESaveDeletedMessages";
constexpr auto kPillStackEnabled = "OEPillStackEnabled";
constexpr auto kPillStackPills = "OEPillStackPills";

[[nodiscard]] QJsonObject Unwrap(const QJsonObject &root) {
	const auto values = root.value("exteraconfig");
	return values.isObject() ? values.toObject() : root;
}

} // namespace

Config &Config::Instance() {
	static auto instance = Config();
	return instance;
}

void Config::load() {
	ensureLoaded();
}

void Config::save() {
	ensureLoaded();

	auto root = QJsonObject();
	root["version"] = kVersion;
	root["exteraconfig"] = _values;

	const auto directory = QDir(QFileInfo(_path).absolutePath());
	if (!directory.exists() && !directory.mkpath(".")) {
		return;
	}

	auto file = QSaveFile(_path);
	if (!file.open(QIODevice::WriteOnly)) {
		return;
	}
	file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	file.commit();
}

bool Config::relativeLastSeen() const {
	return boolValue(kRelativeLastSeen, false);
}

void Config::setRelativeLastSeen(bool value) {
	setValue(kRelativeLastSeen, value);
}

bool Config::addCommaAfterMention() const {
	return boolValue(kAddCommaAfterMention, true);
}

void Config::setAddCommaAfterMention(bool value) {
	setValue(kAddCommaAfterMention, value);
}

int Config::proxyDisableConditions() const {
	return intValue(kProxyDisableConditions, 0);
}

void Config::setProxyDisableConditions(int value) {
	setValue(kProxyDisableConditions, value);
}

bool Config::proxyAutoDisabled() const {
	return boolValue(kProxyAutoDisabled, false);
}

void Config::setProxyAutoDisabled(bool value) {
	setValue(kProxyAutoDisabled, value);
}

bool Config::ghostSendReadPackets() const {
	return boolValue(kGhostSendReadPackets, true);
}

void Config::setGhostSendReadPackets(bool value) {
	setValue(kGhostSendReadPackets, value);
}

bool Config::ghostSendReadStories() const {
	return boolValue(kGhostSendReadStories, true);
}

void Config::setGhostSendReadStories(bool value) {
	setValue(kGhostSendReadStories, value);
}

bool Config::ghostSendOnline() const {
	return boolValue(kGhostSendOnline, true);
}

void Config::setGhostSendOnline(bool value) {
	setValue(kGhostSendOnline, value);
}

bool Config::ghostSendUploadProgress() const {
	return boolValue(kGhostSendUploadProgress, true);
}

void Config::setGhostSendUploadProgress(bool value) {
	setValue(kGhostSendUploadProgress, value);
}

bool Config::ghostSendOfflineAfterOnline() const {
	return boolValue(kGhostSendOfflineAfterOnline, false);
}

void Config::setGhostSendOfflineAfterOnline(bool value) {
	setValue(kGhostSendOfflineAfterOnline, value);
}

bool Config::ghostReadExcluded(qint64 peerId) const {
	ensureLoaded();
	const auto key = QString::number(peerId < 0 ? -peerId : peerId);
	return _values.value(kGhostReadExclusions).toObject().value(key).toBool();
}

void Config::setGhostReadExcluded(qint64 peerId, bool value) {
	ensureLoaded();
	const auto key = QString::number(peerId < 0 ? -peerId : peerId);
	auto exclusions = _values.value(kGhostReadExclusions).toObject();
	if (value) {
		exclusions[key] = true;
	} else {
		exclusions.remove(key);
	}
	setValue(kGhostReadExclusions, exclusions);
}

bool Config::ghostTypingExcluded(qint64 peerId) const {
	ensureLoaded();
	const auto key = QString::number(peerId < 0 ? -peerId : peerId);
	return _values.value(kGhostTypingExclusions).toObject().value(key).toBool();
}

void Config::setGhostTypingExcluded(qint64 peerId, bool value) {
	ensureLoaded();
	const auto key = QString::number(peerId < 0 ? -peerId : peerId);
	auto exclusions = _values.value(kGhostTypingExclusions).toObject();
	if (value) {
		exclusions[key] = true;
	} else {
		exclusions.remove(key);
	}
	setValue(kGhostTypingExclusions, exclusions);
}

bool Config::saveDeletedMessages() const {
	return boolValue(kSaveDeletedMessages, false);
}

void Config::setSaveDeletedMessages(bool value) {
	setValue(kSaveDeletedMessages, value);
}

bool Config::pillStackEnabled() const {
	return boolValue(kPillStackEnabled, false);
}

void Config::setPillStackEnabled(bool value) {
	setValue(kPillStackEnabled, value);
}

int Config::pillStackPills() const {
	return intValue(kPillStackPills, kPillStackAllPills);
}

void Config::setPillStackPills(int value) {
	setValue(kPillStackPills, value);
}

QJsonObject Config::toJson() const {
	ensureLoaded();
	auto root = QJsonObject();
	root["version"] = kVersion;
	root["exteraconfig"] = _values;
	return root;
}

bool Config::fromJson(const QJsonObject &root, QStringList *unknown) {
	ensureLoaded();

	const auto values = Unwrap(root);
	auto accepted = QJsonObject();
	for (auto i = values.begin(); i != values.end(); ++i) {
		const auto key = i.key();
		if (key == kRelativeLastSeen
			|| key == kAddCommaAfterMention
			|| key == kProxyDisableConditions
			|| key == kProxyAutoDisabled
			|| key == kGhostSendReadPackets
			|| key == kGhostSendReadStories
			|| key == kGhostSendOnline
			|| key == kGhostSendUploadProgress
			|| key == kGhostSendOfflineAfterOnline
			|| key == kGhostReadExclusions
			|| key == kGhostTypingExclusions
			|| key == kSaveDeletedMessages
			|| key == kPillStackEnabled
			|| key == kPillStackPills) {
			accepted.insert(key, i.value());
		} else if (unknown) {
			unknown->push_back(key);
		}
	}
	if (accepted.isEmpty()) {
		return false;
	}
	_values = accepted;
	save();
	return true;
}

void Config::ensureLoaded() const {
	if (_loaded) {
		return;
	}
	_loaded = true;
	_path = QStandardPaths::writableLocation(
		QStandardPaths::AppLocalDataLocation) + "/tdata/exteraless.json";

	auto file = QFile(_path);
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	auto error = QJsonParseError{0, QJsonParseError::NoError};
	const auto document = QJsonDocument::fromJson(file.readAll(), &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return;
	}
	_values = Unwrap(document.object());
}

bool Config::boolValue(const char *key, bool fallback) const {
	ensureLoaded();
	const auto value = _values.value(key);
	return value.isBool() ? value.toBool() : fallback;
}

int Config::intValue(const char *key, int fallback) const {
	ensureLoaded();
	const auto value = _values.value(key);
	return value.isDouble() ? value.toInt() : fallback;
}

void Config::setValue(const char *key, const QJsonValue &value) {
	ensureLoaded();
	if (_values.value(key) == value) {
		return;
	}
	_values[key] = value;
	save();
}

} // namespace Exteraless
