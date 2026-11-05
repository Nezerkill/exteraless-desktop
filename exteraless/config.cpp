#include "exteraless/config.h"

#include "exteraless/doubletap.h"

#include <QDir>
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
constexpr auto kTranslucentDeleted = "OEChatsTranslucentDeleted";
constexpr auto kPanguOnSending = "OEChatsPanguOnSending";
constexpr auto kSilentByDefault = "OEChatsSilentByDefault";
constexpr auto kHidePremiumSection = "OESettingsHidePremium";
constexpr auto kHideHelpSection = "OESettingsHideHelp";
constexpr auto kHideAiEditor = "OEAppearanceHideAiEditor";
constexpr auto kHideSendAsPeer = "OEChatsHideSendAsPeer";
constexpr auto kInlineMathResult = "OEChatsInlineMathResult";
constexpr auto kSaveToChatSubfolder = "OEGeneralSaveToChatSubfolder";
constexpr auto kGhostScheduledSend = "OEGhostScheduledSend";
constexpr auto kSeekDuration = "OEChatsSeekDuration";
constexpr auto kUseEditedIcon = "OEChatsUseEditedIcon";
constexpr auto kCustomEditedMark = "OEChatsCustomEditedMark";
constexpr auto kShowForwardDate = "OEChatsShowForwardDate";
constexpr auto kMediaViewerMenuForward = "OEMediaViewerMenuForward";
constexpr auto kMediaViewerMenuCopyFrame = "OEMediaViewerMenuCopyFrame";
constexpr auto kMediaViewerMenuCopyPhoto = "OEMediaViewerMenuCopyPhoto";
constexpr auto kMediaViewerMenuSetProfile = "OEMediaViewerMenuSetProfile";
constexpr auto kChatMenuBoost = "OEChatsChatMenuBoost";
constexpr auto kChatMenuLinked = "OEChatsChatMenuLinked";
constexpr auto kChatMenuViewDeleted = "OEChatsChatMenuViewDeleted";
constexpr auto kChatMenuClearDeleted = "OEChatsChatMenuClearDeleted";
constexpr auto kDisableLinkPreviewByDefault = "OEChatsDisableLinkPreviewByDefault";
constexpr auto kHideAllChatsFolder = "OEChatsHideAllChatsFolder";
constexpr auto kAlwaysShowDownloadIcon = "OEChatsAlwaysShowDownloadIcon";

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
constexpr auto kChatsHideStickerTime = "OEChatsHideStickerTime";

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

bool Config::alwaysShowDownloadIcon() const {
	return boolValue(kAlwaysShowDownloadIcon, false);
}

void Config::setAlwaysShowDownloadIcon(bool value) {
	setValue(kAlwaysShowDownloadIcon, value);
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

void Config::setChatsStickerTimeSide(bool value) {
	setValue(kChatsStickerTimeSide, value);
}

void Config::setChatsHideStickerTime(bool value) {
	setValue(kChatsHideStickerTime, value);
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
			|| key == kTranslucentDeleted
			|| key == kPanguOnSending
			|| key == kSilentByDefault
			|| key == kHidePremiumSection
			|| key == kHideHelpSection
			|| key == kHideAiEditor
			|| key == kHideSendAsPeer
			|| key == kInlineMathResult
			|| key == kSaveToChatSubfolder
			|| key == kGhostScheduledSend
			|| key == kSeekDuration
			|| key == kUseEditedIcon
			|| key == kCustomEditedMark
			|| key == kShowForwardDate
			|| key == kMediaViewerMenuForward
			|| key == kMediaViewerMenuCopyFrame
			|| key == kMediaViewerMenuCopyPhoto
			|| key == kMediaViewerMenuSetProfile
			|| key == kChatMenuBoost
			|| key == kChatMenuLinked
			|| key == kChatMenuViewDeleted
			|| key == kChatMenuClearDeleted
			|| key == kDisableLinkPreviewByDefault
			|| key == kHideAllChatsFolder
			|| key == kAlwaysShowDownloadIcon
			|| key == kUseDeletedIcon
			|| key == kCustomDeletedMark
			|| key == kDeletedMarkColor
			|| key == kHidePhone
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
			|| key == kChatsHideStickerTime) {
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
