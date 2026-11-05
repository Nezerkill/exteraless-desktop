#include "exteraless/backup.h"

#include "exteraless/config.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

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
constexpr auto kTranslucentDeleted = "OEChatsTranslucentDeleted";
constexpr auto kPanguOnSending = "OEChatsPanguOnSending";
constexpr auto kSilentByDefault = "OEChatsSilentByDefault";
constexpr auto kHidePremiumSection = "OESettingsHidePremium";
constexpr auto kHideHelpSection = "OESettingsHideHelp";
constexpr auto kHideAiEditor = "OEAppearanceHideAiEditor";
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
constexpr auto kPluginStates = "OEPluginStates";
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
constexpr auto kChatsHideStickerTime = "OEChatsHideStickerTime";
constexpr auto kHideStickerTime = "hideStickerTime";
constexpr auto kStickerTimeMode = "stickerTimeMode";
constexpr auto kDoubleTapAction = "doubleTapAction";
constexpr auto kDoubleTapActionOut = "doubleTapActionOutOwner";

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
	extera[kHidePhoneNumber] = config.hidePhone();
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
	extera[kChatsHideStickerTime] = chats.hideStickerTime;

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
		} else if (key == kPluginStates && value.isObject()) {
			config.setPluginStates(value.toObject());
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
		} else if ((key == kChatsHideStickerTime || key == kHideStickerTime)
			&& value.isBool()) {
			config.setChatsHideStickerTime(value.toBool());
		} else if (key == kStickerTimeMode
			&& value.isDouble()
			&& value.toInt() >= 0
			&& value.toInt() <= 2) {
			const auto mode = value.toInt();
			config.setChatsHideStickerTime(mode == 2);
			config.setChatsStickerTimeSide(mode == 1);
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
