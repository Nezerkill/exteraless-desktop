#pragma once

#include "exteraless/chats.h"
#include "exteraless/message_menu.h"

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

	void save();
	void reset();
	void setWorkingDir(const QString &workingDir);

	[[nodiscard]] bool relativeLastSeen() const;
	void setRelativeLastSeen(bool value);

	[[nodiscard]] bool addCommaAfterMention() const;
	void setAddCommaAfterMention(bool value);

	[[nodiscard]] bool disableNumberRounding() const;
	void setDisableNumberRounding(bool value);

	[[nodiscard]] bool showSeconds() const;
	void setShowSeconds(bool value);
	[[nodiscard]] bool translucentDeletedMessages() const;
	void setTranslucentDeletedMessages(bool value);
	[[nodiscard]] bool panguOnSending() const;
	void setPanguOnSending(bool value);
	[[nodiscard]] bool silentByDefault() const;
	void setSilentByDefault(bool value);
	[[nodiscard]] bool hidePremiumSection() const;
	void setHidePremiumSection(bool value);
	[[nodiscard]] bool hideHelpSection() const;
	void setHideHelpSection(bool value);
	[[nodiscard]] bool hideAiEditor() const;
	void setHideAiEditor(bool value);
	[[nodiscard]] bool hideSendAsPeer() const;
	void setHideSendAsPeer(bool value);
	[[nodiscard]] bool inlineMathResult() const;
	void setInlineMathResult(bool value);
	[[nodiscard]] bool saveToChatSubfolder() const;
	void setSaveToChatSubfolder(bool value);
	[[nodiscard]] bool useDeletedIcon() const;
	void setUseDeletedIcon(bool value);
	[[nodiscard]] QString customDeletedMark() const;
	void setCustomDeletedMark(const QString &value);
	[[nodiscard]] int deletedMarkColor() const;
	void setDeletedMarkColor(int value);

	[[nodiscard]] bool hidePhone() const;
	void setHidePhone(bool value);

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
	[[nodiscard]] bool ghostShowStatus() const;
	void setGhostShowStatus(bool value);
	[[nodiscard]] bool ghostSendReadLocked() const;
	void setGhostSendReadLocked(bool value);
	[[nodiscard]] bool ghostSendStoriesLocked() const;
	void setGhostSendStoriesLocked(bool value);
	[[nodiscard]] bool ghostSendOnlineLocked() const;
	void setGhostSendOnlineLocked(bool value);
	[[nodiscard]] bool ghostSendUploadProgressLocked() const;
	void setGhostSendUploadProgressLocked(bool value);

	[[nodiscard]] bool ghostReadExcluded(qint64 peerId) const;
	void setGhostReadExcluded(qint64 peerId, bool value);
	[[nodiscard]] QJsonObject ghostReadExclusions() const;
	void setGhostReadExclusions(const QJsonObject &value);
	[[nodiscard]] bool ghostTypingExcluded(qint64 peerId) const;
	void setGhostTypingExcluded(qint64 peerId, bool value);
	[[nodiscard]] QJsonObject ghostTypingExclusions() const;
	void setGhostTypingExclusions(const QJsonObject &value);

	[[nodiscard]] bool saveDeletedMessages() const;
	void setSaveDeletedMessages(bool value);

	[[nodiscard]] bool pillStackEnabled() const;
	void setPillStackEnabled(bool value);

	[[nodiscard]] int pillStackPills() const;
	void setPillStackPills(int value);
	[[nodiscard]] QString pillStackOrder() const;
	void setPillStackOrder(const QString &value);

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
	void setChatsStripTrackingOnPaste(bool value);
	void setChatsUnreadCapsule(bool value);

	[[nodiscard]] MessageMenuSettings messageMenuSettings() const;
	[[nodiscard]] int chatsDoubleTapIn() const;
	void setChatsDoubleTapIn(int value);
	[[nodiscard]] int chatsDoubleTapOut() const;
	void setChatsDoubleTapOut(int value);
	void setChatsStickerTimeSide(bool value);
	void setChatsHideStickerTime(bool value);
	void setChatsMenuReply(bool value);
	void setChatsMenuEdit(bool value);
	void setChatsMenuPin(bool value);
	void setChatsMenuCopyText(bool value);
	void setChatsMenuCopyLink(bool value);
	void setChatsMenuCopyImage(bool value);
	void setChatsMenuForward(bool value);
	void setChatsMenuSelect(bool value);
	void setChatsMenuTranslate(bool value);
	void setChatsMenuReport(bool value);
	void setChatsMenuDelete(bool value);

	[[nodiscard]] bool pluginEnabled(const QString &id) const;
	void setPluginEnabled(const QString &id, bool enabled);
	[[nodiscard]] QJsonObject pluginStates() const;
	void setPluginStates(const QJsonObject &states);

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
	mutable QString _workingDir;
	mutable bool _loaded = false;
	mutable ChatsSettings _chats;
	mutable MessageMenuSettings _messageMenu;
};

} // namespace Exteraless
