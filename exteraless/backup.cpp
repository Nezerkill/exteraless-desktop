#include "exteraless/backup.h"

#include "exteraless/config.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace Exteraless {
namespace {

const auto kPrefix = QString(QChar(0x2001)) + QChar(0x2002);
const auto kSeparator = QChar(0x2000);
const auto kAlphabet = QString::fromUtf16(
	u"\u200a\u200b\u200c\u200f\u202f\u206a\u206b\u206c\u206d\u206e\u206f");
constexpr auto kVersion = 1;

constexpr auto kRelativeLastSeen = "relativeLastSeen";
constexpr auto kAddCommaAfterMention = "addCommaAfterMention";
constexpr auto kDisableNumberRounding = "disableNumberRounding";
constexpr auto kFormatTimeWithSeconds = "formatTimeWithSeconds";
constexpr auto kShowSeconds = "OEGeneralShowSeconds";
constexpr auto kShowIdAndDc = "showIdAndDc";
constexpr auto kShowIdDc = "OEGeneralShowIdDc";
constexpr auto kIdDcType = "idDcType";
constexpr auto kTranslucentDeleted = "OEChatsTranslucentDeleted";
constexpr auto kPanguOnSending = "OEChatsPanguOnSending";
constexpr auto kSilentByDefault = "OEChatsSilentByDefault";
constexpr auto kHidePremiumSection = "OESettingsHidePremium";
constexpr auto kHideHelpSection = "OESettingsHideHelp";
constexpr auto kHideAiEditor = "OEAppearanceHideAiEditor";
constexpr auto kHideSendAsPeer = "OEChatsHideSendAsPeer";
constexpr auto kHideGiftButton = "OEChatsHideGiftButton";
constexpr auto kHideCallButton = "OEChatsHideCallButton";
constexpr auto kHideChannelSearchButton = "OEChatsHideChannelSearchButton";
constexpr auto kInlineMathResult = "OEChatsInlineMathResult";
constexpr auto kSaveToChatSubfolder = "OEGeneralSaveToChatSubfolder";
constexpr auto kGhostScheduledSend = "OEGhostScheduledSend";
constexpr auto kSeekDuration = "OEChatsSeekDuration";
constexpr auto kUseEditedIcon = "OEChatsUseEditedIcon";
constexpr auto kCustomEditedMark = "OEChatsCustomEditedMark";
constexpr auto kShowForwardDate = "OEChatsShowForwardDate";
constexpr auto kChatsSenderMiniAvatars = "OEChatsSenderMiniAvatars";
constexpr auto kMediaViewerMenuForward = "OEMediaViewerMenuForward";
constexpr auto kMediaViewerMenuCopyFrame = "OEMediaViewerMenuCopyFrame";
constexpr auto kMediaViewerMenuCopyPhoto = "OEMediaViewerMenuCopyPhoto";
constexpr auto kMediaViewerMenuSetProfile = "OEMediaViewerMenuSetProfile";
constexpr auto kMediaViewerMenuSubtitles = "OEMediaViewerMenuSubtitles";
constexpr auto kSubtitlesFiles = "OESubtitlesFiles";
constexpr auto kLyricsFiles = "OELyricsFiles";
constexpr auto kLyricsFilesAlias = "lyricsFiles";
constexpr auto kChatMenuBoost = "OEChatsChatMenuBoost";
constexpr auto kChatMenuLinked = "OEChatsChatMenuLinked";
constexpr auto kChatMenuViewDeleted = "OEChatsChatMenuViewDeleted";
constexpr auto kChatMenuClearDeleted = "OEChatsChatMenuClearDeleted";
constexpr auto kDisableLinkPreviewByDefault = "OEChatsDisableLinkPreviewByDefault";
constexpr auto kHideAllChatsFolder = "OEChatsHideAllChatsFolder";
constexpr auto kShowResultsBeforeVoting = "OEChatsShowResultsBeforeVoting";
constexpr auto kAlwaysShowDownloadIcon = "OEChatsAlwaysShowDownloadIcon";
constexpr auto kFalseBottom = "OEPrivacyFalseBottom";
constexpr auto kDecoration = "OEAppearanceChatDecoration";
constexpr auto kShortcutAdministrators = "OEChatsShortcutAdministrators";
constexpr auto kShortcutRecentActions = "OEChatsShortcutRecentActions";
constexpr auto kShortcutStatistics = "OEChatsShortcutStatistics";
constexpr auto kShortcutPermissions = "OEChatsShortcutPermissions";
constexpr auto kShortcutMembers = "OEChatsShortcutMembers";
constexpr auto kGroupedMessageMenu = "OEChatsGroupedMessageMenu";
constexpr auto kDefaultDeleteMenu = "OEChatsDefaultDeleteMenu";
constexpr auto kChatMenuBoostAlias = "chatMenuItemBoostGroup";
constexpr auto kChatMenuLinkedAlias = "chatMenuItemLinkedChat";
constexpr auto kChatMenuViewDeletedAlias = "chatMenuItemViewDeleted";
constexpr auto kChatMenuClearDeletedAlias = "chatMenuItemClearDeleted";
constexpr auto kMediaViewerMenuForwardAlias = "mediaViewerMenuItemForward";
constexpr auto kMediaViewerMenuCopyFrameAlias = "mediaViewerMenuItemCopyFrame";
constexpr auto kMediaViewerMenuCopyPhotoAlias = "mediaViewerMenuItemCopyPhoto";
constexpr auto kMediaViewerMenuSetProfileAlias
	= "mediaViewerMenuItemSetProfilePhoto";
constexpr auto kShowForwardDateAlias = "dateOfForwardedMsg";
constexpr auto kChatsSenderMiniAvatarsAlias = "senderMiniAvatars";
constexpr auto kDisableLinkPreviewByDefaultAlias
	= "disableLinkPreviewByDefault";
constexpr auto kDisableLinkPreviewByDefaultAliasUpper
	= "DisableLinkPreviewByDefault";
constexpr auto kHideAllChatsFolderAlias = "hideAllTab";
constexpr auto kHideAllChatsFolderAliasUpper = "HideAllTab";
constexpr auto kHideAllChatsFolderAliasChats = "hideAllChats";
constexpr auto kHideArchiveAlias = "hideArchive";
constexpr auto kHideArchiveAliasUpper = "HideArchive";
constexpr auto kHideArchiveFolderAlias = "hideArchiveFolder";
constexpr auto kSortByUnreadAlias = "sortByUnread";
constexpr auto kSortByUnreadAliasUpper = "SortByUnread";
constexpr auto kWideChannelPostsAlias = "wideChannelPosts";
constexpr auto kWideChannelPostsAliasUpper = "WideChannelPosts";
constexpr auto kShowResultsBeforeVotingAlias = "showResultsBeforeVoting";
constexpr auto kAlwaysShowDownloadIconAlias = "alwaysShowDownloadIcon";
constexpr auto kAlwaysShowDownloadIconAliasUpper = "AlwaysShowDownloadIcon";
constexpr auto kFalseBottomAlias = "falseBottom";
constexpr auto kFalseBottomAliasUpper = "FalseBottom";
constexpr auto kActionBarDecorationAlias = "actionBarDecoration";
constexpr auto kChatDecorationAlias = "chatDecoration";
constexpr auto kShortcutAdministratorsAlias = "ChannelAdministrators";
constexpr auto kShortcutRecentActionsAlias = "EventLog";
constexpr auto kShortcutStatisticsAlias = "Statistics";
constexpr auto kShortcutPermissionsAlias = "ChannelPermissions";
constexpr auto kShortcutMembersAlias = "GroupMembers";
constexpr auto kGroupedMessageMenuAlias = "groupedMessageMenu";
constexpr auto kGroupedMessageMenuAliasBackup = "groupMessageMenu";
constexpr auto kDefaultDeleteMenuAlias = "DefaultDeleteMenu";
constexpr auto kUseEditedIconAlias = "useEditedIcon";
constexpr auto kCustomEditedMarkAlias = "customEditedMessage";
constexpr auto kSeekDurationAlias = "doubleTapSeekDuration";
constexpr auto kGhostScheduledSendAlias = "GhostScheduledSend";
constexpr auto kSaveToChatSubfolderAlias = "saveToChatSubfolder";
constexpr auto kInlineMathAlias = "inlineMathResult";
constexpr auto kHideSendAsAlias = "hideSendAsChannel";
constexpr auto kHideGiftButtonAlias = "hideGiftButton";
constexpr auto kHideGiftButtonInChannelAlias = "hideGiftButtonInChannel";
constexpr auto kHideCallButtonAlias = "hideCallButton";
constexpr auto kHideChannelSearchButtonAlias = "hideChannelSearchButton";
constexpr auto kHideAiEditorAlias = "hideAiEditor";
constexpr auto kHidePremiumAlias = "hidePremiumSection";
constexpr auto kHideHelpAlias = "hideHelpSection";
constexpr auto kSilentByDefaultAlias = "silentMessageByDefault";
constexpr auto kPanguAlias = "EnablePanguOnSending";
constexpr auto kUseDeletedIcon = "OEChatsUseDeletedIcon";
constexpr auto kCustomDeletedMark = "OEChatsCustomDeletedMark";
constexpr auto kDeletedMarkColor = "OEChatsDeletedMarkColor";
constexpr auto kTranslucentDeletedAlias = "TranslucentDeletedMessages";
constexpr auto kUseDeletedIconAlias = "UseDeletedIcon";
constexpr auto kCustomDeletedMarkAlias = "CustomDeletedMark";
constexpr auto kDeletedMarkColorAlias = "DeletedMarkColor";
constexpr auto kHidePhoneNumber = "hidePhoneNumber";
constexpr auto kHidePhone = "OEGeneralHidePhone";
constexpr auto kGeneralUsePersianCalendar = "OEGeneralUsePersianCalendar";
constexpr auto kGeneralPersianCalendarLatin = "OEGeneralPersianCalendarLatin";
constexpr auto kUsePersianCalendarAlias = "usePersianCalendar";
constexpr auto kPersianCalendarLatinAlias = "displayPersianCalendarByLatin";
constexpr auto kGeneralDefaultVideoQuality = "OEGeneralDefaultVideoQuality";
constexpr auto kGeneralHardwareAcceleration = "OEGeneralHardwareAcceleration";
constexpr auto kChatsTakeGifAsVideo = "OEChatsTakeGifAsVideo";
constexpr auto kGeneralCustomAudioBitrate = "OEGeneralCustomAudioBitrate";
constexpr auto kGeneralAutoUpdateChannel = "OEGeneralAutoUpdateChannel";
constexpr auto kGeneralBotPrivacyEnabled = "OEGeneralBotPrivacyEnabled";
constexpr auto kGeneralBotPrivacyAccount = "OEGeneralBotPrivacyAccount";
constexpr auto kGeneralBotPrivacyAsk = "OEGeneralBotPrivacyAlwaysAsk";
constexpr auto kBotPrivacyEnabledAlias = "botPrivacyEnabled";
constexpr auto kBotPrivacyUserIdAlias = "botPrivacyUserId";
constexpr auto kBotPrivacyAlwaysAskAlias = "alwaysAsk";
constexpr auto kDefaultHlsVideoQualityAlias = "defaultHlsVideoQuality";
constexpr auto kPlayerDecoderAlias = "playerDecoder";
constexpr auto kTakeGifAsVideoAlias = "takeGIFasVideo";
constexpr auto kCustomAudioBitrateAlias = "customAudioBitrate";
constexpr auto kAutoUpdateChannelAlias = "autoUpdateChannel";
constexpr auto kDoNotUseProxy = "doNotUseProxy";
constexpr auto kRemoveMessageTail = "removeMessageTail";
constexpr auto kHideReactionsInChannels = "hideReactionsInChannels";
constexpr auto kHideReactionsInGroups = "hideReactionsInGroups";
constexpr auto kHideReactionsInPrivateChats = "hideReactionsInPrivateChats";
constexpr auto kGhostSendReadPackets = "OEGhostSendReadPackets";
constexpr auto kGhostSendReadStories = "OEGhostSendReadStories";
constexpr auto kGhostSendOnline = "OEGhostSendOnline";
constexpr auto kGhostSendUploadProgress = "OEGhostSendUploadProgress";
constexpr auto kGhostShowStatus = "OEGhostShowStatus";
constexpr auto kShowGhostModeStatus = "showGhostModeStatus";
constexpr auto kGhostSendReadLocked = "OEGhostSendReadLocked";
constexpr auto kGhostSendStoriesLocked = "OEGhostSendStoriesLocked";
constexpr auto kGhostSendOnlineLocked = "OEGhostSendOnlineLocked";
constexpr auto kGhostSendUploadProgressLocked
	= "OEGhostSendUploadProgressLocked";
constexpr auto kSendReadMessagePacketsLocked
	= "sendReadMessagePacketsLocked";
constexpr auto kSendReadStoriesPacketsLocked
	= "sendReadStoriesPacketsLocked";
constexpr auto kSendOnlinePacketsLocked = "sendOnlinePacketsLocked";
constexpr auto kSendUploadProgressLocked = "sendUploadProgressLocked";
constexpr auto kGhostReadExclusions = "OEGhostReadExclusions";
constexpr auto kGhostTypingExclusions = "OEGhostTypingExclusions";
constexpr auto kSaveDeletedMessages = "OESaveDeletedMessages";
constexpr auto kProxyAutoDisabled = "OEProxyAutoDisabled";
constexpr auto kNetworkUseIPv6 = "OENetworkUseIPv6";
constexpr auto kNetworkDnsType = "OENetworkDnsType";
constexpr auto kNetworkCustomDoH = "OENetworkCustomDoH";
constexpr auto kNetworkUseIPv6Alias = "useIPv6";
constexpr auto kNetworkDnsTypeAlias = "dnsType";
constexpr auto kNetworkCustomDoHAlias = "customDoH";
constexpr auto kPluginStates = "OEPluginStates";
constexpr auto kPluginsSettings = "OEPluginSettings";
constexpr auto kPluginsSettingsAlias = "pluginSettings";
constexpr auto kPillStackEnabled = "OEPillStackEnabled";
constexpr auto kPillStackPills = "OEPillStackPills";
constexpr auto kPillStackOrder = "OEPillStackOrder";
constexpr auto kMainMenuOrder = "OEMainMenuOrder";
constexpr auto kDrawerAccountsExpanded = "OEDrawerAccountsExpanded";
constexpr auto kChatsStripTrackingOnOpen = "OEChatsStripTrackingOnOpen";
constexpr auto kChatsStripTrackingOnPaste = "OEChatsStripTrackingOnPaste";
constexpr auto kChatsUnreadCapsule = "OEChatsUnreadCapsule";
constexpr auto kChatsMenuReply = "OEChatsMenuReply";
constexpr auto kChatsMenuEdit = "OEChatsMenuEdit";
constexpr auto kChatsMenuPin = "OEChatsMenuPin";
constexpr auto kChatsMenuCopyText = "OEChatsMenuCopyText";
constexpr auto kChatsMenuCopyLink = "OEChatsMenuCopyLink";
constexpr auto kChatsMenuCopyImage = "OEChatsMenuCopyImage";
constexpr auto kChatsMenuForward = "OEChatsMenuForward";
constexpr auto kChatsMenuSelect = "OEChatsMenuSelect";
constexpr auto kChatsMenuTranslate = "OEChatsMenuTranslate";
constexpr auto kChatsMenuReport = "OEChatsMenuReport";
constexpr auto kChatsMenuDelete = "OEChatsMenuDelete";
constexpr auto kChatsDoubleTapIn = "OEChatsDoubleTapIn";
constexpr auto kChatsDoubleTapOut = "OEChatsDoubleTapOut";
constexpr auto kChatsStickerTimeSide = "OEChatsStickerTimeSide";
constexpr auto kChatsStickerSize = "OEChatsStickerSize";
constexpr auto kChatsStickerShape = "OEChatsStickerShape";
constexpr auto kChatsAvatarRadius = "OEChatsAvatarRadius";
constexpr auto kChatsSectionRadius = "OEChatsSectionRadius";
constexpr auto kChatsSectionSeparateHeaders
	= "OEChatsSectionSeparateHeaders";
constexpr auto kChatsChatHeaderStyle = "OEChatsChatHeaderStyle";
constexpr auto kChatsReplyColors = "OEChatsReplyColors";
constexpr auto kChatsReplyEmoji = "OEChatsReplyEmoji";
constexpr auto kChatsReplyBackground = "OEChatsReplyBackground";
constexpr auto kChatsHideStickerTime = "OEChatsHideStickerTime";
constexpr auto kChatsHideStoriesHeader = "OEChatsHideStoriesHeader";
constexpr auto kChatsHideArchive = "OEChatsHideArchive";
constexpr auto kChatsSortByUnread = "OEChatsSortByUnread";
constexpr auto kChatsWideChannelPosts = "OEChatsWideChannelPosts";
constexpr auto kChatsZalgoFilter = "OEChatsZalgoFilter";
constexpr auto kChatsRegexFiltersEnabled = "OEChatsRegexFiltersEnabled";
constexpr auto kChatsRegexFiltersData = "OEChatsRegexFiltersData";
constexpr auto kChatsRegexFiltersInChats
	= "OEChatsRegexFiltersEnableInChats";
constexpr auto kChatsRegexFiltersShowHidden = "OEChatsRegexFiltersShowHidden";
constexpr auto kChatsRegexFiltersExcluded
	= "OEChatsRegexFiltersExcludedDialogs";
constexpr auto kRegexFiltersEnabledAlias = "regexFiltersEnabled";
constexpr auto kRegexFiltersDataAlias = "regexFiltersData";
constexpr auto kRegexFiltersInChatsAlias = "enableInChats";
constexpr auto kRegexFiltersExcludedAlias = "excludedDialogs";
constexpr auto kChatsShowSpoilersDirectly = "OEChatsShowSpoilersDirectly";
constexpr auto kChatsShowFormattingButtons = "OEChatsShowFormattingButtons";
constexpr auto kChatsHideGreetingSticker = "OEChatsHideGreetingSticker";
constexpr auto kChatsHidePanelsOnScroll = "OEChatsHidePanelsOnScroll";
constexpr auto kChatsChannelBottomButton = "OEChatsChannelBottomButton";
constexpr auto kChatsDownloadSpeedBoost = "OEChatsDownloadSpeedBoost";
constexpr auto kChatsUploadBoost = "OEChatsUploadBoost";
constexpr auto kChatsDisableAutoDownloadingArchive
	= "OEChatsDisableAutoDownloadingArchive";
constexpr auto kChatsBlockWin32ExecutableFiles
	= "OEChatsBlockWin32ExecutableFiles";
constexpr auto kDisableAutoDownloadingArchiveAlias
	= "disableAutoDownloadingArchive";
constexpr auto kBlockWin32ExecutableFilesAlias = "Win32ExecutableFiles";
constexpr auto kHideStickerTime = "hideStickerTime";
constexpr auto kHideStoriesFromHeader = "hideStoriesFromHeader";
constexpr auto kHideStoriesFromHeaderUpper = "HideStoriesFromHeader";
constexpr auto kZalgoFilter = "zalgoFilter";
constexpr auto kZalgoFilterUpper = "ZalgoFilter";
constexpr auto kShowSpoilersDirectly = "showSpoilersDirectly";
constexpr auto kShowFormattingButtons = "showFormattingButtons";
constexpr auto kStickerTimeMode = "stickerTimeMode";
constexpr auto kStickerSizeAlias = "stickerSize";
constexpr auto kStickerShapeAlias = "stickerShape";
constexpr auto kAvatarCornersAlias = "avatarCorners";
constexpr auto kSingleCornerRadiusAlias = "singleCornerRadius";
constexpr auto kAvatarSquarenessAlias = "avatarSquareness";
constexpr auto kSectionRadiusAlias = "sectionRadius";
constexpr auto kSectionRadiusAliasUpper = "SectionRadius";
constexpr auto kSectionSeparateHeadersAlias = "separateHeaders";
constexpr auto kSectionSeparateHeadersAliasUpper = "SeparateHeaders";
constexpr auto kIosChatHeaderAlias = "iosChatHeader";
constexpr auto kNewChatHeaderStyleAlias = "newChatHeaderStyle";
constexpr auto kReplyColorsAlias = "replyColors";
constexpr auto kReplyEmojiAlias = "replyEmoji";
constexpr auto kReplyBackgroundAlias = "replyBackground";
constexpr auto kMessageColoredBackground = "messageColoredBackground";
constexpr auto kMessageColoredBackgroundUpper = "MessageColoredBackground";
constexpr auto kDisableGreetingSticker = "disableGreetingSticker";
constexpr auto kHideKeyboardOnScroll = "hideKeyboardOnScroll";
constexpr auto kHideKeyboardOnChatScroll = "HideKeyboardOnChatScroll";
constexpr auto kBottomButton = "bottomButton";
constexpr auto kBottomButtonUpper = "BottomButton";
constexpr auto kDownloadSpeedBoostAlias = "downloadSpeedBoost";
constexpr auto kUploadBoostAlias = "uploadBoost";
constexpr auto kUploadBoostAliasUpper = "UploadBoost";
constexpr auto kDisableChannelMuteButton = "DisableChannelMuteButton";
constexpr auto kDoubleTapAction = "doubleTapAction";
constexpr auto kDoubleTapActionOut = "doubleTapActionOutOwner";

constexpr auto kTranslationProvider = "OETranslationProvider";
constexpr auto kTranslationGoogleCloudKey = "OETranslationGoogleCloudKey";
constexpr auto kTranslationDeepLKey = "OETranslationDeepLKey";
constexpr auto kTranslationDeepLFormality = "OETranslationDeepLFormality";
constexpr auto kTranslationProviderAlias = "translationProvider";
constexpr auto kTranslationGoogleCloudKeyAlias = "googleCloudTranslateKey";
constexpr auto kTranslationDeepLKeyAlias = "deepLTranslateKey";
constexpr auto kTranslationDeepLFormalityAlias = "deepLFormality";
constexpr auto kAiEndpoint = "OEAiEndpoint";
constexpr auto kAiModel = "OEAiModel";
constexpr auto kAiApiKey = "OEAiApiKey";
constexpr auto kAiEndpointAlias = "aiEndpoint";
constexpr auto kAiModelAlias = "aiModel";
constexpr auto kAiApiKeyAlias = "aiApiKey";
constexpr auto kGeneralRecognitionLanguage = "OEGeneralRecognitionLanguage";
constexpr auto kRecognitionLanguageAlias = "recognitionLanguage";

constexpr auto kShowCopyPhotoButton = "showCopyPhotoButton";
constexpr auto kShowReportButton = "showReportButton";

constexpr auto kSectionExtera = "exteraconfig";
constexpr auto kSectionPills = "pillstackconfig";
constexpr auto kSectionMain = "mainconfig";

[[nodiscard]] QString ToBase11(int value) {
	auto result = QString();
	while (value > 0) {
		result.prepend(kAlphabet[value % 11]);
		value /= 11;
	}
	return result;
}

[[nodiscard]] int FromBase11(const QString &text, bool *ok) {
	auto value = 0;
	for (const auto ch : text) {
		const auto digit = kAlphabet.indexOf(ch);
		if (digit < 0) {
			*ok = false;
			return 0;
		}
		value = value * 11 + digit;
	}
	*ok = true;
	return value;
}

[[nodiscard]] bool IntegralInRange(
		const QJsonValue &value,
		int minimum,
		int maximum) {
	if (!value.isDouble()) {
		return false;
	}
	const auto number = value.toDouble();
	if (number < minimum || number > maximum) {
		return false;
	}
	return (number == double(int(number)));
}

[[nodiscard]] bool ValidProxyConditions(const QJsonValue &value) {
	return IntegralInRange(value, 0, 7);
}

[[nodiscard]] bool ValidPillMask(const QJsonValue &value) {
	return IntegralInRange(value, 0, kPillStackAllPills);
}

[[nodiscard]] bool ValidMainMenuOrder(const QJsonValue &value) {
	if (!value.isString()) {
		return false;
	}
	const auto text = value.toString();
	if (text.isEmpty() || text.size() > 256) {
		return false;
	}
	for (const auto ch : text) {
		if ((ch < u'a' || ch > u'z') && ch != QChar(u',')) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] QJsonObject SanitizeFileEntries(const QJsonValue &value) {
	if (!value.isObject()) {
		return QJsonObject();
	}
	auto result = QJsonObject();
	const auto files = value.toObject();
	for (auto i = files.begin(); i != files.end(); ++i) {
		if (!i.value().isObject()) {
			continue;
		}
		const auto entry = i.value().toObject();
		const auto path = entry.value(QStringLiteral("path"));
		if (!path.isString() || path.toString().isEmpty()) {
			continue;
		}
		auto sanitized = QJsonObject();
		sanitized[QStringLiteral("path")] = path.toString();
		const auto label = entry.value(QStringLiteral("label"));
		sanitized[QStringLiteral("label")]
			= label.isString() ? label.toString() : QString();
		result.insert(i.key(), sanitized);
	}
	return result;
}

} // namespace

QString EncodeInvisible(const QByteArray &json) {
	auto result = QString(kPrefix);
	const auto bytes = json;
	for (auto i = 0; i != bytes.size(); ++i) {
		if (i > 0) {
			result += kSeparator;
		}
		result += ToBase11(static_cast<unsigned char>(bytes[i]));
	}
	return result;
}

QByteArray DecodeInvisible(const QString &text) {
	auto body = text;
	if (body.startsWith(kPrefix)) {
		body.remove(0, 2);
	} else {
		return text.toUtf8();
	}

	auto bytes = QByteArray();
	const auto parts = body.split(kSeparator);
	for (const auto &part : parts) {
		auto ok = false;
		const auto value = FromBase11(part, &ok);
		if (!ok || value < 0 || value > 255) {
			return {};
		}
		bytes.append(static_cast<char>(value));
	}
	return bytes;
}

QString BuildBackup() {
	auto &config = Config::Instance();
	const auto chats = config.chatsSettings();

	auto extera = QJsonObject();
	extera[kRelativeLastSeen] = config.relativeLastSeen();
	extera[kAddCommaAfterMention] = config.addCommaAfterMention();
	extera[kDisableNumberRounding] = config.disableNumberRounding();
	extera[kFormatTimeWithSeconds] = config.showSeconds();
	extera[kShowIdAndDc] = config.showIdDc();
	extera[kHidePhoneNumber] = config.hidePhone();
	extera[kGeneralUsePersianCalendar] = config.usePersianCalendar();
	extera[kGeneralPersianCalendarLatin] = config.displayPersianCalendarByLatin();
	extera[kGeneralDefaultVideoQuality] = config.defaultVideoQuality();
	extera[kGeneralHardwareAcceleration] = config.hardwareAcceleration();
	extera[kChatsTakeGifAsVideo] = config.takeGifAsVideo();
	extera[kGeneralCustomAudioBitrate] = config.customAudioBitrate();
	extera[kGeneralAutoUpdateChannel] = config.autoUpdateChannel();
	extera[kGeneralBotPrivacyEnabled] = config.botPrivacyEnabled();
	extera[kGeneralBotPrivacyAccount] = config.botPrivacyAccount();
	extera[kGeneralBotPrivacyAsk] = config.botPrivacyAlwaysAsk();
	extera[kDoNotUseProxy] = config.proxyDisableConditions();
	extera[kRemoveMessageTail] = chats.removeMessageTail;
	extera[kHideReactionsInChannels] = chats.hideReactionsChannels;
	extera[kHideReactionsInGroups] = chats.hideReactionsGroups;
	extera[kHideReactionsInPrivateChats] = chats.hideReactionsPrivate;
	extera[kGhostSendReadPackets] = config.ghostSendReadPackets();
	extera[kGhostSendReadStories] = config.ghostSendReadStories();
	extera[kGhostSendOnline] = config.ghostSendOnline();
	extera[kGhostSendUploadProgress] = config.ghostSendUploadProgress();
	extera[kGhostShowStatus] = config.ghostShowStatus();
	extera[kGhostSendReadLocked] = config.ghostSendReadLocked();
	extera[kGhostSendStoriesLocked] = config.ghostSendStoriesLocked();
	extera[kGhostSendOnlineLocked] = config.ghostSendOnlineLocked();
	extera[kGhostSendUploadProgressLocked]
		= config.ghostSendUploadProgressLocked();
	extera[kGhostReadExclusions] = config.ghostReadExclusions();
	extera[kGhostTypingExclusions] = config.ghostTypingExclusions();
	extera[kSaveDeletedMessages] = config.saveDeletedMessages();
	extera[kProxyAutoDisabled] = config.proxyAutoDisabled();
	extera[kNetworkUseIPv6] = config.networkUseIPv6();
	extera[kNetworkDnsType] = config.networkDnsType();
	extera[kNetworkCustomDoH] = config.networkCustomDoH();
	extera[kPluginStates] = config.pluginStates();
	extera[kChatsStripTrackingOnOpen] = chats.stripTrackingOnOpen;
	extera[kChatsStripTrackingOnPaste] = chats.stripTrackingOnPaste;
	extera[kChatsUnreadCapsule] = chats.unreadCapsule;
	extera[kTranslucentDeleted] = config.translucentDeletedMessages();
	extera[kPanguOnSending] = config.panguOnSending();
	extera[kSilentByDefault] = config.silentByDefault();
	extera[kHidePremiumSection] = config.hidePremiumSection();
	extera[kHideHelpSection] = config.hideHelpSection();
	extera[kHideAiEditor] = config.hideAiEditor();
	extera[kHideSendAsPeer] = config.hideSendAsPeer();
	extera[kHideGiftButton] = config.hideGiftButton();
	extera[kHideCallButton] = config.hideCallButton();
	extera[kHideChannelSearchButton] = config.hideChannelSearchButton();
	extera[kInlineMathResult] = config.inlineMathResult();
	extera[kSaveToChatSubfolder] = config.saveToChatSubfolder();
	extera[kGhostScheduledSend] = config.ghostScheduledSend();
	extera[kSeekDuration] = config.seekDuration();
	extera[kUseEditedIcon] = config.useEditedIcon();
	extera[kCustomEditedMark] = config.customEditedMark();
	extera[kShowForwardDate] = config.showForwardDate();
	extera[kChatsSenderMiniAvatars] = config.senderMiniAvatars();
	extera[kMediaViewerMenuForward] = config.mediaViewerMenuForward();
	extera[kMediaViewerMenuCopyFrame] = config.mediaViewerMenuCopyFrame();
	extera[kMediaViewerMenuCopyPhoto] = config.mediaViewerMenuCopyPhoto();
	extera[kMediaViewerMenuSetProfile] = config.mediaViewerMenuSetProfile();
	extera[kMediaViewerMenuSubtitles] = config.mediaViewerMenuSubtitles();
	extera[kSubtitlesFiles] = QJsonDocument::fromJson(
		config.subtitlesFiles().toUtf8()).object();
	extera[kLyricsFiles] = QJsonDocument::fromJson(
		config.lyricsFiles().toUtf8()).object();
	extera[kChatMenuBoost] = config.chatMenuBoost();
	extera[kChatMenuLinked] = config.chatMenuLinked();
	extera[kChatMenuViewDeleted] = config.chatMenuViewDeleted();
	extera[kChatMenuClearDeleted] = config.chatMenuClearDeleted();
	extera[kDisableLinkPreviewByDefault]
		= config.disableLinkPreviewByDefault();
	extera[kHideAllChatsFolder] = config.hideAllChatsFolder();
	extera[kShowResultsBeforeVoting] = config.showResultsBeforeVoting();
	extera[kAlwaysShowDownloadIcon] = config.alwaysShowDownloadIcon();
	extera[kFalseBottom] = config.falseBottom();
	extera[kDecoration] = config.decoration();
	extera[kShortcutAdministrators] = config.chatShortcutAdministrators();
	extera[kShortcutRecentActions] = config.chatShortcutRecentActions();
	extera[kShortcutStatistics] = config.chatShortcutStatistics();
	extera[kShortcutPermissions] = config.chatShortcutPermissions();
	extera[kShortcutMembers] = config.chatShortcutMembers();
	extera[kGroupedMessageMenu] = config.groupedMessageMenu();
	extera[kDefaultDeleteMenu] = config.defaultDeleteMenu();
	extera[kUseDeletedIcon] = config.useDeletedIcon();
	extera[kCustomDeletedMark] = config.customDeletedMark();
	extera[kDeletedMarkColor] = config.deletedMarkColor();
	const auto menu = config.messageMenuSettings();
	extera[kChatsMenuReply] = menu.reply;
	extera[kChatsMenuEdit] = menu.edit;
	extera[kChatsMenuPin] = menu.pin;
	extera[kChatsMenuCopyText] = menu.copyText;
	extera[kChatsMenuCopyLink] = menu.copyLink;
	extera[kChatsMenuCopyImage] = menu.copyImage;
	extera[kChatsMenuForward] = menu.forward;
	extera[kChatsMenuSelect] = menu.select;
	extera[kChatsMenuTranslate] = menu.translate;
	extera[kChatsMenuReport] = menu.report;
	extera[kChatsMenuDelete] = menu.deleteMsg;
	extera[kChatsDoubleTapIn] = config.chatsDoubleTapIn();
	extera[kChatsDoubleTapOut] = config.chatsDoubleTapOut();
	extera[kChatsStickerTimeSide] = chats.stickerTimeSide;
	extera[kChatsStickerSize] = chats.stickerSize;
	extera[kChatsStickerShape] = chats.stickerShape;
	extera[kChatsAvatarRadius] = chats.avatarRadius;
	extera[kChatsSectionRadius] = config.sectionRadius();
	extera[kChatsSectionSeparateHeaders] = config.sectionSeparateHeaders();
	extera[kChatsChatHeaderStyle] = config.chatHeaderStyle();
	extera[kChatsReplyColors] = chats.replyColors;
	extera[kChatsReplyEmoji] = chats.replyEmoji;
	extera[kChatsReplyBackground] = chats.replyBackground;
	extera[kChatsHideStickerTime] = chats.hideStickerTime;
	extera[kHideStoriesFromHeader] = chats.hideStoriesHeader;
	extera[kChatsHideArchive] = chats.hideArchive;
	extera[kChatsSortByUnread] = chats.sortByUnread;
	extera[kChatsWideChannelPosts] = chats.wideChannelPosts;
	extera[kChatsHideGreetingSticker] = chats.hideGreetingSticker;
	extera[kChatsHidePanelsOnScroll] = chats.hidePanelsOnScroll;
	extera[kChatsChannelBottomButton] = chats.channelBottomButton;
	extera[kChatsDownloadSpeedBoost] = chats.downloadSpeedBoost;
	extera[kChatsUploadBoost] = chats.uploadBoost;
	extera[kChatsDisableAutoDownloadingArchive]
		= chats.disableAutoDownloadingArchive;
	extera[kChatsBlockWin32ExecutableFiles] = chats.blockWin32Executables;
	extera[kZalgoFilter] = config.zalgoFilter();
	extera[kChatsRegexFiltersEnabled] = config.regexFiltersEnabled();
	extera[kChatsRegexFiltersData] = config.regexFiltersData();
	extera[kChatsRegexFiltersInChats] = config.regexFiltersInChats();
	extera[kChatsRegexFiltersShowHidden] = config.regexFiltersShowHidden();
	extera[kChatsRegexFiltersExcluded] = config.regexFiltersExcluded();
	extera[kShowSpoilersDirectly] = config.showSpoilersDirectly();
	extera[kShowFormattingButtons] = config.showFormattingButtons();
	extera[kTranslationProvider] = config.translationProvider();
	extera[kTranslationDeepLFormality] = config.translationDeepLFormality();
	extera[kAiEndpoint] = config.aiEndpoint();
	extera[kAiModel] = config.aiModel();
	extera[kGeneralRecognitionLanguage] = config.recognitionLanguage();

	auto pills = QJsonObject();
	pills[kPillStackEnabled] = config.pillStackEnabled();
	pills[kPillStackPills] = config.pillStackPills();
	pills[kPillStackOrder] = config.pillStackOrder();

	auto main = QJsonObject();
	main[kMainMenuOrder] = config.mainMenuOrder();
	main[kDrawerAccountsExpanded] = config.drawerAccountsExpanded();

	auto root = QJsonObject();
	root["version"] = kVersion;
	root[kSectionExtera] = extera;
	root[kSectionPills] = pills;
	root[kSectionMain] = main;

	return EncodeInvisible(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

BackupImportResult ApplyBackup(const QString &text) {
	auto result = BackupImportResult();
	const auto json = DecodeInvisible(text);
	if (json.isEmpty()) {
		result.error = "Cannot decode backup.";
		return result;
	}

	auto error = QJsonParseError{0, QJsonParseError::NoError};
	const auto document = QJsonDocument::fromJson(json, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		result.error = "Backup is not a JSON object.";
		return result;
	}

	const auto root = document.object();
	auto sections = std::vector<std::pair<QString, QJsonObject>>();
	const auto sectioned = root.contains(kSectionExtera)
		|| root.contains(kSectionPills)
		|| root.contains(kSectionMain);
	if (sectioned) {
		for (const auto name : { kSectionExtera, kSectionPills, kSectionMain }) {
			const auto value = root.value(name);
			if (value.isObject()) {
				sections.emplace_back(name, value.toObject());
			} else if (!value.isUndefined()) {
				result.unknown.push_back(name);
			}
		}
		for (auto i = root.begin(); i != root.end(); ++i) {
			const auto key = i.key();
			if (key != QStringLiteral("version")
				&& key != kSectionExtera
				&& key != kSectionPills
				&& key != kSectionMain) {
				result.unknown.push_back(key);
			}
		}
	} else {
		sections.emplace_back(kSectionExtera, root);
	}

	auto &config = Config::Instance();
	const auto apply = [&](
			const QString &section,
			const QString &key,
			const QJsonValue &value) {
		if (section == kSectionPills) {
			if (key == kPillStackEnabled && value.isBool()) {
				config.setPillStackEnabled(value.toBool());
				return true;
			} else if (key == kPillStackPills && ValidPillMask(value)) {
				config.setPillStackPills(value.toInt());
				return true;
			} else if (key == kPillStackOrder && value.isString()) {
				config.setPillStackOrder(value.toString());
				return true;
			}
			return false;
		} else if (section == kSectionMain) {
			if (key == kMainMenuOrder && ValidMainMenuOrder(value)) {
				config.setMainMenuOrder(value.toString());
				return true;
			} else if (key == kDrawerAccountsExpanded && value.isBool()) {
				config.setDrawerAccountsExpanded(value.toBool());
				return true;
			}
			return false;
		}
		if (key == kRelativeLastSeen && value.isBool()) {
			config.setRelativeLastSeen(value.toBool());
		} else if (key == kAddCommaAfterMention && value.isBool()) {
			config.setAddCommaAfterMention(value.toBool());
		} else if (key == kDisableNumberRounding && value.isBool()) {
			config.setDisableNumberRounding(value.toBool());
		} else if ((key == kFormatTimeWithSeconds || key == kShowSeconds || key == QStringLiteral("showSeconds"))
			&& value.isBool()) {
			config.setShowSeconds(value.toBool());
		} else if ((key == kHidePhoneNumber || key == kHidePhone || key == QStringLiteral("hidePhone"))
			&& value.isBool()) {
			config.setHidePhone(value.toBool());
		} else if ((key == kGeneralUsePersianCalendar
			|| key == kUsePersianCalendarAlias)
			&& value.isBool()) {
			config.setUsePersianCalendar(value.toBool());
		} else if ((key == kGeneralPersianCalendarLatin
			|| key == kPersianCalendarLatinAlias)
			&& value.isBool()) {
			config.setDisplayPersianCalendarByLatin(value.toBool());
		} else if ((key == kGeneralDefaultVideoQuality
			|| key == kDefaultHlsVideoQualityAlias)
			&& IntegralInRange(value, 0, 2)) {
			config.setDefaultVideoQuality(value.toInt());
		} else if ((key == kGeneralHardwareAcceleration
			|| key == kPlayerDecoderAlias)
			&& value.isBool()) {
			config.setHardwareAcceleration(value.toBool());
		} else if ((key == kChatsTakeGifAsVideo || key == kTakeGifAsVideoAlias)
			&& value.isBool()) {
			config.setTakeGifAsVideo(value.toBool());
		} else if ((key == kGeneralCustomAudioBitrate
			|| key == kCustomAudioBitrateAlias)
			&& value.isDouble()) {
			config.setCustomAudioBitrate(value.toInt());
		} else if ((key == kGeneralAutoUpdateChannel
			|| key == kAutoUpdateChannelAlias)
			&& IntegralInRange(value, 0, 2)) {
			config.setAutoUpdateChannel(value.toInt());
		} else if ((key == kGeneralBotPrivacyEnabled
			|| key == kBotPrivacyEnabledAlias)
			&& value.isBool()) {
			config.setBotPrivacyEnabled(value.toBool());
		} else if ((key == kGeneralBotPrivacyAccount
			|| key == kBotPrivacyUserIdAlias)
			&& value.isDouble()) {
			config.setBotPrivacyAccount(value.toInt());
		} else if ((key == kGeneralBotPrivacyAsk
			|| key == kBotPrivacyAlwaysAskAlias)
			&& value.isBool()) {
			config.setBotPrivacyAlwaysAsk(value.toBool());
		} else if ((key == kShowIdAndDc || key == kShowIdDc)
			&& value.isBool()) {
			config.setShowIdDc(value.toBool());
		} else if (key == kIdDcType && value.isDouble()) {
			config.setShowIdDc(value.toInt() != 0);
		} else if (key == kDoNotUseProxy && ValidProxyConditions(value)) {
			config.setProxyDisableConditions(value.toInt());
		} else if (key == kRemoveMessageTail && value.isBool()) {
			config.setChatsRemoveMessageTail(value.toBool());
		} else if (key == kHideReactionsInChannels && value.isBool()) {
			config.setChatsHideReactionsChannels(value.toBool());
		} else if (key == kHideReactionsInGroups && value.isBool()) {
			config.setChatsHideReactionsGroups(value.toBool());
		} else if (key == kHideReactionsInPrivateChats && value.isBool()) {
			config.setChatsHideReactionsPrivate(value.toBool());
		} else if (key == kGhostSendReadPackets && value.isBool()) {
			config.setGhostSendReadPackets(value.toBool());
		} else if (key == kGhostSendReadStories && value.isBool()) {
			config.setGhostSendReadStories(value.toBool());
		} else if (key == kGhostSendOnline && value.isBool()) {
			config.setGhostSendOnline(value.toBool());
		} else if (key == kGhostSendUploadProgress && value.isBool()) {
			config.setGhostSendUploadProgress(value.toBool());
		} else if ((key == kGhostShowStatus || key == kShowGhostModeStatus)
			&& value.isBool()) {
			config.setGhostShowStatus(value.toBool());
		} else if ((key == kGhostSendReadLocked
			|| key == kSendReadMessagePacketsLocked)
			&& value.isBool()) {
			config.setGhostSendReadLocked(value.toBool());
		} else if ((key == kGhostSendStoriesLocked
			|| key == kSendReadStoriesPacketsLocked)
			&& value.isBool()) {
			config.setGhostSendStoriesLocked(value.toBool());
		} else if ((key == kGhostSendOnlineLocked
			|| key == kSendOnlinePacketsLocked)
			&& value.isBool()) {
			config.setGhostSendOnlineLocked(value.toBool());
		} else if ((key == kGhostSendUploadProgressLocked
			|| key == kSendUploadProgressLocked)
			&& value.isBool()) {
			config.setGhostSendUploadProgressLocked(value.toBool());
		} else if (key == kGhostReadExclusions && value.isObject()) {
			config.setGhostReadExclusions(value.toObject());
		} else if (key == kGhostTypingExclusions && value.isObject()) {
			config.setGhostTypingExclusions(value.toObject());
		} else if (key == kSaveDeletedMessages && value.isBool()) {
			config.setSaveDeletedMessages(value.toBool());
		} else if (key == kProxyAutoDisabled && value.isBool()) {
			// Derived from the current network; a foreign backup must not
			// re-enable the proxy, so import only resets it.
			config.setProxyAutoDisabled(false);
		} else if ((key == kNetworkUseIPv6 || key == kNetworkUseIPv6Alias)
			&& value.isBool()) {
			config.setNetworkUseIPv6(value.toBool());
		} else if ((key == kNetworkDnsType || key == kNetworkDnsTypeAlias)
			&& value.isDouble()) {
			const auto rounded = int(std::lround(value.toDouble()));
			config.setNetworkDnsType(std::clamp(rounded, 0, 1));
		} else if ((key == kNetworkCustomDoH || key == kNetworkCustomDoHAlias)
			&& value.isString()) {
			config.setNetworkCustomDoH(value.toString());
		} else if (key == kPluginStates && value.isObject()) {
			config.setPluginStates(value.toObject());
		} else if ((key == kPluginsSettings || key == kPluginsSettingsAlias)
			&& (value.isString() || value.isObject())) {
			// WHY: plugin settings migrate from old backups but are never
			// exported again.
			config.setPluginsSettings(value.isString()
				? value.toString()
				: QString::fromUtf8(QJsonDocument(value.toObject())
					.toJson(QJsonDocument::Compact)));
		} else if (key == kChatsStripTrackingOnOpen && value.isBool()) {
			config.setChatsStripTrackingOnOpen(value.toBool());
		} else if (key == kChatsStripTrackingOnPaste && value.isBool()) {
			config.setChatsStripTrackingOnPaste(value.toBool());
		} else if (key == kChatsUnreadCapsule && value.isBool()) {
			config.setChatsUnreadCapsule(value.toBool());
		} else if ((key == kTranslucentDeleted
			|| key == kTranslucentDeletedAlias)
			&& value.isBool()) {
			config.setTranslucentDeletedMessages(value.toBool());
		} else if ((key == kPanguOnSending || key == kPanguAlias)
			&& value.isBool()) {
			config.setPanguOnSending(value.toBool());
		} else if ((key == kSilentByDefault || key == kSilentByDefaultAlias)
			&& value.isBool()) {
			config.setSilentByDefault(value.toBool());
		} else if ((key == kHidePremiumSection || key == kHidePremiumAlias)
			&& value.isBool()) {
			config.setHidePremiumSection(value.toBool());
		} else if ((key == kHideHelpSection || key == kHideHelpAlias)
			&& value.isBool()) {
			config.setHideHelpSection(value.toBool());
		} else if ((key == kHideAiEditor || key == kHideAiEditorAlias)
			&& value.isBool()) {
			config.setHideAiEditor(value.toBool());
		} else if ((key == kHideSendAsPeer || key == kHideSendAsAlias)
			&& value.isBool()) {
			config.setHideSendAsPeer(value.toBool());
		} else if ((key == kHideGiftButton
			|| key == kHideGiftButtonInChannelAlias
			|| key == kHideGiftButtonAlias)
			&& value.isBool()) {
			config.setHideGiftButton(value.toBool());
		} else if ((key == kHideCallButton || key == kHideCallButtonAlias)
			&& value.isBool()) {
			config.setHideCallButton(value.toBool());
		} else if ((key == kHideChannelSearchButton
			|| key == kHideChannelSearchButtonAlias)
			&& value.isBool()) {
			config.setHideChannelSearchButton(value.toBool());
		} else if ((key == kInlineMathResult || key == kInlineMathAlias)
			&& value.isBool()) {
			config.setInlineMathResult(value.toBool());
		} else if ((key == kSaveToChatSubfolder
			|| key == kSaveToChatSubfolderAlias)
			&& value.isBool()) {
			config.setSaveToChatSubfolder(value.toBool());
		} else if ((key == kGhostScheduledSend
			|| key == kGhostScheduledSendAlias)
			&& value.isBool()) {
			config.setGhostScheduledSend(value.toBool());
		} else if (key == kSeekDuration
			&& value.isDouble()
			&& (value.toInt() == 5
				|| value.toInt() == 10
				|| value.toInt() == 15
				|| value.toInt() == 30)) {
			config.setSeekDuration(value.toInt());
		} else if ((key == kUseEditedIcon || key == kUseEditedIconAlias)
			&& value.isBool()) {
			config.setUseEditedIcon(value.toBool());
		} else if ((key == kCustomEditedMark || key == kCustomEditedMarkAlias)
			&& value.isString()) {
			config.setCustomEditedMark(value.toString());
		} else if ((key == kShowForwardDate || key == kShowForwardDateAlias)
			&& value.isBool()) {
			config.setShowForwardDate(value.toBool());
		} else if ((key == kChatsSenderMiniAvatars
			|| key == kChatsSenderMiniAvatarsAlias)
			&& value.isBool()) {
			config.setSenderMiniAvatars(value.toBool());
		} else if ((key == kMediaViewerMenuForward
			|| key == kMediaViewerMenuForwardAlias)
			&& value.isBool()) {
			config.setMediaViewerMenuForward(value.toBool());
		} else if ((key == kMediaViewerMenuCopyFrame
			|| key == kMediaViewerMenuCopyFrameAlias)
			&& value.isBool()) {
			config.setMediaViewerMenuCopyFrame(value.toBool());
		} else if ((key == kMediaViewerMenuCopyPhoto
			|| key == kMediaViewerMenuCopyPhotoAlias)
			&& value.isBool()) {
			config.setMediaViewerMenuCopyPhoto(value.toBool());
		} else if ((key == kMediaViewerMenuSetProfile
			|| key == kMediaViewerMenuSetProfileAlias)
			&& value.isBool()) {
			config.setMediaViewerMenuSetProfile(value.toBool());
		} else if (key == kMediaViewerMenuSubtitles && value.isBool()) {
			config.setMediaViewerMenuSubtitles(value.toBool());
		} else if (key == kSubtitlesFiles && value.isObject()) {
			config.setSubtitlesFiles(QString::fromUtf8(
				QJsonDocument(SanitizeFileEntries(value))
					.toJson(QJsonDocument::Compact)));
		} else if ((key == kLyricsFiles || key == kLyricsFilesAlias)
			&& value.isObject()) {
			config.setLyricsFiles(QString::fromUtf8(
				QJsonDocument(SanitizeFileEntries(value))
					.toJson(QJsonDocument::Compact)));
		} else if ((key == kChatMenuBoost || key == kChatMenuBoostAlias)
			&& value.isBool()) {
			config.setChatMenuBoost(value.toBool());
		} else if ((key == kChatMenuLinked || key == kChatMenuLinkedAlias)
			&& value.isBool()) {
			config.setChatMenuLinked(value.toBool());
		} else if ((key == kChatMenuViewDeleted
			|| key == kChatMenuViewDeletedAlias)
			&& value.isBool()) {
			config.setChatMenuViewDeleted(value.toBool());
		} else if ((key == kChatMenuClearDeleted
			|| key == kChatMenuClearDeletedAlias)
			&& value.isBool()) {
			config.setChatMenuClearDeleted(value.toBool());
		} else if ((key == kDisableLinkPreviewByDefault
			|| key == kDisableLinkPreviewByDefaultAlias
			|| key == kDisableLinkPreviewByDefaultAliasUpper)
			&& value.isBool()) {
			config.setDisableLinkPreviewByDefault(value.toBool());
		} else if ((key == kHideAllChatsFolder
			|| key == kHideAllChatsFolderAlias
			|| key == kHideAllChatsFolderAliasUpper
			|| key == kHideAllChatsFolderAliasChats)
			&& value.isBool()) {
			config.setHideAllChatsFolder(value.toBool());
		} else if ((key == kShowResultsBeforeVoting
			|| key == kShowResultsBeforeVotingAlias)
			&& value.isBool()) {
			config.setShowResultsBeforeVoting(value.toBool());
		} else if ((key == kAlwaysShowDownloadIcon
			|| key == kAlwaysShowDownloadIconAlias
			|| key == kAlwaysShowDownloadIconAliasUpper)
			&& value.isBool()) {
			config.setAlwaysShowDownloadIcon(value.toBool());
		} else if ((key == kFalseBottom
			|| key == kFalseBottomAlias
			|| key == kFalseBottomAliasUpper)
			&& value.isBool()) {
			config.setFalseBottom(value.toBool());
		} else if (key == kActionBarDecorationAlias
			&& value.isDouble()
			&& value.toInt() >= 0
			&& value.toInt() <= 3) {
			const auto map = std::array{ 3, 1, 2, 0 };
			config.setDecoration(map[value.toInt()]);
		} else if (key == kChatDecorationAlias
			&& value.isDouble()
			&& value.toInt() >= 0
			&& value.toInt() <= 2) {
			const auto map = std::array{ 3, 1, 0 };
			config.setDecoration(map[value.toInt()]);
		} else if (key == kDecoration
			&& value.isDouble()
			&& value.toInt() >= 0
			&& value.toInt() <= 3) {
			config.setDecoration(value.toInt());
		} else if ((key == kShortcutAdministrators
			|| key == kShortcutAdministratorsAlias)
			&& value.isBool()) {
			config.setChatShortcutAdministrators(value.toBool());
		} else if ((key == kShortcutRecentActions
			|| key == kShortcutRecentActionsAlias)
			&& value.isBool()) {
			config.setChatShortcutRecentActions(value.toBool());
		} else if ((key == kShortcutStatistics
			|| key == kShortcutStatisticsAlias)
			&& value.isBool()) {
			config.setChatShortcutStatistics(value.toBool());
		} else if ((key == kShortcutPermissions
			|| key == kShortcutPermissionsAlias)
			&& value.isBool()) {
			config.setChatShortcutPermissions(value.toBool());
		} else if ((key == kShortcutMembers
			|| key == kShortcutMembersAlias)
			&& value.isBool()) {
			config.setChatShortcutMembers(value.toBool());
		} else if ((key == kGroupedMessageMenu
			|| key == kGroupedMessageMenuAlias
			|| key == kGroupedMessageMenuAliasBackup)
			&& value.isBool()) {
			config.setGroupedMessageMenu(value.toBool());
		} else if ((key == kDefaultDeleteMenu || key == kDefaultDeleteMenuAlias)
			&& value.isBool()) {
			config.setDefaultDeleteMenu(value.toBool());
		} else if (key == kDefaultDeleteMenuAlias
			&& value.isDouble()
			&& value.toInt() >= 0) {
			const auto mask = value.toInt();
			config.setDefaultDeleteMenu((mask & 2) != 0);
		} else if (key == kSeekDurationAlias
			&& value.isDouble()
			&& value.toInt() >= 0
			&& value.toInt() <= 3) {
			const auto durations = std::array{ 5, 10, 15, 30 };
			config.setSeekDuration(durations[value.toInt()]);
		} else if ((key == kUseDeletedIcon || key == kUseDeletedIconAlias)
			&& value.isBool()) {
			config.setUseDeletedIcon(value.toBool());
		} else if ((key == kCustomDeletedMark
			|| key == kCustomDeletedMarkAlias)
			&& value.isString()) {
			config.setCustomDeletedMark(value.toString());
		} else if ((key == kDeletedMarkColor || key == kDeletedMarkColorAlias)
			&& value.isDouble()
			&& value.toInt() >= 0) {
			config.setDeletedMarkColor(value.toInt());
		} else if (key == kChatsMenuReply && value.isBool()) {
			config.setChatsMenuReply(value.toBool());
		} else if (key == kChatsMenuEdit && value.isBool()) {
			config.setChatsMenuEdit(value.toBool());
		} else if (key == kChatsMenuPin && value.isBool()) {
			config.setChatsMenuPin(value.toBool());
		} else if (key == kChatsMenuCopyText && value.isBool()) {
			config.setChatsMenuCopyText(value.toBool());
		} else if (key == kChatsMenuCopyLink && value.isBool()) {
			config.setChatsMenuCopyLink(value.toBool());
		} else if ((key == kChatsMenuCopyImage || key == kShowCopyPhotoButton)
			&& value.isBool()) {
			config.setChatsMenuCopyImage(value.toBool());
		} else if (key == kChatsMenuForward && value.isBool()) {
			config.setChatsMenuForward(value.toBool());
		} else if (key == kChatsMenuSelect && value.isBool()) {
			config.setChatsMenuSelect(value.toBool());
		} else if (key == kChatsMenuTranslate && value.isBool()) {
			config.setChatsMenuTranslate(value.toBool());
		} else if ((key == kChatsMenuReport || key == kShowReportButton)
			&& value.isBool()) {
			config.setChatsMenuReport(value.toBool());
		} else if (key == kChatsMenuDelete && value.isBool()) {
			config.setChatsMenuDelete(value.toBool());
		} else if ((key == kChatsDoubleTapIn || key == kDoubleTapAction)
			&& value.isDouble()
			&& value.toInt() >= -1
			&& value.toInt() <= 12) {
			config.setChatsDoubleTapIn(value.toInt());
		} else if ((key == kChatsDoubleTapOut
			|| key == kDoubleTapActionOut
			|| key == QStringLiteral("doubleTapActionOut"))
			&& value.isDouble()
			&& value.toInt() >= -1
			&& value.toInt() <= 12) {
			config.setChatsDoubleTapOut(value.toInt());
		} else if (key == kChatsStickerTimeSide && value.isBool()) {
			config.setChatsStickerTimeSide(value.toBool());
		} else if (key == kChatsStickerSize && IntegralInRange(value, 4, 20)) {
			config.setStickerSize(value.toInt());
		} else if (key == kStickerSizeAlias && value.isDouble()) {
			const auto rounded = int(std::lround(value.toDouble()));
			config.setStickerSize(std::clamp(rounded, 4, 20));
		} else if (key == kChatsStickerShape && IntegralInRange(value, 0, 2)) {
			config.setStickerShape(value.toInt());
		} else if (key == kStickerShapeAlias && value.isDouble()) {
			const auto rounded = int(std::lround(value.toDouble()));
			config.setStickerShape(std::clamp(rounded, 0, 2));
		} else if (key == kChatsAvatarRadius && IntegralInRange(value, -1, 32)) {
			config.setAvatarRadius(value.toInt());
		} else if (key == kChatsSectionRadius
			&& IntegralInRange(value, -1, 16)) {
			config.setSectionRadius(value.toInt());
		} else if ((key == kSectionRadiusAlias
			|| key == kSectionRadiusAliasUpper)
			&& value.isDouble()) {
			const auto rounded = int(std::lround(value.toDouble()));
			config.setSectionRadius(std::clamp(rounded, -1, 16));
		} else if ((key == kChatsSectionSeparateHeaders
			|| key == kSectionSeparateHeadersAlias
			|| key == kSectionSeparateHeadersAliasUpper)
			&& value.isBool()) {
			config.setSectionSeparateHeaders(value.toBool());
		} else if ((key == kAvatarCornersAlias
			|| key == kSingleCornerRadiusAlias)
			&& value.isDouble()) {
			const auto rounded = int(std::lround(value.toDouble()));
			config.setAvatarRadius(std::clamp(rounded, -1, 32));
		} else if (key == kAvatarSquarenessAlias && value.isDouble()) {
			const auto squareness = std::clamp(value.toDouble(), 0., 1.);
			const auto rounded = int(std::lround(32. * (1. - squareness)));
			config.setAvatarRadius(std::clamp(rounded, 0, 32));
		} else if (key == kChatsChatHeaderStyle
			&& IntegralInRange(value, 0, 2)) {
			config.setChatHeaderStyle(value.toInt());
		} else if (key == kIosChatHeaderAlias && value.isBool()) {
			config.setChatHeaderStyle(value.toBool() ? 1 : 0);
		} else if (key == kNewChatHeaderStyleAlias
			&& IntegralInRange(value, 0, 2)) {
			config.setChatHeaderStyle(value.toInt());
		} else if (key == kNewChatHeaderStyleAlias && value.isBool()) {
			config.setChatHeaderStyle(value.toBool() ? 2 : 0);
		} else if ((key == kChatsReplyColors || key == kReplyColorsAlias)
			&& value.isBool()) {
			config.setChatsReplyColors(value.toBool());
		} else if ((key == kChatsReplyEmoji || key == kReplyEmojiAlias)
			&& value.isBool()) {
			config.setChatsReplyEmoji(value.toBool());
		} else if ((key == kChatsReplyBackground || key == kReplyBackgroundAlias)
			&& value.isBool()) {
			config.setChatsReplyBackground(value.toBool());
		} else if ((key == kMessageColoredBackground
			|| key == kMessageColoredBackgroundUpper)
			&& value.isBool()) {
			if (!value.toBool()) {
				config.setChatsReplyBackground(false);
			}
		} else if ((key == kChatsHideStickerTime || key == kHideStickerTime)
			&& value.isBool()) {
			config.setChatsHideStickerTime(value.toBool());
		} else if ((key == kChatsHideStoriesHeader
			|| key == kHideStoriesFromHeader
			|| key == kHideStoriesFromHeaderUpper)
			&& value.isBool()) {
			config.setChatsHideStoriesHeader(value.toBool());
		} else if ((key == kChatsZalgoFilter
			|| key == kZalgoFilter
			|| key == kZalgoFilterUpper)
			&& value.isBool()) {
			config.setZalgoFilter(value.toBool());
		} else if ((key == kChatsRegexFiltersEnabled
			|| key == kRegexFiltersEnabledAlias)
			&& value.isBool()) {
			config.setRegexFiltersEnabled(value.toBool());
		} else if ((key == kChatsRegexFiltersData
			|| key == kRegexFiltersDataAlias)
			&& value.isString()) {
			config.setRegexFiltersData(value.toString());
		} else if ((key == kChatsRegexFiltersInChats
			|| key == kRegexFiltersInChatsAlias)
			&& value.isBool()) {
			config.setRegexFiltersInChats(value.toBool());
		} else if (key == kChatsRegexFiltersShowHidden && value.isBool()) {
			config.setRegexFiltersShowHidden(value.toBool());
		} else if ((key == kChatsRegexFiltersExcluded
			|| key == kRegexFiltersExcludedAlias)
			&& value.isString()) {
			config.setRegexFiltersExcluded(value.toString());
		} else if ((key == kChatsShowSpoilersDirectly
			|| key == kShowSpoilersDirectly)
			&& value.isBool()) {
			config.setShowSpoilersDirectly(value.toBool());
		} else if ((key == kChatsShowFormattingButtons
			|| key == kShowFormattingButtons)
			&& value.isBool()) {
			config.setShowFormattingButtons(value.toBool());
		} else if ((key == kChatsHideArchive
			|| key == kHideArchiveAlias
			|| key == kHideArchiveAliasUpper
			|| key == kHideArchiveFolderAlias)
			&& value.isBool()) {
			config.setChatsHideArchive(value.toBool());
		} else if ((key == kChatsSortByUnread
			|| key == kSortByUnreadAlias
			|| key == kSortByUnreadAliasUpper)
			&& value.isBool()) {
			config.setChatsSortByUnread(value.toBool());
		} else if ((key == kChatsWideChannelPosts
			|| key == kWideChannelPostsAlias
			|| key == kWideChannelPostsAliasUpper)
			&& value.isBool()) {
			config.setChatsWideChannelPosts(value.toBool());
		} else if ((key == kChatsHideGreetingSticker
			|| key == kDisableGreetingSticker)
			&& value.isBool()) {
			config.setChatsHideGreetingSticker(value.toBool());
		} else if ((key == kChatsHidePanelsOnScroll
			|| key == kHideKeyboardOnScroll
			|| key == kHideKeyboardOnChatScroll)
			&& value.isBool()) {
			config.setChatsHidePanelsOnScroll(value.toBool());
		} else if ((key == kChatsChannelBottomButton
			|| key == kBottomButton
			|| key == kBottomButtonUpper)
			&& IntegralInRange(value, 0, 2)) {
			config.setChatsChannelBottomButton(value.toInt());
		} else if ((key == kChatsDownloadSpeedBoost
			|| key == QStringLiteral("OEGeneralDownloadSpeedBoost"))
			&& IntegralInRange(value, 0, 2)) {
			config.setChatsDownloadSpeedBoost(value.toInt());
		} else if (key == kDownloadSpeedBoostAlias && value.isDouble()) {
			const auto rounded = int(std::lround(value.toDouble()));
			config.setChatsDownloadSpeedBoost(std::clamp(rounded, 0, 2));
		} else if ((key == kChatsUploadBoost
			|| key == kUploadBoostAlias
			|| key == kUploadBoostAliasUpper)
			&& value.isBool()) {
			config.setChatsUploadBoost(value.toBool());
		} else if (key == kDisableChannelMuteButton && value.isBool()) {
			if (value.toBool()) {
				config.setChatsChannelBottomButton(0);
			}
		} else if ((key == kChatsDisableAutoDownloadingArchive
			|| key == kDisableAutoDownloadingArchiveAlias)
			&& value.isBool()) {
			config.setDisableAutoDownloadingArchive(value.toBool());
		} else if ((key == kChatsBlockWin32ExecutableFiles
			|| key == kBlockWin32ExecutableFilesAlias)
			&& value.isBool()) {
			config.setBlockWin32Executables(value.toBool());
		} else if (key == kStickerTimeMode
			&& value.isDouble()
			&& value.toInt() >= 0
			&& value.toInt() <= 2) {
			const auto mode = value.toInt();
			config.setChatsHideStickerTime(mode == 2);
			config.setChatsStickerTimeSide(mode == 1);
		} else if ((key == kTranslationProvider
			|| key == kTranslationProviderAlias)
			&& IntegralInRange(value, 0, 3)) {
			config.setTranslationProvider(value.toInt());
		} else if ((key == kTranslationDeepLFormality
			|| key == kTranslationDeepLFormalityAlias)
			&& IntegralInRange(value, 0, 2)) {
			config.setTranslationDeepLFormality(value.toInt());
		} else if ((key == kTranslationGoogleCloudKey
			|| key == kTranslationGoogleCloudKeyAlias)
			&& value.isString()) {
			// WHY: old backups carry the secret; import accepts, export omits.
			config.setTranslationGoogleCloudKey(value.toString());
		} else if ((key == kTranslationDeepLKey
			|| key == kTranslationDeepLKeyAlias)
			&& value.isString()) {
			config.setTranslationDeepLKey(value.toString());
		} else if ((key == kAiEndpoint || key == kAiEndpointAlias)
			&& value.isString()) {
			config.setAiEndpoint(value.toString());
		} else if ((key == kAiModel || key == kAiModelAlias)
			&& value.isString()) {
			config.setAiModel(value.toString());
		} else if ((key == kAiApiKey || key == kAiApiKeyAlias)
			&& value.isString()) {
			// WHY: old backups carry the secret; import accepts, export omits.
			config.setAiApiKey(value.toString());
		} else if ((key == kGeneralRecognitionLanguage
			|| key == kRecognitionLanguageAlias)
			&& value.isString()
			&& !value.toString().isEmpty()) {
			config.setRecognitionLanguage(value.toString());
		} else {
			return false;
		}
		return true;
	};

	for (const auto &[section, values] : sections) {
		for (auto i = values.begin(); i != values.end(); ++i) {
			if (i.key() == QStringLiteral("version")) {
				continue;
			}
			if (apply(section, i.key(), i.value())) {
				++result.applied;
			} else {
				result.unknown.push_back((section == kSectionExtera)
					? i.key()
					: (section + QChar(u'.') + i.key()));
			}
		}
	}

	result.ok = true;
	return result;
}

} // namespace Exteraless
