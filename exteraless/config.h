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
	Weather = 256,
	Btc = 512,
	Eth = 1024,
	Usd = 2048,
	Eur = 4096,
	Gold = 8192,
};

constexpr auto kPillStackAllPills = 0x3FFF;

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
	[[nodiscard]] bool showIdDc() const;
	void setShowIdDc(bool value);
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
	[[nodiscard]] bool hideGiftButton() const;
	void setHideGiftButton(bool value);
	[[nodiscard]] bool hideCallButton() const;
	void setHideCallButton(bool value);
	[[nodiscard]] bool hideChannelSearchButton() const;
	void setHideChannelSearchButton(bool value);
	[[nodiscard]] bool inlineMathResult() const;
	void setInlineMathResult(bool value);
	[[nodiscard]] bool saveToChatSubfolder() const;
	void setSaveToChatSubfolder(bool value);
	[[nodiscard]] bool ghostScheduledSend() const;
	void setGhostScheduledSend(bool value);
	[[nodiscard]] int seekDuration() const;
	void setSeekDuration(int value);
	[[nodiscard]] bool useEditedIcon() const;
	void setUseEditedIcon(bool value);
	[[nodiscard]] QString customEditedMark() const;
	void setCustomEditedMark(const QString &value);
	[[nodiscard]] bool showForwardDate() const;
	void setShowForwardDate(bool value);
	[[nodiscard]] bool senderMiniAvatars() const;
	void setSenderMiniAvatars(bool value);
	[[nodiscard]] bool mediaViewerMenuForward() const;
	void setMediaViewerMenuForward(bool value);
	[[nodiscard]] bool mediaViewerMenuCopyFrame() const;
	void setMediaViewerMenuCopyFrame(bool value);
	[[nodiscard]] bool mediaViewerMenuCopyPhoto() const;
	void setMediaViewerMenuCopyPhoto(bool value);
	[[nodiscard]] bool mediaViewerMenuSetProfile() const;
	void setMediaViewerMenuSetProfile(bool value);
	[[nodiscard]] bool mediaViewerMenuSubtitles() const;
	void setMediaViewerMenuSubtitles(bool value);
	[[nodiscard]] QString subtitlesFiles() const;
	void setSubtitlesFiles(const QString &value);
	[[nodiscard]] QString lyricsFiles() const;
	void setLyricsFiles(const QString &value);
	[[nodiscard]] bool lyricsAllowNetwork() const;
	void setLyricsAllowNetwork(bool value);
	[[nodiscard]] bool chatMenuBoost() const;
	void setChatMenuBoost(bool value);
	[[nodiscard]] bool chatMenuLinked() const;
	void setChatMenuLinked(bool value);
	[[nodiscard]] bool chatMenuViewDeleted() const;
	void setChatMenuViewDeleted(bool value);
	[[nodiscard]] bool chatMenuClearDeleted() const;
	void setChatMenuClearDeleted(bool value);
	[[nodiscard]] bool disableLinkPreviewByDefault() const;
	void setDisableLinkPreviewByDefault(bool value);
	[[nodiscard]] bool hideAllChatsFolder() const;
	void setHideAllChatsFolder(bool value);
	[[nodiscard]] bool showResultsBeforeVoting() const;
	void setShowResultsBeforeVoting(bool value);
	[[nodiscard]] bool alwaysShowDownloadIcon() const;
	void setAlwaysShowDownloadIcon(bool value);
	[[nodiscard]] bool falseBottom() const;
	void setFalseBottom(bool value);
	[[nodiscard]] int decoration() const;
	void setDecoration(int value);
	[[nodiscard]] bool chatShortcutAdministrators() const;
	void setChatShortcutAdministrators(bool value);
	[[nodiscard]] bool chatShortcutRecentActions() const;
	void setChatShortcutRecentActions(bool value);
	[[nodiscard]] bool chatShortcutStatistics() const;
	void setChatShortcutStatistics(bool value);
	[[nodiscard]] bool chatShortcutPermissions() const;
	void setChatShortcutPermissions(bool value);
	[[nodiscard]] bool chatShortcutMembers() const;
	void setChatShortcutMembers(bool value);
	[[nodiscard]] bool groupedMessageMenu() const;
	void setGroupedMessageMenu(bool value);
	[[nodiscard]] bool defaultDeleteMenu() const;
	void setDefaultDeleteMenu(bool value);
	[[nodiscard]] bool useDeletedIcon() const;
	void setUseDeletedIcon(bool value);
	[[nodiscard]] QString customDeletedMark() const;
	void setCustomDeletedMark(const QString &value);
	[[nodiscard]] int deletedMarkColor() const;
	void setDeletedMarkColor(int value);

	[[nodiscard]] bool hidePhone() const;
	void setHidePhone(bool value);

	[[nodiscard]] bool usePersianCalendar() const;
	void setUsePersianCalendar(bool value);
	[[nodiscard]] bool displayPersianCalendarByLatin() const;
	void setDisplayPersianCalendarByLatin(bool value);

	[[nodiscard]] int defaultVideoQuality() const;
	void setDefaultVideoQuality(int value);
	[[nodiscard]] int autoUpdateChannel() const;
	void setAutoUpdateChannel(int value);
	[[nodiscard]] bool hardwareAcceleration() const;
	void setHardwareAcceleration(bool value);
	[[nodiscard]] bool takeGifAsVideo() const;
	void setTakeGifAsVideo(bool value);
	[[nodiscard]] int customAudioBitrate() const;
	void setCustomAudioBitrate(int value);

	[[nodiscard]] bool botPrivacyEnabled() const;
	void setBotPrivacyEnabled(bool value);
	[[nodiscard]] int botPrivacyAccount() const;
	void setBotPrivacyAccount(int value);
	[[nodiscard]] bool botPrivacyAlwaysAsk() const;
	void setBotPrivacyAlwaysAsk(bool value);

	[[nodiscard]] int proxyDisableConditions() const;
	void setProxyDisableConditions(int value);

	[[nodiscard]] bool proxyAutoDisabled() const;
	void setProxyAutoDisabled(bool value);

	[[nodiscard]] bool networkUseIPv6() const;
	void setNetworkUseIPv6(bool value);
	[[nodiscard]] int networkDnsType() const;
	void setNetworkDnsType(int value);
	[[nodiscard]] QString networkCustomDoH() const;
	void setNetworkCustomDoH(const QString &value);

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
	[[nodiscard]] int stickerSize() const;
	void setStickerSize(int value);
	void setChatsStickerSize(int value);
	[[nodiscard]] int stickerShape() const;
	void setStickerShape(int value);
	[[nodiscard]] int avatarRadius() const;
	void setAvatarRadius(int value);
	[[nodiscard]] int sectionRadius() const;
	void setSectionRadius(int value);
	[[nodiscard]] bool sectionSeparateHeaders() const;
	void setSectionSeparateHeaders(bool value);
	[[nodiscard]] int chatHeaderStyle() const;
	void setChatHeaderStyle(int value);
	[[nodiscard]] bool replyColors() const;
	void setChatsReplyColors(bool value);
	[[nodiscard]] bool replyEmoji() const;
	void setChatsReplyEmoji(bool value);
	[[nodiscard]] bool replyBackground() const;
	void setChatsReplyBackground(bool value);
	void setChatsStickerTimeSide(bool value);
	void setChatsHideStickerTime(bool value);
	void setChatsHideStoriesHeader(bool value);
	void setChatsHideArchive(bool value);
	void setChatsSortByUnread(bool value);
	void setChatsWideChannelPosts(bool value);
	void setChatsHideGreetingSticker(bool value);
	void setChatsHidePanelsOnScroll(bool value);
	void setChatsChannelBottomButton(int value);
	void setChatsDownloadSpeedBoost(int value);
	void setChatsUploadBoost(bool value);
	[[nodiscard]] bool disableAutoDownloadingArchive() const;
	void setDisableAutoDownloadingArchive(bool value);
	[[nodiscard]] bool blockWin32Executables() const;
	void setBlockWin32Executables(bool value);
	[[nodiscard]] bool zalgoFilter() const;
	void setZalgoFilter(bool value);
	[[nodiscard]] bool regexFiltersEnabled() const;
	void setRegexFiltersEnabled(bool value);
	[[nodiscard]] QString regexFiltersData() const;
	void setRegexFiltersData(const QString &value);
	[[nodiscard]] bool regexFiltersInChats() const;
	void setRegexFiltersInChats(bool value);
	[[nodiscard]] bool regexFiltersShowHidden() const;
	void setRegexFiltersShowHidden(bool value);
	[[nodiscard]] QString regexFiltersExcluded() const;
	void setRegexFiltersExcluded(const QString &value);
	[[nodiscard]] bool showSpoilersDirectly() const;
	void setShowSpoilersDirectly(bool value);
	[[nodiscard]] bool showFormattingButtons() const;
	void setShowFormattingButtons(bool value);
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

	[[nodiscard]] int translationProvider() const;
	void setTranslationProvider(int value);
	[[nodiscard]] QString translationGoogleCloudKey() const;
	void setTranslationGoogleCloudKey(const QString &value);
	[[nodiscard]] QString translationDeepLKey() const;
	void setTranslationDeepLKey(const QString &value);
	[[nodiscard]] int translationDeepLFormality() const;
	void setTranslationDeepLFormality(int value);

	[[nodiscard]] QString aiEndpoint() const;
	void setAiEndpoint(const QString &value);
	[[nodiscard]] QString aiModel() const;
	void setAiModel(const QString &value);
	[[nodiscard]] QString aiApiKey() const;
	void setAiApiKey(const QString &value);

	[[nodiscard]] QString recognitionLanguage() const;
	void setRecognitionLanguage(const QString &value);

	[[nodiscard]] bool pluginEnabled(const QString &id) const;
	void setPluginEnabled(const QString &id, bool enabled);
	[[nodiscard]] QJsonObject pluginStates() const;
	void setPluginStates(const QJsonObject &states);
	[[nodiscard]] QString pluginsSettings() const;
	void setPluginsSettings(const QString &value);

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
