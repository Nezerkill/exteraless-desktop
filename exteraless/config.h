#pragma once

#include "exteraless/chats.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace Exteraless {

enum class PillStackPill : int {
	Cpu = 1,
	Ram = 2,
	Net = 4,
	Dc = 8,
	Proxy = 16,
	Cache = 32,
	Ghost = 64,
	LastSeen = 128,
};

constexpr auto kPillStackAllPills = 0xFF;

[[nodiscard]] constexpr bool HasPillStackPill(
		int mask,
		PillStackPill pill) {
	return (mask & int(pill)) != 0;
}

[[nodiscard]] constexpr int WithPillStackPill(
		int mask,
		PillStackPill pill,
		bool enabled) {
	return enabled
		? (mask | int(pill))
		: (mask & ~int(pill));
}

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

	[[nodiscard]] bool pillStackEnabled() const;
	void setPillStackEnabled(bool value);

	[[nodiscard]] int pillStackPills() const;
	void setPillStackPills(int value);

	[[nodiscard]] QString mainMenuOrder() const;
	void setMainMenuOrder(const QString &value);

	[[nodiscard]] bool drawerAccountsExpanded() const;
	void setDrawerAccountsExpanded(bool value);

	[[nodiscard]] ChatsSettings chatsSettings() const;
	void setChatsRemoveMessageTail(bool value);
	void setChatsHideReactionsChannels(bool value);
	void setChatsHideReactionsGroups(bool value);
	void setChatsHideReactionsPrivate(bool value);
	void setChatsStripTrackingOnOpen(bool value);

	[[nodiscard]] bool pluginEnabled(const QString &id) const;
	void setPluginEnabled(const QString &id, bool enabled);
	[[nodiscard]] QJsonObject pluginStates() const;

	[[nodiscard]] QJsonObject toJson() const;
	bool fromJson(const QJsonObject &root, QStringList *unknown = nullptr);

private:
	Config() = default;

	void ensureLoaded() const;
	void refreshChats() const;
	[[nodiscard]] bool boolValue(const char *key, bool fallback) const;
	[[nodiscard]] int intValue(const char *key, int fallback) const;
	[[nodiscard]] QString stringValue(
		const char *key,
		const QString &fallback) const;
	void setValue(const char *key, const QJsonValue &value);

	mutable QJsonObject _values;
	mutable QString _path;
	mutable bool _loaded = false;
	mutable ChatsSettings _chats;
};

} // namespace Exteraless
