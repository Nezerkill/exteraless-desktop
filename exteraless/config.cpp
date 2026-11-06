#include "exteraless/config.h"

#include "exteraless/doubletap.h"

#include <QDir>
#include <algorithm>
#include <array>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace Exteraless {
namespace {

constexpr auto kVersion = 1;
constexpr auto kRelativeLastSeen = "OERelativeLastSeen";
constexpr auto kAddCommaAfterMention = "OEAddCommaAfterMention";
constexpr auto kDisableNumberRounding = "OEGeneralDisableNumberRounding";
constexpr auto kShowSeconds = "OEGeneralShowSeconds";
constexpr auto kShowIdDc = "OEGeneralShowIdDc";
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

constexpr auto kMaxEditedMarkLength = 16;

[[nodiscard]] int SanitizeSeekDuration(int value) {
	switch (value) {
	case 5:
	case 10:
	case 15:
	case 30: return value;
	}
	return 5;
}
constexpr auto kUseDeletedIcon = "OEChatsUseDeletedIcon";
constexpr auto kCustomDeletedMark = "OEChatsCustomDeletedMark";
constexpr auto kDeletedMarkColor = "OEChatsDeletedMarkColor";
constexpr auto kMaxDeletedMarkLength = 32;
constexpr auto kHidePhone = "OEGeneralHidePhone";
constexpr auto kGeneralUsePersianCalendar = "OEGeneralUsePersianCalendar";
constexpr auto kGeneralPersianCalendarLatin = "OEGeneralPersianCalendarLatin";
constexpr auto kGeneralBotPrivacyEnabled = "OEGeneralBotPrivacyEnabled";
constexpr auto kGeneralBotPrivacyAccount = "OEGeneralBotPrivacyAccount";
constexpr auto kGeneralBotPrivacyAsk = "OEGeneralBotPrivacyAlwaysAsk";
constexpr auto kProxyDisableConditions = "OEProxyDisableConditions";
constexpr auto kProxyAutoDisabled = "OEProxyAutoDisabled";
constexpr auto kGhostSendReadPackets = "OEGhostSendReadPackets";
constexpr auto kGhostSendReadStories = "OEGhostSendReadStories";
constexpr auto kGhostSendOnline = "OEGhostSendOnline";
constexpr auto kGhostSendUploadProgress = "OEGhostSendUploadProgress";
constexpr auto kGhostShowStatus = "OEGhostShowStatus";
constexpr auto kGhostSendReadLocked = "OEGhostSendReadLocked";
constexpr auto kGhostSendStoriesLocked = "OEGhostSendStoriesLocked";
constexpr auto kGhostSendOnlineLocked = "OEGhostSendOnlineLocked";
constexpr auto kGhostSendUploadProgressLocked
	= "OEGhostSendUploadProgressLocked";
constexpr auto kGhostReadExclusions = "OEGhostReadExclusions";
constexpr auto kGhostTypingExclusions = "OEGhostTypingExclusions";
constexpr auto kSaveDeletedMessages = "OESaveDeletedMessages";
constexpr auto kPillStackEnabled = "OEPillStackEnabled";
constexpr auto kPillStackPills = "OEPillStackPills";
constexpr auto kPillStackOrder = "OEPillStackOrder";
constexpr auto kMainMenuOrder = "OEMainMenuOrder";
constexpr auto kDrawerAccountsExpanded = "OEDrawerAccountsExpanded";
constexpr auto kPluginStates = "OEPluginStates";
constexpr auto kChatsRemoveMessageTail = "OEChatsRemoveMessageTail";
constexpr auto kChatsHideReactionsChannels = "OEChatsHideReactionsChannels";
constexpr auto kChatsHideReactionsGroups = "OEChatsHideReactionsGroups";
constexpr auto kChatsHideReactionsPrivate = "OEChatsHideReactionsPrivate";
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
constexpr auto kChatsHideGreetingSticker = "OEChatsHideGreetingSticker";
constexpr auto kChatsHidePanelsOnScroll = "OEChatsHidePanelsOnScroll";
constexpr auto kChatsChannelBottomButton = "OEChatsChannelBottomButton";
constexpr auto kChatsDownloadSpeedBoost = "OEChatsDownloadSpeedBoost";
constexpr auto kChatsUploadBoost = "OEChatsUploadBoost";
constexpr auto kChatsZalgoFilter = "OEChatsZalgoFilter";
constexpr auto kChatsShowSpoilersDirectly = "OEChatsShowSpoilersDirectly";
constexpr auto kChatsShowFormattingButtons = "OEChatsShowFormattingButtons";
constexpr auto kChatsDisableAutoDownloadingArchive
	= "OEChatsDisableAutoDownloadingArchive";
constexpr auto kChatsBlockWin32ExecutableFiles
	= "OEChatsBlockWin32ExecutableFiles";
constexpr auto kGeneralDefaultVideoQuality = "OEGeneralDefaultVideoQuality";
constexpr auto kGeneralHardwareAcceleration = "OEGeneralHardwareAcceleration";
constexpr auto kChatsTakeGifAsVideo = "OEChatsTakeGifAsVideo";
constexpr auto kGeneralCustomAudioBitrate = "OEGeneralCustomAudioBitrate";
constexpr auto kGeneralAutoUpdateChannel = "OEGeneralAutoUpdateChannel";
constexpr auto kTranslationProvider = "OETranslationProvider";
constexpr auto kTranslationGoogleCloudKey = "OETranslationGoogleCloudKey";
constexpr auto kTranslationDeepLKey = "OETranslationDeepLKey";
constexpr auto kTranslationDeepLFormality = "OETranslationDeepLFormality";
constexpr auto kAiEndpoint = "OEAiEndpoint";
constexpr auto kAiModel = "OEAiModel";
constexpr auto kAiApiKey = "OEAiApiKey";

[[nodiscard]] int SanitizeAudioBitrate(int value) {
	switch (value) {
	case 16:
	case 24:
	case 48: return value;
	}
	return 32;
}

const auto kDefaultMainMenuOrder = QStringLiteral(
	"exteraless,ghost,pillstack,proxy,storage");

const auto kDefaultPillStackOrder = QStringLiteral(
	"cpu,ram,net,dc,proxy,cache,ghost,lastseen");
const auto kPillStackOrderKeys = std::array<const char*, 8>{
	"cpu", "ram", "net", "dc", "proxy", "cache", "ghost", "lastseen" };

[[nodiscard]] QString SanitizePillStackOrder(const QString &value) {
	auto accepted = QStringList();
	for (const auto &part : value.split(',')) {
		const auto key = part.trimmed().toLower();
		for (const auto name : kPillStackOrderKeys) {
			const auto known = QString::fromLatin1(name);
			if ((key == known) && !accepted.contains(known)) {
				accepted.push_back(known);
				break;
			}
		}
	}
	for (const auto name : kPillStackOrderKeys) {
		const auto known = QString::fromLatin1(name);
		if (!accepted.contains(known)) {
			accepted.push_back(known);
		}
	}
	return accepted.join(',');
}

[[nodiscard]] QJsonObject Unwrap(const QJsonObject &root) {
	const auto values = root.value("exteraconfig");
	return values.isObject() ? values.toObject() : root;
}

[[nodiscard]] bool IsValidPluginId(const QString &id) {
	static const auto expression = QRegularExpression(
		QStringLiteral("^[A-Za-z][A-Za-z0-9_-]{1,31}$"));
	return expression.match(id).hasMatch();
}

[[nodiscard]] QJsonObject SanitizePluginStates(const QJsonValue &value) {
	auto result = QJsonObject();
	if (!value.isObject()) {
		return result;
	}
	const auto states = value.toObject();
	for (auto i = states.begin(); i != states.end(); ++i) {
		if (IsValidPluginId(i.key())
			&& i.value().isBool()
			&& i.value().toBool()) {
			result.insert(i.key(), true);
		}
	}
	return result;
}

[[nodiscard]] QJsonObject SanitizeExclusions(const QJsonValue &value) {
	auto result = QJsonObject();
	if (!value.isObject()) {
		return result;
	}
	const auto exclusions = value.toObject();
	for (auto i = exclusions.begin(); i != exclusions.end(); ++i) {
		auto ok = false;
		const auto raw = i.key().toLongLong(&ok);
		if (!ok || !raw || !i.value().isBool() || !i.value().toBool()) {
			continue;
		}
		const auto id = (raw < 0)
			? (quint64(0) - quint64(raw))
			: quint64(raw);
		result.insert(QString::number(id), true);
	}
	return result;
}

void SanitizeExclusions(QJsonObject *values, const char *key) {
	if (!values->contains(key)) {
		return;
	}
	const auto exclusions = SanitizeExclusions(values->value(key));
	if (exclusions.isEmpty()) {
		values->remove(key);
	} else {
		(*values)[key] = exclusions;
	}
}

} // namespace

Config &Config::Instance() {
	static auto instance = Config();
	return instance;
}

void Config::reset() {
	ensureLoaded();
	if (_values.isEmpty()) {
		return;
	}
	_values = QJsonObject();
	refreshChats();
	save();
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

bool Config::disableNumberRounding() const {
	return boolValue(kDisableNumberRounding, false);
}

void Config::setDisableNumberRounding(bool value) {
	setValue(kDisableNumberRounding, value);
}

bool Config::showSeconds() const {
	return boolValue(kShowSeconds, false);
}

void Config::setShowSeconds(bool value) {
	setValue(kShowSeconds, value);
}

bool Config::showIdDc() const {
	return boolValue(kShowIdDc, false);
}

void Config::setShowIdDc(bool value) {
	setValue(kShowIdDc, value);
}

bool Config::translucentDeletedMessages() const {
	return boolValue(kTranslucentDeleted, true);
}

void Config::setTranslucentDeletedMessages(bool value) {
	setValue(kTranslucentDeleted, value);
}

bool Config::panguOnSending() const {
	return boolValue(kPanguOnSending, false);
}

void Config::setPanguOnSending(bool value) {
	setValue(kPanguOnSending, value);
}

bool Config::silentByDefault() const {
	return boolValue(kSilentByDefault, false);
}

void Config::setSilentByDefault(bool value) {
	setValue(kSilentByDefault, value);
}

bool Config::hidePremiumSection() const {
	return boolValue(kHidePremiumSection, false);
}

void Config::setHidePremiumSection(bool value) {
	setValue(kHidePremiumSection, value);
}

bool Config::hideHelpSection() const {
	return boolValue(kHideHelpSection, false);
}

void Config::setHideHelpSection(bool value) {
	setValue(kHideHelpSection, value);
}

bool Config::hideAiEditor() const {
	return boolValue(kHideAiEditor, false);
}

void Config::setHideAiEditor(bool value) {
	setValue(kHideAiEditor, value);
}

bool Config::hideSendAsPeer() const {
	return boolValue(kHideSendAsPeer, false);
}

void Config::setHideSendAsPeer(bool value) {
	setValue(kHideSendAsPeer, value);
}

bool Config::hideGiftButton() const {
	return boolValue(kHideGiftButton, false);
}

void Config::setHideGiftButton(bool value) {
	setValue(kHideGiftButton, value);
}

bool Config::hideCallButton() const {
	return boolValue(kHideCallButton, false);
}

void Config::setHideCallButton(bool value) {
	setValue(kHideCallButton, value);
}

bool Config::hideChannelSearchButton() const {
	return boolValue(kHideChannelSearchButton, false);
}

void Config::setHideChannelSearchButton(bool value) {
	setValue(kHideChannelSearchButton, value);
}

bool Config::inlineMathResult() const {
	return boolValue(kInlineMathResult, false);
}

void Config::setInlineMathResult(bool value) {
	setValue(kInlineMathResult, value);
}

bool Config::saveToChatSubfolder() const {
	return boolValue(kSaveToChatSubfolder, false);
}

void Config::setSaveToChatSubfolder(bool value) {
	setValue(kSaveToChatSubfolder, value);
}

bool Config::ghostScheduledSend() const {
	return boolValue(kGhostScheduledSend, false);
}

void Config::setGhostScheduledSend(bool value) {
	setValue(kGhostScheduledSend, value);
}

int Config::seekDuration() const {
	return SanitizeSeekDuration(intValue(kSeekDuration, 5));
}

void Config::setSeekDuration(int value) {
	setValue(kSeekDuration, SanitizeSeekDuration(value));
}

bool Config::useEditedIcon() const {
	return boolValue(kUseEditedIcon, false);
}

void Config::setUseEditedIcon(bool value) {
	setValue(kUseEditedIcon, value);
}

QString Config::customEditedMark() const {
	return stringValue(kCustomEditedMark, QString()).left(
		kMaxEditedMarkLength);
}

void Config::setCustomEditedMark(const QString &value) {
	setValue(kCustomEditedMark, value.left(kMaxEditedMarkLength));
}

bool Config::showForwardDate() const {
	return boolValue(kShowForwardDate, false);
}

void Config::setShowForwardDate(bool value) {
	setValue(kShowForwardDate, value);
}

bool Config::senderMiniAvatars() const {
	return boolValue(kChatsSenderMiniAvatars, false);
}

void Config::setSenderMiniAvatars(bool value) {
	setValue(kChatsSenderMiniAvatars, value);
}

bool Config::mediaViewerMenuForward() const {
	return boolValue(kMediaViewerMenuForward, true);
}

void Config::setMediaViewerMenuForward(bool value) {
	setValue(kMediaViewerMenuForward, value);
}

bool Config::mediaViewerMenuCopyFrame() const {
	return boolValue(kMediaViewerMenuCopyFrame, true);
}

void Config::setMediaViewerMenuCopyFrame(bool value) {
	setValue(kMediaViewerMenuCopyFrame, value);
}

bool Config::mediaViewerMenuCopyPhoto() const {
	return boolValue(kMediaViewerMenuCopyPhoto, true);
}

void Config::setMediaViewerMenuCopyPhoto(bool value) {
	setValue(kMediaViewerMenuCopyPhoto, value);
}

bool Config::mediaViewerMenuSetProfile() const {
	return boolValue(kMediaViewerMenuSetProfile, true);
}

void Config::setMediaViewerMenuSetProfile(bool value) {
	setValue(kMediaViewerMenuSetProfile, value);
}

bool Config::mediaViewerMenuSubtitles() const {
	return boolValue(kMediaViewerMenuSubtitles, true);
}

void Config::setMediaViewerMenuSubtitles(bool value) {
	setValue(kMediaViewerMenuSubtitles, value);
}

QString Config::subtitlesFiles() const {
	return stringValue(kSubtitlesFiles, QString());
}

void Config::setSubtitlesFiles(const QString &value) {
	auto error = QJsonParseError{0, QJsonParseError::NoError};
	const auto document = QJsonDocument::fromJson(value.toUtf8(), &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return;
	}
	setValue(kSubtitlesFiles, value);
}

bool Config::chatMenuBoost() const {
	return boolValue(kChatMenuBoost, true);
}

void Config::setChatMenuBoost(bool value) {
	setValue(kChatMenuBoost, value);
}

bool Config::chatMenuLinked() const {
	return boolValue(kChatMenuLinked, true);
}

void Config::setChatMenuLinked(bool value) {
	setValue(kChatMenuLinked, value);
}

bool Config::chatMenuViewDeleted() const {
	return boolValue(kChatMenuViewDeleted, true);
}

void Config::setChatMenuViewDeleted(bool value) {
	setValue(kChatMenuViewDeleted, value);
}

bool Config::chatMenuClearDeleted() const {
	return boolValue(kChatMenuClearDeleted, true);
}

void Config::setChatMenuClearDeleted(bool value) {
	setValue(kChatMenuClearDeleted, value);
}

bool Config::disableLinkPreviewByDefault() const {
	return boolValue(kDisableLinkPreviewByDefault, false);
}

void Config::setDisableLinkPreviewByDefault(bool value) {
	setValue(kDisableLinkPreviewByDefault, value);
}

bool Config::hideAllChatsFolder() const {
	return boolValue(kHideAllChatsFolder, false);
}

void Config::setHideAllChatsFolder(bool value) {
	setValue(kHideAllChatsFolder, value);
}

bool Config::showResultsBeforeVoting() const {
	return boolValue(kShowResultsBeforeVoting, false);
}

void Config::setShowResultsBeforeVoting(bool value) {
	setValue(kShowResultsBeforeVoting, value);
}

bool Config::alwaysShowDownloadIcon() const {
	return boolValue(kAlwaysShowDownloadIcon, false);
}

void Config::setAlwaysShowDownloadIcon(bool value) {
	setValue(kAlwaysShowDownloadIcon, value);
}

bool Config::falseBottom() const {
	return boolValue(kFalseBottom, false);
}

void Config::setFalseBottom(bool value) {
	setValue(kFalseBottom, value);
}

int Config::decoration() const {
	return std::clamp(intValue(kDecoration, 0), 0, 3);
}

void Config::setDecoration(int value) {
	setValue(kDecoration, std::clamp(value, 0, 3));
}

bool Config::chatShortcutAdministrators() const {
	return boolValue(kShortcutAdministrators, false);
}

void Config::setChatShortcutAdministrators(bool value) {
	setValue(kShortcutAdministrators, value);
}

bool Config::chatShortcutRecentActions() const {
	return boolValue(kShortcutRecentActions, false);
}

void Config::setChatShortcutRecentActions(bool value) {
	setValue(kShortcutRecentActions, value);
}

bool Config::chatShortcutStatistics() const {
	return boolValue(kShortcutStatistics, false);
}

void Config::setChatShortcutStatistics(bool value) {
	setValue(kShortcutStatistics, value);
}

bool Config::chatShortcutPermissions() const {
	return boolValue(kShortcutPermissions, false);
}

void Config::setChatShortcutPermissions(bool value) {
	setValue(kShortcutPermissions, value);
}

bool Config::chatShortcutMembers() const {
	return boolValue(kShortcutMembers, false);
}

void Config::setChatShortcutMembers(bool value) {
	setValue(kShortcutMembers, value);
}

bool Config::groupedMessageMenu() const {
	return boolValue(kGroupedMessageMenu, false);
}

void Config::setGroupedMessageMenu(bool value) {
	setValue(kGroupedMessageMenu, value);
}

bool Config::defaultDeleteMenu() const {
	return boolValue(kDefaultDeleteMenu, false);
}

void Config::setDefaultDeleteMenu(bool value) {
	setValue(kDefaultDeleteMenu, value);
}

bool Config::useDeletedIcon() const {
	return boolValue(kUseDeletedIcon, true);
}

void Config::setUseDeletedIcon(bool value) {
	setValue(kUseDeletedIcon, value);
}

QString Config::customDeletedMark() const {
	return stringValue(kCustomDeletedMark, QString()).left(
		kMaxDeletedMarkLength);
}

void Config::setCustomDeletedMark(const QString &value) {
	setValue(
		kCustomDeletedMark,
		value.left(kMaxDeletedMarkLength));
}

int Config::deletedMarkColor() const {
	const auto value = intValue(kDeletedMarkColor, 0);
	return (value >= 0) ? value : 0;
}

void Config::setDeletedMarkColor(int value) {
	setValue(kDeletedMarkColor, (value >= 0) ? value : 0);
}

bool Config::hidePhone() const {
	return boolValue(kHidePhone, false);
}

void Config::setHidePhone(bool value) {
	setValue(kHidePhone, value);
}

bool Config::usePersianCalendar() const {
	return boolValue(kGeneralUsePersianCalendar, false);
}

void Config::setUsePersianCalendar(bool value) {
	setValue(kGeneralUsePersianCalendar, value);
}

bool Config::displayPersianCalendarByLatin() const {
	return boolValue(kGeneralPersianCalendarLatin, false);
}

void Config::setDisplayPersianCalendarByLatin(bool value) {
	setValue(kGeneralPersianCalendarLatin, value);
}

int Config::defaultVideoQuality() const {
	// 0 = Auto, higher values request a fixed HLS tier.
	return std::clamp(intValue(kGeneralDefaultVideoQuality, 0), 0, 2);
}

void Config::setDefaultVideoQuality(int value) {
	setValue(kGeneralDefaultVideoQuality, std::clamp(value, 0, 2));
}

int Config::autoUpdateChannel() const {
	// 0 = off, 1 = release, 2 = beta.
	return std::clamp(intValue(kGeneralAutoUpdateChannel, 1), 0, 2);
}

void Config::setAutoUpdateChannel(int value) {
	setValue(kGeneralAutoUpdateChannel, std::clamp(value, 0, 2));
}

bool Config::hardwareAcceleration() const {
	return boolValue(kGeneralHardwareAcceleration, true);
}

void Config::setHardwareAcceleration(bool value) {
	setValue(kGeneralHardwareAcceleration, value);
}

bool Config::takeGifAsVideo() const {
	return boolValue(kChatsTakeGifAsVideo, false);
}

void Config::setTakeGifAsVideo(bool value) {
	setValue(kChatsTakeGifAsVideo, value);
}

int Config::customAudioBitrate() const {
	return SanitizeAudioBitrate(intValue(kGeneralCustomAudioBitrate, 32));
}

void Config::setCustomAudioBitrate(int value) {
	setValue(kGeneralCustomAudioBitrate, SanitizeAudioBitrate(value));
}

bool Config::botPrivacyEnabled() const {
	return boolValue(kGeneralBotPrivacyEnabled, false);
}

void Config::setBotPrivacyEnabled(bool value) {
	setValue(kGeneralBotPrivacyEnabled, value);
}

int Config::botPrivacyAccount() const {
	return std::clamp(intValue(kGeneralBotPrivacyAccount, -1), -1, 15);
}

void Config::setBotPrivacyAccount(int value) {
	setValue(kGeneralBotPrivacyAccount, std::clamp(value, -1, 15));
}

bool Config::botPrivacyAlwaysAsk() const {
	return boolValue(kGeneralBotPrivacyAsk, false);
}

void Config::setBotPrivacyAlwaysAsk(bool value) {
	setValue(kGeneralBotPrivacyAsk, value);
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

bool Config::ghostShowStatus() const {
	return boolValue(kGhostShowStatus, false);
}

void Config::setGhostShowStatus(bool value) {
	setValue(kGhostShowStatus, value);
}

bool Config::ghostSendReadLocked() const {
	return boolValue(kGhostSendReadLocked, false);
}

void Config::setGhostSendReadLocked(bool value) {
	setValue(kGhostSendReadLocked, value);
}

bool Config::ghostSendStoriesLocked() const {
	return boolValue(kGhostSendStoriesLocked, false);
}

void Config::setGhostSendStoriesLocked(bool value) {
	setValue(kGhostSendStoriesLocked, value);
}

bool Config::ghostSendOnlineLocked() const {
	return boolValue(kGhostSendOnlineLocked, false);
}

void Config::setGhostSendOnlineLocked(bool value) {
	setValue(kGhostSendOnlineLocked, value);
}

bool Config::ghostSendUploadProgressLocked() const {
	return boolValue(kGhostSendUploadProgressLocked, false);
}

void Config::setGhostSendUploadProgressLocked(bool value) {
	setValue(kGhostSendUploadProgressLocked, value);
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

QJsonObject Config::ghostReadExclusions() const {
	ensureLoaded();
	return SanitizeExclusions(_values.value(kGhostReadExclusions));
}

void Config::setGhostReadExclusions(const QJsonObject &value) {
	const auto exclusions = SanitizeExclusions(QJsonValue(value));
	setValue(
		kGhostReadExclusions,
		exclusions.isEmpty()
			? QJsonValue(QJsonValue::Undefined)
			: QJsonValue(exclusions));
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

QJsonObject Config::ghostTypingExclusions() const {
	ensureLoaded();
	return SanitizeExclusions(_values.value(kGhostTypingExclusions));
}

void Config::setGhostTypingExclusions(const QJsonObject &value) {
	const auto exclusions = SanitizeExclusions(QJsonValue(value));
	setValue(
		kGhostTypingExclusions,
		exclusions.isEmpty()
			? QJsonValue(QJsonValue::Undefined)
			: QJsonValue(exclusions));
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

QString Config::pillStackOrder() const {
	return SanitizePillStackOrder(
		stringValue(kPillStackOrder, kDefaultPillStackOrder));
}

void Config::setPillStackOrder(const QString &value) {
	setValue(kPillStackOrder, SanitizePillStackOrder(value));
}

QString Config::mainMenuOrder() const {
	return stringValue(kMainMenuOrder, kDefaultMainMenuOrder);
}

void Config::setMainMenuOrder(const QString &value) {
	setValue(kMainMenuOrder, value);
}

bool Config::drawerAccountsExpanded() const {
	return boolValue(kDrawerAccountsExpanded, true);
}

void Config::setDrawerAccountsExpanded(bool value) {
	setValue(kDrawerAccountsExpanded, value);
}

ChatsSettings Config::chatsSettings() const {
	ensureLoaded();
	return _chats;
}

void Config::setChatsRemoveMessageTail(bool value) {
	setValue(kChatsRemoveMessageTail, value);
}

void Config::setChatsHideReactionsChannels(bool value) {
	setValue(kChatsHideReactionsChannels, value);
}

void Config::setChatsHideReactionsGroups(bool value) {
	setValue(kChatsHideReactionsGroups, value);
}

void Config::setChatsHideReactionsPrivate(bool value) {
	setValue(kChatsHideReactionsPrivate, value);
}

void Config::setChatsStripTrackingOnOpen(bool value) {
	setValue(kChatsStripTrackingOnOpen, value);
}

void Config::setChatsStripTrackingOnPaste(bool value) {
	setValue(kChatsStripTrackingOnPaste, value);
}

void Config::setChatsUnreadCapsule(bool value) {
	setValue(kChatsUnreadCapsule, value);
}

MessageMenuSettings Config::messageMenuSettings() const {
	ensureLoaded();
	return _messageMenu;
}

void Config::setChatsMenuReply(bool value) {
	setValue(kChatsMenuReply, value);
}

void Config::setChatsMenuEdit(bool value) {
	setValue(kChatsMenuEdit, value);
}

void Config::setChatsMenuPin(bool value) {
	setValue(kChatsMenuPin, value);
}

void Config::setChatsMenuCopyText(bool value) {
	setValue(kChatsMenuCopyText, value);
}

void Config::setChatsMenuCopyLink(bool value) {
	setValue(kChatsMenuCopyLink, value);
}

void Config::setChatsMenuCopyImage(bool value) {
	setValue(kChatsMenuCopyImage, value);
}

void Config::setChatsMenuForward(bool value) {
	setValue(kChatsMenuForward, value);
}

void Config::setChatsMenuSelect(bool value) {
	setValue(kChatsMenuSelect, value);
}

void Config::setChatsMenuTranslate(bool value) {
	setValue(kChatsMenuTranslate, value);
}

void Config::setChatsMenuReport(bool value) {
	setValue(kChatsMenuReport, value);
}

void Config::setChatsMenuDelete(bool value) {
	setValue(kChatsMenuDelete, value);
}

int Config::chatsDoubleTapIn() const {
	return SanitizeDoubleTapValue(intValue(kChatsDoubleTapIn, -1));
}

void Config::setChatsDoubleTapIn(int value) {
	setValue(kChatsDoubleTapIn, SanitizeDoubleTapValue(value));
}

int Config::chatsDoubleTapOut() const {
	return SanitizeDoubleTapValue(intValue(kChatsDoubleTapOut, -1));
}

void Config::setChatsDoubleTapOut(int value) {
	setValue(kChatsDoubleTapOut, SanitizeDoubleTapValue(value));
}

int Config::stickerSize() const {
	return std::clamp(intValue(kChatsStickerSize, 14), 4, 20);
}

void Config::setStickerSize(int value) {
	setValue(kChatsStickerSize, std::clamp(value, 4, 20));
}

void Config::setChatsStickerSize(int value) {
	setStickerSize(value);
}

int Config::stickerShape() const {
	return std::clamp(intValue(kChatsStickerShape, 0), 0, 2);
}

void Config::setStickerShape(int value) {
	setValue(kChatsStickerShape, std::clamp(value, 0, 2));
}

int Config::avatarRadius() const {
	return std::clamp(intValue(kChatsAvatarRadius, -1), -1, 32);
}

void Config::setAvatarRadius(int value) {
	setValue(kChatsAvatarRadius, std::clamp(value, -1, 32));
}

int Config::sectionRadius() const {
	return std::clamp(intValue(kChatsSectionRadius, -1), -1, 16);
}

void Config::setSectionRadius(int value) {
	setValue(kChatsSectionRadius, std::clamp(value, -1, 16));
}

bool Config::sectionSeparateHeaders() const {
	return boolValue(kChatsSectionSeparateHeaders, false);
}

void Config::setSectionSeparateHeaders(bool value) {
	setValue(kChatsSectionSeparateHeaders, value);
}

int Config::chatHeaderStyle() const {
	return std::clamp(intValue(kChatsChatHeaderStyle, 0), 0, 2);
}

void Config::setChatHeaderStyle(int value) {
	setValue(kChatsChatHeaderStyle, std::clamp(value, 0, 2));
}

bool Config::replyColors() const {
	return boolValue(kChatsReplyColors, true);
}

void Config::setChatsReplyColors(bool value) {
	setValue(kChatsReplyColors, value);
}

bool Config::replyEmoji() const {
	return boolValue(kChatsReplyEmoji, true);
}

void Config::setChatsReplyEmoji(bool value) {
	setValue(kChatsReplyEmoji, value);
}

bool Config::replyBackground() const {
	return boolValue(kChatsReplyBackground, true);
}

void Config::setChatsReplyBackground(bool value) {
	setValue(kChatsReplyBackground, value);
}

void Config::setChatsStickerTimeSide(bool value) {
	setValue(kChatsStickerTimeSide, value);
}

void Config::setChatsHideStickerTime(bool value) {
	setValue(kChatsHideStickerTime, value);
}

void Config::setChatsHideStoriesHeader(bool value) {
	setValue(kChatsHideStoriesHeader, value);
}

void Config::setChatsHideArchive(bool value) {
	setValue(kChatsHideArchive, value);
}

void Config::setChatsSortByUnread(bool value) {
	setValue(kChatsSortByUnread, value);
}

void Config::setChatsWideChannelPosts(bool value) {
	setValue(kChatsWideChannelPosts, value);
}

void Config::setChatsHideGreetingSticker(bool value) {
	setValue(kChatsHideGreetingSticker, value);
}

void Config::setChatsHidePanelsOnScroll(bool value) {
	setValue(kChatsHidePanelsOnScroll, value);
}

void Config::setChatsChannelBottomButton(int value) {
	setValue(kChatsChannelBottomButton, std::clamp(value, 0, 2));
}

void Config::setChatsDownloadSpeedBoost(int value) {
	setValue(kChatsDownloadSpeedBoost, std::clamp(value, 0, 2));
}

void Config::setChatsUploadBoost(bool value) {
	setValue(kChatsUploadBoost, value);
}

bool Config::disableAutoDownloadingArchive() const {
	return boolValue(kChatsDisableAutoDownloadingArchive, false);
}

void Config::setDisableAutoDownloadingArchive(bool value) {
	setValue(kChatsDisableAutoDownloadingArchive, value);
}

bool Config::blockWin32Executables() const {
	return boolValue(kChatsBlockWin32ExecutableFiles, true);
}

void Config::setBlockWin32Executables(bool value) {
	setValue(kChatsBlockWin32ExecutableFiles, value);
}

bool Config::zalgoFilter() const {
	return boolValue(kChatsZalgoFilter, false);
}

void Config::setZalgoFilter(bool value) {
	setValue(kChatsZalgoFilter, value);
}

bool Config::showSpoilersDirectly() const {
	return boolValue(kChatsShowSpoilersDirectly, false);
}

void Config::setShowSpoilersDirectly(bool value) {
	setValue(kChatsShowSpoilersDirectly, value);
}

bool Config::showFormattingButtons() const {
	return boolValue(kChatsShowFormattingButtons, false);
}

void Config::setShowFormattingButtons(bool value) {
	setValue(kChatsShowFormattingButtons, value);
}

int Config::translationProvider() const {
	return std::clamp(intValue(kTranslationProvider, 0), 0, 3);
}

void Config::setTranslationProvider(int value) {
	setValue(kTranslationProvider, std::clamp(value, 0, 3));
}

QString Config::translationGoogleCloudKey() const {
	return stringValue(kTranslationGoogleCloudKey, QString());
}

void Config::setTranslationGoogleCloudKey(const QString &value) {
	setValue(kTranslationGoogleCloudKey, value);
}

QString Config::translationDeepLKey() const {
	return stringValue(kTranslationDeepLKey, QString());
}

void Config::setTranslationDeepLKey(const QString &value) {
	setValue(kTranslationDeepLKey, value);
}

int Config::translationDeepLFormality() const {
	return std::clamp(intValue(kTranslationDeepLFormality, 0), 0, 2);
}

void Config::setTranslationDeepLFormality(int value) {
	setValue(kTranslationDeepLFormality, std::clamp(value, 0, 2));
}

QString Config::aiEndpoint() const {
	return stringValue(
		kAiEndpoint,
		QStringLiteral("https://api.openai.com/v1"));
}

void Config::setAiEndpoint(const QString &value) {
	setValue(kAiEndpoint, value);
}

QString Config::aiModel() const {
	return stringValue(kAiModel, QStringLiteral("gpt-4o-mini"));
}

void Config::setAiModel(const QString &value) {
	setValue(kAiModel, value);
}

QString Config::aiApiKey() const {
	return stringValue(kAiApiKey, QString());
}

void Config::setAiApiKey(const QString &value) {
	setValue(kAiApiKey, value);
}

bool Config::pluginEnabled(const QString &id) const {
	if (!IsValidPluginId(id)) {
		return false;
	}
	ensureLoaded();
	return SanitizePluginStates(
		_values.value(kPluginStates)).value(id).toBool();
}

void Config::setPluginEnabled(const QString &id, bool enabled) {
	if (!IsValidPluginId(id)) {
		return;
	}
	ensureLoaded();
	auto states = SanitizePluginStates(_values.value(kPluginStates));
	if (enabled) {
		states.insert(id, true);
	} else {
		states.remove(id);
	}
	setValue(
		kPluginStates,
		states.isEmpty()
			? QJsonValue(QJsonValue::Undefined)
			: QJsonValue(states));
}

QJsonObject Config::pluginStates() const {
	ensureLoaded();
	return SanitizePluginStates(_values.value(kPluginStates));
}

void Config::setPluginStates(const QJsonObject &states) {
	const auto sanitized = SanitizePluginStates(QJsonValue(states));
	setValue(
		kPluginStates,
		sanitized.isEmpty()
			? QJsonValue(QJsonValue::Undefined)
			: QJsonValue(sanitized));
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
			|| key == kDisableNumberRounding
			|| key == kShowSeconds
			|| key == kShowIdDc
			|| key == kTranslucentDeleted
			|| key == kPanguOnSending
			|| key == kSilentByDefault
			|| key == kHidePremiumSection
			|| key == kHideHelpSection
			|| key == kHideAiEditor
			|| key == kHideSendAsPeer
			|| key == kHideGiftButton
			|| key == kHideCallButton
			|| key == kHideChannelSearchButton
			|| key == kInlineMathResult
			|| key == kSaveToChatSubfolder
			|| key == kGhostScheduledSend
			|| key == kSeekDuration
			|| key == kUseEditedIcon
			|| key == kCustomEditedMark
			|| key == kShowForwardDate
			|| key == kChatsSenderMiniAvatars
			|| key == kMediaViewerMenuForward
			|| key == kMediaViewerMenuCopyFrame
			|| key == kMediaViewerMenuCopyPhoto
			|| key == kMediaViewerMenuSetProfile
			|| key == kMediaViewerMenuSubtitles
			|| key == kSubtitlesFiles
			|| key == kChatMenuBoost
			|| key == kChatMenuLinked
			|| key == kChatMenuViewDeleted
			|| key == kChatMenuClearDeleted
			|| key == kDisableLinkPreviewByDefault
			|| key == kHideAllChatsFolder
			|| key == kShowResultsBeforeVoting
			|| key == kAlwaysShowDownloadIcon
			|| key == kFalseBottom
			|| key == kDecoration
			|| key == kShortcutAdministrators
			|| key == kShortcutRecentActions
			|| key == kShortcutStatistics
			|| key == kShortcutPermissions
			|| key == kShortcutMembers
			|| key == kGroupedMessageMenu
			|| key == kDefaultDeleteMenu
			|| key == kUseDeletedIcon
			|| key == kCustomDeletedMark
			|| key == kDeletedMarkColor
			|| key == kHidePhone
			|| key == kGeneralUsePersianCalendar
			|| key == kGeneralPersianCalendarLatin
			|| key == kGeneralDefaultVideoQuality
			|| key == kGeneralHardwareAcceleration
			|| key == kChatsTakeGifAsVideo
			|| key == kGeneralCustomAudioBitrate
			|| key == kGeneralAutoUpdateChannel
			|| key == kGeneralBotPrivacyEnabled
			|| key == kGeneralBotPrivacyAccount
			|| key == kGeneralBotPrivacyAsk
			|| key == kProxyDisableConditions
			|| key == kProxyAutoDisabled
			|| key == kGhostSendReadPackets
			|| key == kGhostSendReadStories
			|| key == kGhostSendOnline
			|| key == kGhostSendUploadProgress
			|| key == kGhostShowStatus
			|| key == kGhostSendReadLocked
			|| key == kGhostSendStoriesLocked
			|| key == kGhostSendOnlineLocked
			|| key == kGhostSendUploadProgressLocked
			|| key == kGhostReadExclusions
			|| key == kGhostTypingExclusions
			|| key == kSaveDeletedMessages
			|| key == kPillStackEnabled
			|| key == kPillStackPills
			|| key == kPillStackOrder
			|| key == kMainMenuOrder
			|| key == kDrawerAccountsExpanded
			|| key == kPluginStates
			|| key == kChatsRemoveMessageTail
			|| key == kChatsHideReactionsChannels
			|| key == kChatsHideReactionsGroups
			|| key == kChatsHideReactionsPrivate
			|| key == kChatsStripTrackingOnOpen
			|| key == kChatsStripTrackingOnPaste
			|| key == kChatsUnreadCapsule
			|| key == kChatsMenuReply
			|| key == kChatsMenuEdit
			|| key == kChatsMenuPin
			|| key == kChatsMenuCopyText
			|| key == kChatsMenuCopyLink
			|| key == kChatsMenuCopyImage
			|| key == kChatsMenuForward
			|| key == kChatsMenuSelect
			|| key == kChatsMenuTranslate
			|| key == kChatsMenuReport
			|| key == kChatsMenuDelete
			|| key == kChatsDoubleTapIn
			|| key == kChatsDoubleTapOut
			|| key == kChatsStickerTimeSide
			|| key == kChatsStickerSize
			|| key == kChatsStickerShape
			|| key == kChatsReplyColors
			|| key == kChatsReplyEmoji
			|| key == kChatsReplyBackground
			|| key == kChatsHideStickerTime
			|| key == kChatsHideStoriesHeader
			|| key == kChatsHideArchive
			|| key == kChatsSortByUnread
			|| key == kChatsWideChannelPosts
			|| key == kChatsHideGreetingSticker
			|| key == kChatsHidePanelsOnScroll
			|| key == kChatsChannelBottomButton
			|| key == kChatsDownloadSpeedBoost
			|| key == kChatsUploadBoost
			|| key == kChatsDisableAutoDownloadingArchive
			|| key == kChatsBlockWin32ExecutableFiles
			|| key == kChatsZalgoFilter
			|| key == kChatsShowSpoilersDirectly
			|| key == kChatsShowFormattingButtons
			|| key == kChatsAvatarRadius
			|| key == kChatsSectionRadius
			|| key == kChatsSectionSeparateHeaders
			|| key == kChatsChatHeaderStyle
			|| key == kTranslationProvider
			|| key == kTranslationGoogleCloudKey
			|| key == kTranslationDeepLKey
			|| key == kTranslationDeepLFormality
			|| key == kAiEndpoint
			|| key == kAiModel
			|| key == kAiApiKey) {
			accepted.insert(key, i.value());
		} else if (unknown) {
			unknown->push_back(key);
		}
	}
	if (accepted.contains(kPluginStates)) {
		const auto states = SanitizePluginStates(
			accepted.value(kPluginStates));
		if (states.isEmpty()) {
			accepted.remove(kPluginStates);
		} else {
			accepted[kPluginStates] = states;
		}
	}
	SanitizeExclusions(&accepted, kGhostReadExclusions);
	SanitizeExclusions(&accepted, kGhostTypingExclusions);
	if (const auto order = accepted.value(kPillStackOrder); order.isString()) {
		accepted[kPillStackOrder] = SanitizePillStackOrder(order.toString());
	} else if (accepted.contains(kPillStackOrder)) {
		accepted.remove(kPillStackOrder);
	}
	for (const auto key : { kChatsDoubleTapIn, kChatsDoubleTapOut }) {
		const auto value = accepted.value(key);
		if (value.isDouble()) {
			accepted[key] = SanitizeDoubleTapValue(value.toInt());
		} else if (accepted.contains(key)) {
			accepted.remove(key);
		}
	}
	if (accepted.isEmpty()) {
		return false;
	}
	_values = accepted;
	save();
	refreshChats();
	return true;
}

void Config::setWorkingDir(const QString &workingDir) {
	if (workingDir.isEmpty() || _workingDir == workingDir) {
		return;
	}
	_workingDir = workingDir;
	if (_loaded) {
		_loaded = false;
		_values = QJsonObject();
		ensureLoaded();
	}
}

void Config::ensureLoaded() const {
	if (_loaded) {
		return;
	}
	_loaded = true;
	const auto standard = QStandardPaths::writableLocation(
		QStandardPaths::AppLocalDataLocation) + "/tdata/exteraless.json";
	if (_workingDir.isEmpty()) {
		_path = standard;
	} else {
		_path = QDir(_workingDir).filePath(
			QStringLiteral("tdata/exteraless.json"));
		if (!QFile::exists(_path) && QFile::exists(standard)) {
			QDir().mkpath(QFileInfo(_path).absolutePath());
			QFile::copy(standard, _path);
		}
	}

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
	if (_values.contains(kPluginStates)) {
		const auto states = SanitizePluginStates(_values.value(kPluginStates));
		if (states.isEmpty()) {
			_values.remove(kPluginStates);
		} else {
			_values[kPluginStates] = states;
		}
	}
	SanitizeExclusions(&_values, kGhostReadExclusions);
	SanitizeExclusions(&_values, kGhostTypingExclusions);
	refreshChats();
}

void Config::refreshChats() const {
	_chats.removeMessageTail = boolValue(kChatsRemoveMessageTail, true);
	_chats.hideReactionsChannels = boolValue(
		kChatsHideReactionsChannels, false);
	_chats.hideReactionsGroups = boolValue(kChatsHideReactionsGroups, false);
	_chats.hideReactionsPrivate = boolValue(kChatsHideReactionsPrivate, false);
	_chats.stripTrackingOnOpen = boolValue(kChatsStripTrackingOnOpen, true);
	_chats.stripTrackingOnPaste
		= boolValue(kChatsStripTrackingOnPaste, true);
	_chats.unreadCapsule = boolValue(kChatsUnreadCapsule, false);
	_chats.stickerTimeSide = boolValue(kChatsStickerTimeSide, false);
	_chats.hideStickerTime = boolValue(kChatsHideStickerTime, false);
	_chats.stickerSize
		= std::clamp(intValue(kChatsStickerSize, 14), 4, 20);
	_chats.stickerShape
		= std::clamp(intValue(kChatsStickerShape, 0), 0, 2);
	_chats.replyColors = boolValue(kChatsReplyColors, true);
	_chats.replyEmoji = boolValue(kChatsReplyEmoji, true);
	_chats.replyBackground = boolValue(kChatsReplyBackground, true);
	_chats.hideStoriesHeader = boolValue(kChatsHideStoriesHeader, false);
	_chats.hideArchive = boolValue(kChatsHideArchive, false);
	_chats.sortByUnread = boolValue(kChatsSortByUnread, false);
	_chats.wideChannelPosts = boolValue(kChatsWideChannelPosts, false);
	_chats.hideGreetingSticker = boolValue(
		kChatsHideGreetingSticker, false);
	_chats.hidePanelsOnScroll = boolValue(kChatsHidePanelsOnScroll, false);
	_chats.channelBottomButton
		= std::clamp(intValue(kChatsChannelBottomButton, 1), 0, 2);
	_chats.downloadSpeedBoost
		= std::clamp(intValue(kChatsDownloadSpeedBoost, 0), 0, 2);
	_chats.uploadBoost = boolValue(kChatsUploadBoost, false);
	_chats.disableAutoDownloadingArchive = boolValue(
		kChatsDisableAutoDownloadingArchive,
		false);
	_chats.blockWin32Executables = boolValue(
		kChatsBlockWin32ExecutableFiles,
		true);
	_chats.avatarRadius = std::clamp(intValue(kChatsAvatarRadius, -1), -1, 32);

	_messageMenu.reply = boolValue(kChatsMenuReply, true);
	_messageMenu.edit = boolValue(kChatsMenuEdit, true);
	_messageMenu.pin = boolValue(kChatsMenuPin, true);
	_messageMenu.copyText = boolValue(kChatsMenuCopyText, true);
	_messageMenu.copyLink = boolValue(kChatsMenuCopyLink, true);
	_messageMenu.copyImage = boolValue(kChatsMenuCopyImage, true);
	_messageMenu.forward = boolValue(kChatsMenuForward, true);
	_messageMenu.select = boolValue(kChatsMenuSelect, true);
	_messageMenu.translate = boolValue(kChatsMenuTranslate, true);
	_messageMenu.report = boolValue(kChatsMenuReport, true);
	_messageMenu.deleteMsg = boolValue(kChatsMenuDelete, true);
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

QString Config::stringValue(
		const char *key,
		const QString &fallback) const {
	ensureLoaded();
	const auto value = _values.value(key);
	return value.isString() ? value.toString() : fallback;
}

void Config::setValue(const char *key, const QJsonValue &value) {
	ensureLoaded();
	if (_values.value(key) == value) {
		return;
	}
	_values[key] = value;
	refreshChats();
	save();
}

} // namespace Exteraless
