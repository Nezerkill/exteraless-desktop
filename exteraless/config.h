#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace Exteraless {

class Config final {
public:
	static Config &Instance();

	void load();
	void save();

	[[nodiscard]] bool relativeLastSeen() const;
	void setRelativeLastSeen(bool value);

	[[nodiscard]] bool addCommaAfterMention() const;
	void setAddCommaAfterMention(bool value);

	[[nodiscard]] int proxyDisableConditions() const;
	void setProxyDisableConditions(int value);

	[[nodiscard]] bool proxyAutoDisabled() const;
	void setProxyAutoDisabled(bool value);

	[[nodiscard]] bool ghostSendReadPackets() const;
	void setGhostSendReadPackets(bool value);
	[[nodiscard]] bool ghostSendReadStories() const;
	void setGhostSendReadStories(bool value);
	[[nodiscard]] bool ghostSendOnline() const;
	void setGhostSendOnline(bool value);
	[[nodiscard]] bool ghostSendUploadProgress() const;
	void setGhostSendUploadProgress(bool value);
	[[nodiscard]] bool ghostSendOfflineAfterOnline() const;
	void setGhostSendOfflineAfterOnline(bool value);

	[[nodiscard]] bool ghostReadExcluded(qint64 peerId) const;
	void setGhostReadExcluded(qint64 peerId, bool value);
	[[nodiscard]] bool ghostTypingExcluded(qint64 peerId) const;
	void setGhostTypingExcluded(qint64 peerId, bool value);

	[[nodiscard]] bool saveDeletedMessages() const;
	void setSaveDeletedMessages(bool value);

	[[nodiscard]] QJsonObject toJson() const;
	bool fromJson(const QJsonObject &root, QStringList *unknown = nullptr);

private:
	Config() = default;

	void ensureLoaded() const;
	[[nodiscard]] bool boolValue(const char *key, bool fallback) const;
	[[nodiscard]] int intValue(const char *key, int fallback) const;
	void setValue(const char *key, const QJsonValue &value);

	mutable QJsonObject _values;
	mutable QString _path;
	mutable bool _loaded = false;
};

} // namespace Exteraless
