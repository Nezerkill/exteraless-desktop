#include "exteraless/ai_client.h"
#include "exteraless/backup.h"
#include "exteraless/chats.h"
#include "exteraless/config.h"
#include "exteraless/deleted_entry.h"
#include "exteraless/deleted_messages_trim.h"
#include "exteraless/doubletap.h"
#include "exteraless/extra_pills.h"
#include "exteraless/format.h"
#include "exteraless/ghost_schedule.h"
#include "exteraless/ghost_status.h"
#include "exteraless/links.h"
#include "exteraless/lrclib.h"
#include "exteraless/lyrics.h"
#include "exteraless/math.h"
#include "exteraless/network.h"
#include "exteraless/pangu.h"
#include "exteraless/plugins/installer.h"
#include "exteraless/proxy_rules.h"
#include "exteraless/regex_filters.h"
#include "exteraless/save_path.h"
#include "exteraless/subtitles.h"
#include "exteraless/update_check.h"
#include "exteraless/zalgo.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDate>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

namespace {

using Exteraless::ChatsSettings;

[[nodiscard]] int PrintUsage() {
	std::cerr
		<< "Usage:\n"
		<< "  exteraless-chats-cli strip <url>\n"
		<< "  exteraless-chats-cli strip-text <text>\n"
		<< "  exteraless-chats-cli hidden <hideChannels> <hideGroups> "
		<< "<hidePrivate> <channel> <group> <user>\n"
		<< "  exteraless-chats-cli config-defaults\n"
		<< "  exteraless-chats-cli config-loaded\n"
		<< "  exteraless-chats-cli config-set <removeTail> <hideChannels> "
		<< "<hideGroups> <hidePrivate> <stripTracking> <stripTrackingPaste> "
		<< "<unreadCapsule>\n"
		<< "  exteraless-chats-cli config-fromjson <json>\n"
		<< "  exteraless-chats-cli config-raw\n"
		<< "  exteraless-chats-cli config-reset\n"
		<< "  exteraless-chats-cli general\n"
		<< "  exteraless-chats-cli general-set <0|1> [<showSeconds:0|1>]\n"
		<< "  exteraless-chats-cli bot-privacy\n"
		<< "  exteraless-chats-cli bot-privacy-set <enabled 0|1> "
		<< "<account -1..15> <ask 0|1>\n"
		<< "  exteraless-chats-cli id-dc\n"
		<< "  exteraless-chats-cli id-dc-set <0|1>\n"
		<< "  exteraless-chats-cli id-dc-format <bareId> <dc> <0|1>\n"
		<< "  exteraless-chats-cli clock <hh> <mm> <ss> <0|1> [<locale>]\n"
		<< "  exteraless-chats-cli persian-date <yyyy-mm-dd> <latin 0|1> "
		<< "<year 0|1>\n"
		<< "  exteraless-chats-cli persian-calendar\n"
		<< "  exteraless-chats-cli persian-calendar-set <0|1> [<latin 0|1>]\n"
		<< "  exteraless-chats-cli message-menu\n"
		<< "  exteraless-chats-cli message-menu-set <reply> <edit> <pin> "
		<< "<copyText> <copyLink> <copyImage> <forward> <select> "
		<< "<translate> <report> <deleteMsg>\n"
		<< "  exteraless-chats-cli doubletap\n"
		<< "  exteraless-chats-cli doubletap-set <in> <out>\n"
		<< "  exteraless-chats-cli doubletap-resolve <value>\n"
		<< "  exteraless-chats-cli stickertime\n"
		<< "  exteraless-chats-cli stickertime-set <side> <hidden>\n"
		<< "  exteraless-chats-cli pillorder\n"
		<< "  exteraless-chats-cli pillorder-set <csv>\n"
		<< "  exteraless-chats-cli pillorder-reset\n"
		<< "  exteraless-chats-cli extra-pills-parse <domain> <file> "
		<< "[<code>]\n"
		<< "  exteraless-chats-cli ghost\n"
		<< "  exteraless-chats-cli ghost-set <0|1>\n"
		<< "  exteraless-chats-cli ghost-flags <read> <stories> <online> "
		<< "<typing>\n"
		<< "  exteraless-chats-cli ghost-show <0|1>\n"
		<< "  exteraless-chats-cli ghost-schedule\n"
		<< "  exteraless-chats-cli ghost-schedule-set <0|1>\n"
		<< "  exteraless-chats-cli ghost-schedule-decide <enabled> "
		<< "<sendOnline> <self> <secret> <bot> <user>\n"
		<< "  exteraless-chats-cli ghost-lock <read> <stories> <online> "
		<< "<typing>\n"
		<< "  exteraless-chats-cli deletedmark\n"
		<< "  exteraless-chats-cli deletedmark-set <translucent> <icon> "
		<< "<mark> <color>\n"
		<< "  exteraless-chats-cli vpn-name <interface>\n"
		<< "  exteraless-chats-cli proxy-rule <conditionActive> <autoDisabled> "
		<< "<shouldDisable> <proxyEnabled>\n"
		<< "  exteraless-chats-cli pangu\n"
		<< "  exteraless-chats-cli pangu-set <0|1>\n"
		<< "  exteraless-chats-cli pangu-apply <text>\n"
		<< "  exteraless-chats-cli pangu-tags <text> <offset> <length>\n"
		<< "  exteraless-chats-cli zalgo\n"
		<< "  exteraless-chats-cli zalgo-set <0|1>\n"
		<< "  exteraless-chats-cli zalgo-apply <text>\n"
		<< "  exteraless-chats-cli zalgo-tags <text> <offset> <length>\n"
		<< "  exteraless-chats-cli spoilers-direct\n"
		<< "  exteraless-chats-cli spoilers-direct-set <0|1>\n"
		<< "  exteraless-chats-cli formatting-buttons\n"
		<< "  exteraless-chats-cli formatting-buttons-set <0|1>\n"
		<< "  exteraless-chats-cli silent\n"
		<< "  exteraless-chats-cli silent-set <0|1>\n"
		<< "  exteraless-chats-cli settings-sections\n"
		<< "  exteraless-chats-cli settings-sections-set <hidePremium> "
		<< "<hideHelp>\n"
		<< "  exteraless-chats-cli ai-editor\n"
		<< "  exteraless-chats-cli ai-editor-set <0|1>\n"
		<< "  exteraless-chats-cli disable-link-preview\n"
		<< "  exteraless-chats-cli disable-link-preview-set <0|1>\n"
		<< "  exteraless-chats-cli hide-all-chats\n"
		<< "  exteraless-chats-cli hide-all-chats-set <0|1>\n"
		<< "  exteraless-chats-cli hide-archive\n"
		<< "  exteraless-chats-cli hide-archive-set <0|1>\n"
		<< "  exteraless-chats-cli autodownload-archive\n"
		<< "  exteraless-chats-cli autodownload-archive-set <0|1>\n"
		<< "  exteraless-chats-cli win32-exe\n"
		<< "  exteraless-chats-cli win32-exe-set <0|1>\n"
		<< "  exteraless-chats-cli greeting-sticker\n"
		<< "  exteraless-chats-cli greeting-sticker-set <0|1>\n"
		<< "  exteraless-chats-cli panels-on-scroll\n"
		<< "  exteraless-chats-cli panels-on-scroll-set <0|1>\n"
		<< "  exteraless-chats-cli channel-bottom-button\n"
		<< "  exteraless-chats-cli channel-bottom-button-set <0..2>\n"
		<< "  exteraless-chats-cli download-boost\n"
		<< "  exteraless-chats-cli download-boost-set <0..2>\n"
		<< "  exteraless-chats-cli upload-boost\n"
		<< "  exteraless-chats-cli upload-boost-set <0|1>\n"
		<< "  exteraless-chats-cli wide-posts\n"
		<< "  exteraless-chats-cli wide-posts-set <0|1>\n"
		<< "  exteraless-chats-cli regex-filters\n"
		<< "  exteraless-chats-cli regex-filter-set <pattern> <ci 0|1> "
		<< "<reversed 0|1>\n"
		<< "  exteraless-chats-cli regex-filter-list\n"
		<< "  exteraless-chats-cli regex-filter-clear\n"
		<< "  exteraless-chats-cli regex-test <text>\n"
		<< "  exteraless-chats-cli sort-unread\n"
		<< "  exteraless-chats-cli sort-unread-set <0|1>\n"
		<< "  exteraless-chats-cli archive-skip <inMainMenu> <hideArchive>\n"
		<< "  exteraless-chats-cli always-show-download\n"
		<< "  exteraless-chats-cli always-show-download-set <0|1>\n"
		<< "  exteraless-chats-cli show-results-before-voting\n"
		<< "  exteraless-chats-cli show-results-before-voting-set <0|1>\n"
		<< "  exteraless-chats-cli reply-style\n"
		<< "  exteraless-chats-cli reply-style-set <colors> <emoji> "
		<< "<background>\n"
		<< "  exteraless-chats-cli false-bottom\n"
		<< "  exteraless-chats-cli false-bottom-set <0|1>\n"
		<< "  exteraless-chats-cli decoration\n"
		<< "  exteraless-chats-cli decoration-set <0|1|2|3>\n"
		<< "  exteraless-chats-cli shortcuts\n"
		<< "  exteraless-chats-cli shortcuts-set <admins> <recent> "
		<< "<stats> <perms> <members>\n"
		<< "  exteraless-chats-cli message-menu-style\n"
		<< "  exteraless-chats-cli message-menu-style-set <grouped> "
		<< "<defaultDelete>\n"
		<< "  exteraless-chats-cli chat-menu\n"
		<< "  exteraless-chats-cli chat-menu-set <boost> <linked> "
		<< "<viewDeleted> <clearDeleted>\n"
		<< "  exteraless-chats-cli media-menu\n"
		<< "  exteraless-chats-cli media-menu-set <forward> <copyFrame> "
		<< "<copyPhoto> <setProfile>\n"
		<< "  exteraless-chats-cli media-quality\n"
		<< "  exteraless-chats-cli media-quality-set <0..2>\n"
		<< "  exteraless-chats-cli hw-accel\n"
		<< "  exteraless-chats-cli hw-accel-set <0|1>\n"
		<< "  exteraless-chats-cli gif-as-video\n"
		<< "  exteraless-chats-cli gif-as-video-set <0|1>\n"
		<< "  exteraless-chats-cli audio-bitrate\n"
		<< "  exteraless-chats-cli audio-bitrate-set <16|24|32|48>\n"
		<< "  exteraless-chats-cli update-channel\n"
		<< "  exteraless-chats-cli update-channel-set <0..2>\n"
		<< "  exteraless-chats-cli update-check <json> <current> "
		<< "<prerelease 0|1>\n"
		<< "  exteraless-chats-cli translation-provider\n"
		<< "  exteraless-chats-cli translation-provider-set <0..3>\n"
		<< "  exteraless-chats-cli translation-keys\n"
		<< "  exteraless-chats-cli translation-keys-set <google|deepl> "
		<< "<key>\n"
		<< "  exteraless-chats-cli translation-formality\n"
		<< "  exteraless-chats-cli translation-formality-set <0..2>\n"
		<< "  exteraless-chats-cli recognition-language\n"
		<< "  exteraless-chats-cli recognition-language-set <code>\n"
		<< "  exteraless-chats-cli ai-config\n"
		<< "  exteraless-chats-cli ai-config-set-endpoint <url>\n"
		<< "  exteraless-chats-cli ai-config-set-model <model>\n"
		<< "  exteraless-chats-cli ai-config-set-key <key>\n"
		<< "  exteraless-chats-cli ai-request-json <prompt>\n"
		<< "  exteraless-chats-cli ai-stream-parse <file>\n"
		<< "  exteraless-chats-cli ai-stream-chunk <file> <bytes>\n"
		<< "  exteraless-chats-cli forward-date\n"
		<< "  exteraless-chats-cli forward-date-set <0|1>\n"
		<< "  exteraless-chats-cli sender-mini-avatars\n"
		<< "  exteraless-chats-cli sender-mini-avatars-set <0|1>\n"
		<< "  exteraless-chats-cli edited-mark\n"
		<< "  exteraless-chats-cli edited-mark-set <icon> <mark>\n"
		<< "  exteraless-chats-cli seek-duration\n"
		<< "  exteraless-chats-cli seek-duration-set <seconds>\n"
		<< "  exteraless-chats-cli sticker-size\n"
		<< "  exteraless-chats-cli sticker-size-set <4..20>\n"
		<< "  exteraless-chats-cli sticker-shape\n"
		<< "  exteraless-chats-cli sticker-shape-set <0..2>\n"
		<< "  exteraless-chats-cli avatar-corners\n"
		<< "  exteraless-chats-cli avatar-corners-set <-1..32>\n"
		<< "  exteraless-chats-cli header-style\n"
		<< "  exteraless-chats-cli header-style-set <0..2>\n"
		<< "  exteraless-chats-cli section-radius\n"
		<< "  exteraless-chats-cli section-radius-set <-1..16>\n"
		<< "  exteraless-chats-cli separate-headers\n"
		<< "  exteraless-chats-cli separate-headers-set <0|1>\n"
		<< "  exteraless-chats-cli save-subfolder\n"
		<< "  exteraless-chats-cli save-subfolder-set <0|1>\n"
		<< "  exteraless-chats-cli chat-folder <name>\n"
		<< "  exteraless-chats-cli chat-subfolder <path> <peerName>\n"
		<< "  exteraless-chats-cli inline-math\n"
		<< "  exteraless-chats-cli inline-math-set <0|1>\n"
		<< "  exteraless-chats-cli math-eval <expression>\n"
		<< "  exteraless-chats-cli send-as\n"
		<< "  exteraless-chats-cli send-as-set <0|1>\n"
		<< "  exteraless-chats-cli profile-buttons\n"
		<< "  exteraless-chats-cli profile-buttons-set <gift> <call> "
		<< "<search>\n"
		<< "  exteraless-chats-cli storiesheader\n"
		<< "  exteraless-chats-cli storiesheader-set <0|1>\n"
		<< "  exteraless-chats-cli plugin-filename <name>\n"
		<< "  exteraless-chats-cli plugin-install <source> <directory>\n"
		<< "  exteraless-chats-cli plugin-delete <directory> <file>\n"
		<< "  exteraless-chats-cli deleted\n"
		<< "  exteraless-chats-cli deleted-set <0|1>\n"
		<< "  exteraless-chats-cli deleted-trim [<maxEntries> <maxPerPeer>] "
		<< "<json>\n"
		<< "  exteraless-chats-cli deleted-entry <json>\n"
		<< "  exteraless-chats-cli backup-export <removeTail> "
		<< "<hideChannels> <hideGroups> <hidePrivate> <stripTracking> "
		<< "<stripTrackingPaste> <unreadCapsule>\n"
		<< "  exteraless-chats-cli backup-export-current\n"
		<< "  exteraless-chats-cli backup-import <base64>\n"
		<< "  exteraless-chats-cli backup-import-json <json>\n"
		<< "  exteraless-chats-cli subtitles-parse <file>\n"
		<< "  exteraless-chats-cli subtitles-active <file> <pos>\n"
		<< "  exteraless-chats-cli subtitles-key <docId>\n"
		<< "  exteraless-chats-cli lyrics-parse <file>\n"
		<< "  exteraless-chats-cli lyrics-active <file> <posMs>\n"
		<< "  exteraless-chats-cli lyrics-search-parse <file>\n"
		<< "  exteraless-chats-cli lyrics-get-parse <file>\n"
		<< "  exteraless-chats-cli network\n"
		<< "  exteraless-chats-cli network-set-ipv6 <0|1>\n"
		<< "  exteraless-chats-cli network-set-dns <0|1>\n"
		<< "  exteraless-chats-cli network-set-doh <url|->\n";
	return 2;
}

[[nodiscard]] bool ReadFlag(const QString &text, bool *value) {
	if (text == QStringLiteral("0")) {
		*value = false;
		return true;
	} else if (text == QStringLiteral("1")) {
		*value = true;
		return true;
	}
	return false;
}

[[nodiscard]] bool ReadFlags(
		const QStringList &arguments,
		int first,
		std::array<bool, 7> *flags) {
	for (auto i = 0; i != int(flags->size()); ++i) {
		if (!ReadFlag(arguments[first + i], &(*flags)[i])) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] QJsonArray ChatsArray(const ChatsSettings &chats) {
	return QJsonArray{
		chats.removeMessageTail,
		chats.hideReactionsChannels,
		chats.hideReactionsGroups,
		chats.hideReactionsPrivate,
		chats.stripTrackingOnOpen,
		chats.stripTrackingOnPaste,
		chats.unreadCapsule,
	};
}

[[nodiscard]] QJsonArray FiltersArray(
		const QList<Exteraless::Filter> &filters) {
	auto result = QJsonArray();
	for (const auto &filter : filters) {
		result.append(QJsonObject{
			{ QStringLiteral("regex"), filter.regex },
			{ QStringLiteral("caseInsensitive"),
				filter.caseInsensitive },
			{ QStringLiteral("reversed"), filter.reversed },
			{ QStringLiteral("enabled"), filter.enabled },
		});
	}
	return result;
}

void PrintJson(const QJsonObject &root) {
	const auto bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
	std::cout.write(bytes.constData(), bytes.size());
	std::cout << '\n';
}

[[nodiscard]] bool ReadFileBytes(const QString &path, QByteArray *bytes) {
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}
	*bytes = file.readAll();
	return true;
}

void PrintChats() {
	PrintJson(QJsonObject{
		{ QStringLiteral("chats"),
			ChatsArray(Exteraless::Config::Instance().chatsSettings()) },
	});
}

void PrintGeneral() {
	PrintJson(QJsonObject{
		{ QStringLiteral("disableNumberRounding"),
			Exteraless::Config::Instance().disableNumberRounding() },
		{ QStringLiteral("showSeconds"),
			Exteraless::Config::Instance().showSeconds() },
		{ QStringLiteral("hidePhone"),
			Exteraless::Config::Instance().hidePhone() },
	});
}

[[nodiscard]] QString MaskTranslationKey(const QString &key) {
	if (key.isEmpty()) {
		return QString();
	}
	return (key.size() > 4 ? key.left(4) : QString())
		+ QStringLiteral("****");
}

void PrintTranslationProvider() {
	PrintJson(QJsonObject{
		{ QStringLiteral("provider"),
			Exteraless::Config::Instance().translationProvider() },
	});
}

void PrintTranslationKeys() {
	const auto &config = Exteraless::Config::Instance();
	PrintJson(QJsonObject{
		{ QStringLiteral("google"),
			MaskTranslationKey(config.translationGoogleCloudKey()) },
		{ QStringLiteral("deepl"),
			MaskTranslationKey(config.translationDeepLKey()) },
	});
}

void PrintTranslationFormality() {
	PrintJson(QJsonObject{
		{ QStringLiteral("formality"),
			Exteraless::Config::Instance().translationDeepLFormality() },
	});
}

void PrintRecognitionLanguage() {
	PrintJson(QJsonObject{
		{ QStringLiteral("language"),
			Exteraless::Config::Instance().recognitionLanguage() },
	});
}

void PrintAiConfig() {
	const auto &config = Exteraless::Config::Instance();
	PrintJson(QJsonObject{
		{ QStringLiteral("endpoint"), config.aiEndpoint() },
		{ QStringLiteral("model"), config.aiModel() },
		{ QStringLiteral("hasKey"), !config.aiApiKey().isEmpty() },
	});
}

void PrintNetwork() {
	const auto &config = Exteraless::Config::Instance();
	PrintJson(QJsonObject{
		{ QStringLiteral("useIPv6"), config.networkUseIPv6() },
		{ QStringLiteral("dnsType"), config.networkDnsType() },
		{ QStringLiteral("customDoh"), config.networkCustomDoH() },
	});
}

void PrintPersianCalendar() {
	const auto &config = Exteraless::Config::Instance();
	PrintJson(QJsonObject{
		{ QStringLiteral("usePersianCalendar"),
			config.usePersianCalendar() },
		{ QStringLiteral("displayPersianCalendarByLatin"),
			config.displayPersianCalendarByLatin() },
	});
}

void PrintBotPrivacy() {
	const auto &config = Exteraless::Config::Instance();
	PrintJson(QJsonObject{
		{ QStringLiteral("enabled"), config.botPrivacyEnabled() },
		{ QStringLiteral("account"), config.botPrivacyAccount() },
		{ QStringLiteral("alwaysAsk"), config.botPrivacyAlwaysAsk() },
	});
}

void PrintDeleted() {
	PrintJson(QJsonObject{
		{ QStringLiteral("saveDeletedMessages"),
			Exteraless::Config::Instance().saveDeletedMessages() },
	});
}

[[nodiscard]] QJsonObject MessageMenuObject(
		const Exteraless::MessageMenuSettings &menu) {
	return QJsonObject{
		{ QStringLiteral("reply"), menu.reply },
		{ QStringLiteral("edit"), menu.edit },
		{ QStringLiteral("pin"), menu.pin },
		{ QStringLiteral("copyText"), menu.copyText },
		{ QStringLiteral("copyLink"), menu.copyLink },
		{ QStringLiteral("copyImage"), menu.copyImage },
		{ QStringLiteral("forward"), menu.forward },
		{ QStringLiteral("select"), menu.select },
		{ QStringLiteral("translate"), menu.translate },
		{ QStringLiteral("report"), menu.report },
		{ QStringLiteral("deleteMsg"), menu.deleteMsg },
	};
}

void PrintMessageMenu() {
	PrintJson(QJsonObject{
		{ QStringLiteral("menu"),
			MessageMenuObject(
				Exteraless::Config::Instance().messageMenuSettings()) },
	});
}

void ApplyMenuFlags(const std::array<bool, 11> &flags) {
	auto &config = Exteraless::Config::Instance();
	config.setChatsMenuReply(flags[0]);
	config.setChatsMenuEdit(flags[1]);
	config.setChatsMenuPin(flags[2]);
	config.setChatsMenuCopyText(flags[3]);
	config.setChatsMenuCopyLink(flags[4]);
	config.setChatsMenuCopyImage(flags[5]);
	config.setChatsMenuForward(flags[6]);
	config.setChatsMenuSelect(flags[7]);
	config.setChatsMenuTranslate(flags[8]);
	config.setChatsMenuReport(flags[9]);
	config.setChatsMenuDelete(flags[10]);
}

void PrintDoubleTap() {
	const auto &config = Exteraless::Config::Instance();
	PrintJson(QJsonObject{
		{ QStringLiteral("in"), config.chatsDoubleTapIn() },
		{ QStringLiteral("out"), config.chatsDoubleTapOut() },
	});
}

void PrintDeletedMark() {
	const auto &config = Exteraless::Config::Instance();
	PrintJson(QJsonObject{
		{ QStringLiteral("translucent"),
			config.translucentDeletedMessages() },
		{ QStringLiteral("icon"), config.useDeletedIcon() },
		{ QStringLiteral("mark"), config.customDeletedMark() },
		{ QStringLiteral("color"), config.deletedMarkColor() },
	});
}

void PrintGhost() {
	const auto &config = Exteraless::Config::Instance();
	PrintJson(QJsonObject{
		{ QStringLiteral("read"), config.ghostSendReadPackets() },
		{ QStringLiteral("stories"), config.ghostSendReadStories() },
		{ QStringLiteral("online"), config.ghostSendOnline() },
		{ QStringLiteral("typing"), config.ghostSendUploadProgress() },
		{ QStringLiteral("active"), Exteraless::IsGhostActive() },
		{ QStringLiteral("partial"), Exteraless::IsGhostPartiallyActive() },
		{ QStringLiteral("count"), Exteraless::GhostEnabledCount() },
		{ QStringLiteral("showStatus"), config.ghostShowStatus() },
		{ QStringLiteral("readLocked"), config.ghostSendReadLocked() },
		{ QStringLiteral("storiesLocked"), config.ghostSendStoriesLocked() },
		{ QStringLiteral("onlineLocked"), config.ghostSendOnlineLocked() },
		{ QStringLiteral("typingLocked"),
			config.ghostSendUploadProgressLocked() },
		{ QStringLiteral("unlocked"), Exteraless::GhostUnlockedCount() },
		{ QStringLiteral("scheduledSend"),
			config.ghostScheduledSend() },
	});
}

void PrintPillOrder() {
	const auto order = Exteraless::Config::Instance().pillStackOrder();
	PrintJson(QJsonObject{
		{ QStringLiteral("order"),
			QJsonArray::fromStringList(order.split(',')) },
	});
}

void PrintStickerTime() {
	const auto chats = Exteraless::Config::Instance().chatsSettings();
	PrintJson(QJsonObject{
		{ QStringLiteral("side"), chats.stickerTimeSide },
		{ QStringLiteral("hidden"), chats.hideStickerTime },
	});
}

[[nodiscard]] QString DoubleTapName(int value) {
	const auto action = Exteraless::ResolveDoubleTap(value);
	if (!action) {
		return QStringLiteral("telegram");
	}
	switch (*action) {
	case Exteraless::DoubleTapAction::None:
		return QStringLiteral("none");
	case Exteraless::DoubleTapAction::SendReaction:
		return QStringLiteral("reaction");
	case Exteraless::DoubleTapAction::Translate:
		return QStringLiteral("translate");
	case Exteraless::DoubleTapAction::Reply:
		return QStringLiteral("reply");
	case Exteraless::DoubleTapAction::Save:
		return QStringLiteral("save");
	case Exteraless::DoubleTapAction::Repeat:
		return QStringLiteral("repeat");
	case Exteraless::DoubleTapAction::RepeatAsCopy:
		return QStringLiteral("repeatcopy");
	case Exteraless::DoubleTapAction::Edit:
		return QStringLiteral("edit");
	case Exteraless::DoubleTapAction::Delete:
		return QStringLiteral("delete");
	case Exteraless::DoubleTapAction::Forward:
		return QStringLiteral("forward");
	case Exteraless::DoubleTapAction::Download:
		return QStringLiteral("download");
	default:
		break;
	}
	return QStringLiteral("none");
}

void ApplyFlags(const std::array<bool, 7> &flags) {
	auto &config = Exteraless::Config::Instance();
	config.setChatsRemoveMessageTail(flags[0]);
	config.setChatsHideReactionsChannels(flags[1]);
	config.setChatsHideReactionsGroups(flags[2]);
	config.setChatsHideReactionsPrivate(flags[3]);
	config.setChatsStripTrackingOnOpen(flags[4]);
	config.setChatsStripTrackingOnPaste(flags[5]);
	config.setChatsUnreadCapsule(flags[6]);
}

void PrintImportResult(const Exteraless::BackupImportResult &result) {
	PrintJson(QJsonObject{
		{ QStringLiteral("ok"), result.ok },
		{ QStringLiteral("error"), result.error },
		{ QStringLiteral("applied"), result.applied },
		{ QStringLiteral("unknown"),
			QJsonArray::fromStringList(result.unknown) },
		{ QStringLiteral("chats"),
			ChatsArray(Exteraless::Config::Instance().chatsSettings()) },
		{ QStringLiteral("disableNumberRounding"),
			Exteraless::Config::Instance().disableNumberRounding() },
	});
}

[[nodiscard]] QString ToBase64(const QString &text) {
	return QString::fromLatin1(text.toUtf8().toBase64());
}

[[nodiscard]] QString FromBase64(const QString &text) {
	return QString::fromUtf8(QByteArray::fromBase64(text.toLatin1()));
}

void PrintBackup() {
	const auto encoded = Exteraless::BuildBackup();
	const auto document = QJsonDocument::fromJson(
		Exteraless::DecodeInvisible(encoded));
	const auto object = document.object();
	PrintJson(QJsonObject{
		{ QStringLiteral("backup_base64"), ToBase64(encoded) },
		{ QStringLiteral("exteraconfig"),
			object.value(QStringLiteral("exteraconfig")).toObject() },
		{ QStringLiteral("pillstackconfig"),
			object.value(QStringLiteral("pillstackconfig")).toObject() },
		{ QStringLiteral("mainconfig"),
			object.value(QStringLiteral("mainconfig")).toObject() },
	});
}

} // namespace

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral("ExteralessDesktop"));
	const auto workdir = qEnvironmentVariable("EXTERALESS_WORKDIR");
	if (!workdir.isEmpty()) {
		Exteraless::Config::Instance().setWorkingDir(workdir);
	}
	const auto arguments = app.arguments();
	if (arguments.size() < 2) {
		return PrintUsage();
	}
	const auto mode = arguments[1];
	if (mode == QStringLiteral("strip")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		std::cout
			<< Exteraless::StripTracking(arguments[2]).toUtf8().constData()
			<< '\n';
		return 0;
	}
	if (mode == QStringLiteral("strip-text")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		std::cout
			<< Exteraless::StripTrackingInText(arguments[2]).toUtf8().constData()
			<< '\n';
		return 0;
	}
	if (mode == QStringLiteral("hidden")) {
		if (arguments.size() != 8) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 6>();
		for (auto i = 0; i != int(flags.size()); ++i) {
			if (!ReadFlag(arguments[2 + i], &flags[i])) {
				return PrintUsage();
			}
		}
		auto settings = Exteraless::ChatsSettings();
		settings.hideReactionsChannels = flags[0];
		settings.hideReactionsGroups = flags[1];
		settings.hideReactionsPrivate = flags[2];
		std::cout << (Exteraless::HiddenReactionsFor(
			settings,
			flags[3],
			flags[4],
			flags[5]) ? "hidden" : "shown") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("config-defaults")
		|| mode == QStringLiteral("config-loaded")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintChats();
		return 0;
	}
	if (mode == QStringLiteral("general")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintGeneral();
		return 0;
	}
	if (mode == QStringLiteral("general-set")) {
		if (arguments.size() < 3 || arguments.size() > 5) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setDisableNumberRounding(value);
		if (arguments.size() >= 4) {
			auto seconds = false;
			if (!ReadFlag(arguments[3], &seconds)) {
				return PrintUsage();
			}
			Exteraless::Config::Instance().setShowSeconds(seconds);
		}
		if (arguments.size() == 5) {
			auto hide = false;
			if (!ReadFlag(arguments[4], &hide)) {
				return PrintUsage();
			}
			Exteraless::Config::Instance().setHidePhone(hide);
		}
		PrintGeneral();
		return 0;
	}
	if (mode == QStringLiteral("bot-privacy")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintBotPrivacy();
		return 0;
	}
	if (mode == QStringLiteral("bot-privacy-set")) {
		if (arguments.size() != 5) {
			return PrintUsage();
		}
		auto enabled = false;
		auto ask = false;
		auto ok = false;
		const auto account = arguments[3].toInt(&ok);
		if (!ReadFlag(arguments[2], &enabled)
			|| !ok
			|| !ReadFlag(arguments[4], &ask)) {
			return PrintUsage();
		}
		auto &config = Exteraless::Config::Instance();
		config.setBotPrivacyEnabled(enabled);
		config.setBotPrivacyAccount(account);
		config.setBotPrivacyAlwaysAsk(ask);
		PrintBotPrivacy();
		return 0;
	}
	if (mode == QStringLiteral("id-dc")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("showIdDc"),
				Exteraless::Config::Instance().showIdDc() },
		});
		return 0;
	}
	if (mode == QStringLiteral("id-dc-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setShowIdDc(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("showIdDc"),
				Exteraless::Config::Instance().showIdDc() },
		});
		return 0;
	}
	if (mode == QStringLiteral("id-dc-format")) {
		if (arguments.size() != 5) {
			return PrintUsage();
		}
		auto ok = false;
		const auto bareId = arguments[2].toULongLong(&ok);
		if (!ok) {
			return PrintUsage();
		}
		const auto dcId = arguments[3].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		auto enabled = false;
		if (!ReadFlag(arguments[4], &enabled)) {
			return PrintUsage();
		}
		std::cout
			<< Exteraless::FormatIdDc(
				bareId,
				dcId,
				enabled).toUtf8().constData()
			<< '\n';
		return 0;
	}
	if (mode == QStringLiteral("clock")) {
		if (arguments.size() != 6 && arguments.size() != 7) {
			return PrintUsage();
		}
		auto ok = false;
		const auto h = arguments[2].toInt(&ok);
		if (!ok || h < 0 || h > 23) return PrintUsage();
		const auto m = arguments[3].toInt(&ok);
		if (!ok || m < 0 || m > 59) return PrintUsage();
		const auto s = arguments[4].toInt(&ok);
		if (!ok || s < 0 || s > 59) return PrintUsage();
		auto withSeconds = false;
		if (!ReadFlag(arguments[5], &withSeconds)) return PrintUsage();
		const auto locale = (arguments.size() == 7)
			? QLocale(arguments[6])
			: QLocale();
		std::cout
			<< Exteraless::FormatClock(QTime(h, m, s), withSeconds, locale)
				.toUtf8()
				.constData()
			<< '\n';
		return 0;
	}
	if (mode == QStringLiteral("persian-date")) {
		if (arguments.size() != 5) {
			return PrintUsage();
		}
		auto latin = false;
		auto withYear = false;
		if (!ReadFlag(arguments[3], &latin)
			|| !ReadFlag(arguments[4], &withYear)) {
			return PrintUsage();
		}
		const auto date = QDate::fromString(
			arguments[2],
			QStringLiteral("yyyy-MM-dd"));
		PrintJson(QJsonObject{
			{ QStringLiteral("text"),
				Exteraless::PersianDateText(date, latin, withYear) },
		});
		return 0;
	}
	if (mode == QStringLiteral("persian-calendar")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintPersianCalendar();
		return 0;
	}
	if (mode == QStringLiteral("persian-calendar-set")) {
		if (arguments.size() != 3 && arguments.size() != 4) {
			return PrintUsage();
		}
		auto enabled = false;
		if (!ReadFlag(arguments[2], &enabled)) {
			return PrintUsage();
		}
		auto &config = Exteraless::Config::Instance();
		config.setUsePersianCalendar(enabled);
		if (arguments.size() == 4) {
			auto latin = false;
			if (!ReadFlag(arguments[3], &latin)) {
				return PrintUsage();
			}
			config.setDisplayPersianCalendarByLatin(latin);
		}
		PrintPersianCalendar();
		return 0;
	}
	if (mode == QStringLiteral("message-menu")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintMessageMenu();
		return 0;
	}
	if (mode == QStringLiteral("message-menu-set")) {
		if (arguments.size() != 13) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 11>();
		for (auto i = 0; i != 11; ++i) {
			if (!ReadFlag(arguments[2 + i], &flags[i])) {
				return PrintUsage();
			}
		}
		ApplyMenuFlags(flags);
		PrintMessageMenu();
		return 0;
	}
	if (mode == QStringLiteral("doubletap")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintDoubleTap();
		return 0;
	}
	if (mode == QStringLiteral("doubletap-set")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		auto ok = false;
		const auto in = arguments[2].toInt(&ok);
		if (!ok) return PrintUsage();
		const auto out = arguments[3].toInt(&ok);
		if (!ok) return PrintUsage();
		auto &config = Exteraless::Config::Instance();
		config.setChatsDoubleTapIn(in);
		config.setChatsDoubleTapOut(out);
		PrintDoubleTap();
		return 0;
	}
	if (mode == QStringLiteral("doubletap-resolve")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		std::cout << DoubleTapName(value).toUtf8().constData() << '\n';
		return 0;
	}
	if (mode == QStringLiteral("stickertime")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintStickerTime();
		return 0;
	}
	if (mode == QStringLiteral("disable-link-preview")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("disabled"),
				Exteraless::Config::Instance()
					.disableLinkPreviewByDefault() },
		});
		return 0;
	}
	if (mode == QStringLiteral("disable-link-preview-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance()
			.setDisableLinkPreviewByDefault(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("hide-all-chats")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("hidden"),
				Exteraless::Config::Instance().hideAllChatsFolder() },
		});
		return 0;
	}
	if (mode == QStringLiteral("hide-all-chats-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setHideAllChatsFolder(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("hide-archive")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("hidden"),
				Exteraless::Config::Instance().chatsSettings().hideArchive },
		});
		return 0;
	}
	if (mode == QStringLiteral("hide-archive-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setChatsHideArchive(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("autodownload-archive")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("disabled"),
				Exteraless::Config::Instance()
					.chatsSettings().disableAutoDownloadingArchive },
		});
		return 0;
	}
	if (mode == QStringLiteral("autodownload-archive-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setDisableAutoDownloadingArchive(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("win32-exe")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("blocked"),
				Exteraless::Config::Instance()
					.chatsSettings().blockWin32Executables },
		});
		return 0;
	}
	if (mode == QStringLiteral("win32-exe-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setBlockWin32Executables(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("greeting-sticker")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("hidden"),
				Exteraless::Config::Instance()
					.chatsSettings().hideGreetingSticker },
		});
		return 0;
	}
	if (mode == QStringLiteral("greeting-sticker-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setChatsHideGreetingSticker(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("hidden"),
				Exteraless::Config::Instance()
					.chatsSettings().hideGreetingSticker },
		});
		return 0;
	}
	if (mode == QStringLiteral("panels-on-scroll")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("hidden"),
				Exteraless::Config::Instance()
					.chatsSettings().hidePanelsOnScroll },
		});
		return 0;
	}
	if (mode == QStringLiteral("panels-on-scroll-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setChatsHidePanelsOnScroll(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("hidden"),
				Exteraless::Config::Instance()
					.chatsSettings().hidePanelsOnScroll },
		});
		return 0;
	}
	if (mode == QStringLiteral("channel-bottom-button")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("mode"),
				Exteraless::Config::Instance()
					.chatsSettings().channelBottomButton },
		});
		return 0;
	}
	if (mode == QStringLiteral("channel-bottom-button-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok || value < 0 || value > 2) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setChatsChannelBottomButton(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("mode"),
				Exteraless::Config::Instance()
					.chatsSettings().channelBottomButton },
		});
		return 0;
	}
	if (mode == QStringLiteral("download-boost")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("mode"),
				Exteraless::Config::Instance()
					.chatsSettings().downloadSpeedBoost },
		});
		return 0;
	}
	if (mode == QStringLiteral("download-boost-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setChatsDownloadSpeedBoost(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("mode"),
				Exteraless::Config::Instance()
					.chatsSettings().downloadSpeedBoost },
		});
		return 0;
	}
	if (mode == QStringLiteral("upload-boost")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance()
					.chatsSettings().uploadBoost },
		});
		return 0;
	}
	if (mode == QStringLiteral("upload-boost-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setChatsUploadBoost(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("wide-posts")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("wideChannelPosts"),
				Exteraless::Config::Instance()
					.chatsSettings().wideChannelPosts },
		});
		return 0;
	}
	if (mode == QStringLiteral("wide-posts-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setChatsWideChannelPosts(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("regex-filters")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		const auto &config = Exteraless::Config::Instance();
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"), config.regexFiltersEnabled() },
			{ QStringLiteral("count"),
				int(Exteraless::ParseFilters(
					config.regexFiltersData()).size()) },
		});
		return 0;
	}
	if (mode == QStringLiteral("regex-filter-set")) {
		if (arguments.size() != 5) {
			return PrintUsage();
		}
		auto caseInsensitive = false;
		auto reversed = false;
		if (!ReadFlag(arguments[3], &caseInsensitive)
			|| !ReadFlag(arguments[4], &reversed)) {
			return PrintUsage();
		}
		const auto pattern = arguments[2];
		if (pattern.isEmpty()
			|| !QRegularExpression(pattern).isValid()) {
			return PrintUsage();
		}
		auto filters = Exteraless::ParseFilters(
			Exteraless::Config::Instance().regexFiltersData());
		auto filter = Exteraless::Filter();
		filter.regex = pattern;
		filter.caseInsensitive = caseInsensitive;
		filter.reversed = reversed;
		filters.push_back(filter);
		Exteraless::Config::Instance().setRegexFiltersData(
			QString::fromUtf8(QJsonDocument(
				FiltersArray(filters)).toJson(QJsonDocument::Compact)));
		PrintJson(QJsonObject{
			{ QStringLiteral("count"), int(filters.size()) },
		});
		return 0;
	}
	if (mode == QStringLiteral("regex-filter-list")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		const auto filters = Exteraless::ParseFilters(
			Exteraless::Config::Instance().regexFiltersData());
		PrintJson(QJsonObject{
			{ QStringLiteral("filters"), FiltersArray(filters) },
			{ QStringLiteral("count"), int(filters.size()) },
		});
		return 0;
	}
	if (mode == QStringLiteral("regex-filter-clear")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setRegexFiltersData(
			QStringLiteral("[]"));
		PrintJson(QJsonObject{
			{ QStringLiteral("count"), 0 },
		});
		return 0;
	}
	if (mode == QStringLiteral("regex-test")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		const auto &config = Exteraless::Config::Instance();
		const auto hidden = config.regexFiltersEnabled()
			&& !config.regexFiltersShowHidden()
			&& Exteraless::MatchesText(
				Exteraless::CompileFilters(
					Exteraless::ParseFilters(
						config.regexFiltersData())),
				arguments[2]);
		PrintJson(QJsonObject{
			{ QStringLiteral("hidden"), hidden },
		});
		return 0;
	}
	if (mode == QStringLiteral("sort-unread")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("sorted"),
				Exteraless::Config::Instance().chatsSettings().sortByUnread },
		});
		return 0;
	}
	if (mode == QStringLiteral("sort-unread-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setChatsSortByUnread(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("archive-skip")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		auto inMainMenu = false;
		auto hideArchive = false;
		if (!ReadFlag(arguments[2], &inMainMenu)
			|| !ReadFlag(arguments[3], &hideArchive)) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("skip"),
				Exteraless::ArchiveRowSkipped(inMainMenu, hideArchive) },
		});
		return 0;
	}
	if (mode == QStringLiteral("always-show-download")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().alwaysShowDownloadIcon() },
		});
		return 0;
	}
	if (mode == QStringLiteral("always-show-download-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setAlwaysShowDownloadIcon(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("show-results-before-voting")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance()
					.showResultsBeforeVoting() },
		});
		return 0;
	}
	if (mode == QStringLiteral("show-results-before-voting-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setShowResultsBeforeVoting(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("reply-style")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		const auto &config = Exteraless::Config::Instance();
		PrintJson(QJsonObject{
			{ QStringLiteral("colors"), config.replyColors() },
			{ QStringLiteral("emoji"), config.replyEmoji() },
			{ QStringLiteral("background"), config.replyBackground() },
		});
		return 0;
	}
	if (mode == QStringLiteral("reply-style-set")) {
		if (arguments.size() != 5) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 3>();
		for (auto i = 0; i != 3; ++i) {
			if (!ReadFlag(arguments[2 + i], &flags[i])) {
				return PrintUsage();
			}
		}
		auto &config = Exteraless::Config::Instance();
		config.setChatsReplyColors(flags[0]);
		config.setChatsReplyEmoji(flags[1]);
		config.setChatsReplyBackground(flags[2]);
		PrintJson(QJsonObject{
			{ QStringLiteral("colors"), config.replyColors() },
			{ QStringLiteral("emoji"), config.replyEmoji() },
			{ QStringLiteral("background"), config.replyBackground() },
		});
		return 0;
	}
	if (mode == QStringLiteral("false-bottom")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().falseBottom() },
		});
		return 0;
	}
	if (mode == QStringLiteral("false-bottom-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setFalseBottom(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("decoration")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("value"),
				Exteraless::Config::Instance().decoration() },
		});
		return 0;
	}
	if (mode == QStringLiteral("decoration-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		const auto value = arguments[2].toInt();
		if (value < 0 || value > 3) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setDecoration(value);
		std::cout << value << '\n';
		return 0;
	}
	if (mode == QStringLiteral("shortcuts")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		const auto &config = Exteraless::Config::Instance();
		PrintJson(QJsonObject{
			{ QStringLiteral("administrators"),
				config.chatShortcutAdministrators() },
			{ QStringLiteral("recentActions"),
				config.chatShortcutRecentActions() },
			{ QStringLiteral("statistics"),
				config.chatShortcutStatistics() },
			{ QStringLiteral("permissions"),
				config.chatShortcutPermissions() },
			{ QStringLiteral("members"),
				config.chatShortcutMembers() },
		});
		return 0;
	}
	if (mode == QStringLiteral("shortcuts-set")) {
		if (arguments.size() != 7) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 5>();
		for (auto i = 0; i != 5; ++i) {
			if (!ReadFlag(arguments[2 + i], &flags[i])) {
				return PrintUsage();
			}
		}
		auto &config = Exteraless::Config::Instance();
		config.setChatShortcutAdministrators(flags[0]);
		config.setChatShortcutRecentActions(flags[1]);
		config.setChatShortcutStatistics(flags[2]);
		config.setChatShortcutPermissions(flags[3]);
		config.setChatShortcutMembers(flags[4]);
		PrintJson(QJsonObject{
			{ QStringLiteral("administrators"),
				config.chatShortcutAdministrators() },
			{ QStringLiteral("recentActions"),
				config.chatShortcutRecentActions() },
			{ QStringLiteral("statistics"),
				config.chatShortcutStatistics() },
			{ QStringLiteral("permissions"),
				config.chatShortcutPermissions() },
			{ QStringLiteral("members"),
				config.chatShortcutMembers() },
		});
		return 0;
	}
	if (mode == QStringLiteral("message-menu-style")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		const auto &config = Exteraless::Config::Instance();
		PrintJson(QJsonObject{
			{ QStringLiteral("grouped"), config.groupedMessageMenu() },
			{ QStringLiteral("defaultDelete"),
				config.defaultDeleteMenu() },
		});
		return 0;
	}
	if (mode == QStringLiteral("message-menu-style-set")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		auto grouped = false;
		auto defaultDelete = false;
		if (!ReadFlag(arguments[2], &grouped)
			|| !ReadFlag(arguments[3], &defaultDelete)) {
			return PrintUsage();
		}
		auto &config = Exteraless::Config::Instance();
		config.setGroupedMessageMenu(grouped);
		config.setDefaultDeleteMenu(defaultDelete);
		PrintJson(QJsonObject{
			{ QStringLiteral("grouped"), config.groupedMessageMenu() },
			{ QStringLiteral("defaultDelete"),
				config.defaultDeleteMenu() },
		});
		return 0;
	}
	if (mode == QStringLiteral("chat-menu")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		const auto &config = Exteraless::Config::Instance();
		PrintJson(QJsonObject{
			{ QStringLiteral("boost"), config.chatMenuBoost() },
			{ QStringLiteral("linked"), config.chatMenuLinked() },
			{ QStringLiteral("viewDeleted"),
				config.chatMenuViewDeleted() },
			{ QStringLiteral("clearDeleted"),
				config.chatMenuClearDeleted() },
		});
		return 0;
	}
	if (mode == QStringLiteral("chat-menu-set")) {
		if (arguments.size() != 6) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 4>();
		for (auto i = 0; i != 4; ++i) {
			if (!ReadFlag(arguments[2 + i], &flags[i])) {
				return PrintUsage();
			}
		}
		auto &config = Exteraless::Config::Instance();
		config.setChatMenuBoost(flags[0]);
		config.setChatMenuLinked(flags[1]);
		config.setChatMenuViewDeleted(flags[2]);
		config.setChatMenuClearDeleted(flags[3]);
		PrintJson(QJsonObject{
			{ QStringLiteral("boost"), config.chatMenuBoost() },
			{ QStringLiteral("linked"), config.chatMenuLinked() },
			{ QStringLiteral("viewDeleted"),
				config.chatMenuViewDeleted() },
			{ QStringLiteral("clearDeleted"),
				config.chatMenuClearDeleted() },
		});
		return 0;
	}
	if (mode == QStringLiteral("media-menu")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		const auto &config = Exteraless::Config::Instance();
		PrintJson(QJsonObject{
			{ QStringLiteral("forward"), config.mediaViewerMenuForward() },
			{ QStringLiteral("copyFrame"), config.mediaViewerMenuCopyFrame() },
			{ QStringLiteral("copyPhoto"), config.mediaViewerMenuCopyPhoto() },
			{ QStringLiteral("setProfile"),
				config.mediaViewerMenuSetProfile() },
		});
		return 0;
	}
	if (mode == QStringLiteral("media-menu-set")) {
		if (arguments.size() != 6) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 4>();
		for (auto i = 0; i != 4; ++i) {
			if (!ReadFlag(arguments[2 + i], &flags[i])) {
				return PrintUsage();
			}
		}
		auto &config = Exteraless::Config::Instance();
		config.setMediaViewerMenuForward(flags[0]);
		config.setMediaViewerMenuCopyFrame(flags[1]);
		config.setMediaViewerMenuCopyPhoto(flags[2]);
		config.setMediaViewerMenuSetProfile(flags[3]);
		PrintJson(QJsonObject{
			{ QStringLiteral("forward"), config.mediaViewerMenuForward() },
			{ QStringLiteral("copyFrame"), config.mediaViewerMenuCopyFrame() },
			{ QStringLiteral("copyPhoto"), config.mediaViewerMenuCopyPhoto() },
			{ QStringLiteral("setProfile"),
				config.mediaViewerMenuSetProfile() },
		});
		return 0;
	}
	if (mode == QStringLiteral("media-quality")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("quality"),
				Exteraless::Config::Instance().defaultVideoQuality() },
		});
		return 0;
	}
	if (mode == QStringLiteral("media-quality-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setDefaultVideoQuality(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("quality"),
				Exteraless::Config::Instance().defaultVideoQuality() },
		});
		return 0;
	}
	if (mode == QStringLiteral("hw-accel")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().hardwareAcceleration() },
		});
		return 0;
	}
	if (mode == QStringLiteral("hw-accel-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setHardwareAcceleration(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().hardwareAcceleration() },
		});
		return 0;
	}
	if (mode == QStringLiteral("gif-as-video")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().takeGifAsVideo() },
		});
		return 0;
	}
	if (mode == QStringLiteral("gif-as-video-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setTakeGifAsVideo(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().takeGifAsVideo() },
		});
		return 0;
	}
	if (mode == QStringLiteral("audio-bitrate")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("bitrate"),
				Exteraless::Config::Instance().customAudioBitrate() },
		});
		return 0;
	}
	if (mode == QStringLiteral("audio-bitrate-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setCustomAudioBitrate(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("bitrate"),
				Exteraless::Config::Instance().customAudioBitrate() },
		});
		return 0;
	}
	if (mode == QStringLiteral("update-channel")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("channel"),
				Exteraless::Config::Instance().autoUpdateChannel() },
		});
		return 0;
	}
	if (mode == QStringLiteral("update-channel-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setAutoUpdateChannel(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("channel"),
				Exteraless::Config::Instance().autoUpdateChannel() },
		});
		return 0;
	}
	if (mode == QStringLiteral("update-check")) {
		if (arguments.size() != 5) {
			return PrintUsage();
		}
		auto withPrerelease = false;
		if (!ReadFlag(arguments[4], &withPrerelease)) {
			return PrintUsage();
		}
		const auto info = Exteraless::ParseReleasesJson(
			arguments[2].toUtf8(),
			arguments[3],
			withPrerelease);
		PrintJson(QJsonObject{
			{ QStringLiteral("available"), info.available },
			{ QStringLiteral("version"), info.version },
			{ QStringLiteral("url"), info.url },
		});
		return 0;
	}
	if (mode == QStringLiteral("translation-provider")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintTranslationProvider();
		return 0;
	}
	if (mode == QStringLiteral("translation-provider-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setTranslationProvider(value);
		PrintTranslationProvider();
		return 0;
	}
	if (mode == QStringLiteral("translation-keys")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintTranslationKeys();
		return 0;
	}
	if (mode == QStringLiteral("translation-keys-set")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		const auto target = arguments[2];
		if (target == QStringLiteral("google")) {
			Exteraless::Config::Instance().setTranslationGoogleCloudKey(
				arguments[3]);
		} else if (target == QStringLiteral("deepl")) {
			Exteraless::Config::Instance().setTranslationDeepLKey(
				arguments[3]);
		} else {
			return PrintUsage();
		}
		PrintTranslationKeys();
		return 0;
	}
	if (mode == QStringLiteral("translation-formality")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintTranslationFormality();
		return 0;
	}
	if (mode == QStringLiteral("translation-formality-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setTranslationDeepLFormality(value);
		PrintTranslationFormality();
		return 0;
	}
	if (mode == QStringLiteral("recognition-language")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintRecognitionLanguage();
		return 0;
	}
	if (mode == QStringLiteral("recognition-language-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		const auto code = arguments[2].trimmed().toLower();
		if (code.isEmpty()) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setRecognitionLanguage(code);
		PrintRecognitionLanguage();
		return 0;
	}
	if (mode == QStringLiteral("ai-config")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintAiConfig();
		return 0;
	}
	if (mode == QStringLiteral("ai-config-set-endpoint")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setAiEndpoint(arguments[2]);
		PrintAiConfig();
		return 0;
	}
	if (mode == QStringLiteral("ai-config-set-model")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setAiModel(arguments[2]);
		PrintAiConfig();
		return 0;
	}
	if (mode == QStringLiteral("ai-config-set-key")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setAiApiKey(arguments[2]);
		PrintAiConfig();
		return 0;
	}
	if (mode == QStringLiteral("ai-request-json")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		const auto &config = Exteraless::Config::Instance();
		const auto body = Exteraless::BuildRequestJson(
			config.aiEndpoint(),
			config.aiModel(),
			Exteraless::AiRequest{ .prompt = arguments[2] });
		PrintJson(QJsonDocument::fromJson(body).object());
		return 0;
	}
	if (mode == QStringLiteral("ai-stream-parse")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto bytes = QByteArray();
		if (!ReadFileBytes(arguments[2], &bytes)) {
			return PrintUsage();
		}
		auto parser = Exteraless::SseParser();
		const auto text = parser.feed(bytes);
		PrintJson(QJsonObject{
			{ QStringLiteral("text"), text },
			{ QStringLiteral("done"), parser.isDone() },
		});
		return 0;
	}
	if (mode == QStringLiteral("ai-stream-chunk")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		auto ok = false;
		const auto chunk = arguments[3].toLongLong(&ok);
		if (!ok || chunk <= 0) {
			return PrintUsage();
		}
		auto bytes = QByteArray();
		if (!ReadFileBytes(arguments[2], &bytes)) {
			return PrintUsage();
		}
		const auto step = qsizetype(chunk);
		auto parser = Exteraless::SseParser();
		auto text = QString();
		for (auto offset = qsizetype(0); offset < bytes.size();
				offset += step) {
			text += parser.feed(bytes.mid(offset, step));
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("text"), text },
			{ QStringLiteral("done"), parser.isDone() },
		});
		return 0;
	}
	if (mode == QStringLiteral("network")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintNetwork();
		return 0;
	}
	if (mode == QStringLiteral("network-set-ipv6")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setNetworkUseIPv6(value);
		PrintNetwork();
		return 0;
	}
	if (mode == QStringLiteral("network-set-dns")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setNetworkDnsType(
			std::clamp(value, 0, 1));
		PrintNetwork();
		return 0;
	}
	if (mode == QStringLiteral("network-set-doh")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setNetworkCustomDoH(
			(arguments[2] == QStringLiteral("-"))
				? QString()
				: arguments[2]);
		PrintNetwork();
		return 0;
	}
	if (mode == QStringLiteral("forward-date")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().showForwardDate() },
		});
		return 0;
	}
	if (mode == QStringLiteral("forward-date-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setShowForwardDate(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("sender-mini-avatars")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().senderMiniAvatars() },
		});
		return 0;
	}
	if (mode == QStringLiteral("sender-mini-avatars-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setSenderMiniAvatars(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("edited-mark")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("icon"),
				Exteraless::Config::Instance().useEditedIcon() },
			{ QStringLiteral("mark"),
				Exteraless::Config::Instance().customEditedMark() },
		});
		return 0;
	}
	if (mode == QStringLiteral("edited-mark-set")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		auto icon = false;
		if (!ReadFlag(arguments[2], &icon)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setUseEditedIcon(icon);
		Exteraless::Config::Instance().setCustomEditedMark(arguments[3]);
		PrintJson(QJsonObject{
			{ QStringLiteral("icon"),
				Exteraless::Config::Instance().useEditedIcon() },
			{ QStringLiteral("mark"),
				Exteraless::Config::Instance().customEditedMark() },
		});
		return 0;
	}
	if (mode == QStringLiteral("seek-duration")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("seconds"),
				Exteraless::Config::Instance().seekDuration() },
		});
		return 0;
	}
	if (mode == QStringLiteral("seek-duration-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setSeekDuration(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("seconds"),
				Exteraless::Config::Instance().seekDuration() },
		});
		return 0;
	}
	if (mode == QStringLiteral("sticker-size")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("stickerSize"),
				Exteraless::Config::Instance().stickerSize() },
		});
		return 0;
	}
	if (mode == QStringLiteral("sticker-size-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setStickerSize(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("stickerSize"),
				Exteraless::Config::Instance().stickerSize() },
		});
		return 0;
	}
	if (mode == QStringLiteral("sticker-shape")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("stickerShape"),
				Exteraless::Config::Instance().stickerShape() },
		});
		return 0;
	}
	if (mode == QStringLiteral("sticker-shape-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setStickerShape(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("stickerShape"),
				Exteraless::Config::Instance().stickerShape() },
		});
		return 0;
	}
	if (mode == QStringLiteral("avatar-corners")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("radius"),
				Exteraless::Config::Instance().avatarRadius() },
		});
		return 0;
	}
	if (mode == QStringLiteral("avatar-corners-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setAvatarRadius(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("radius"),
				Exteraless::Config::Instance().avatarRadius() },
		});
		return 0;
	}
	if (mode == QStringLiteral("header-style")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("style"),
				Exteraless::Config::Instance().chatHeaderStyle() },
		});
		return 0;
	}
	if (mode == QStringLiteral("header-style-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setChatHeaderStyle(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("style"),
				Exteraless::Config::Instance().chatHeaderStyle() },
		});
		return 0;
	}
	if (mode == QStringLiteral("section-radius")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("radius"),
				Exteraless::Config::Instance().sectionRadius() },
		});
		return 0;
	}
	if (mode == QStringLiteral("section-radius-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto value = arguments[2].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setSectionRadius(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("radius"),
				Exteraless::Config::Instance().sectionRadius() },
		});
		return 0;
	}
	if (mode == QStringLiteral("separate-headers")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().sectionSeparateHeaders() },
		});
		return 0;
	}
	if (mode == QStringLiteral("separate-headers-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setSectionSeparateHeaders(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().sectionSeparateHeaders() },
		});
		return 0;
	}
	if (mode == QStringLiteral("save-subfolder")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().saveToChatSubfolder() },
		});
		return 0;
	}
	if (mode == QStringLiteral("save-subfolder-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setSaveToChatSubfolder(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("chat-folder")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		std::cout
			<< Exteraless::SanitizeChatFolderName(arguments[2])
				.toUtf8().constData()
			<< '\n';
		return 0;
	}
	if (mode == QStringLiteral("chat-subfolder")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		std::cout
			<< Exteraless::ChatSubfolderSavePath(
				arguments[2],
				arguments[3]).toUtf8().constData()
			<< '\n';
		return 0;
	}
	if (mode == QStringLiteral("inline-math")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().inlineMathResult() },
		});
		return 0;
	}
	if (mode == QStringLiteral("inline-math-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setInlineMathResult(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("math-eval")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		const auto result = Exteraless::EvaluateMathExpression(arguments[2]);
		std::cout
			<< (result ? *result : QStringLiteral("invalid")).toUtf8().constData()
			<< '\n';
		return 0;
	}
	if (mode == QStringLiteral("send-as")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("hidden"),
				Exteraless::Config::Instance().hideSendAsPeer() },
		});
		return 0;
	}
	if (mode == QStringLiteral("send-as-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setHideSendAsPeer(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("profile-buttons")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		const auto &config = Exteraless::Config::Instance();
		PrintJson(QJsonObject{
			{ QStringLiteral("gift"), config.hideGiftButton() },
			{ QStringLiteral("call"), config.hideCallButton() },
			{ QStringLiteral("search"),
				config.hideChannelSearchButton() },
		});
		return 0;
	}
	if (mode == QStringLiteral("profile-buttons-set")) {
		if (arguments.size() != 5) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 3>();
		for (auto i = 0; i != 3; ++i) {
			if (!ReadFlag(arguments[2 + i], &flags[i])) {
				return PrintUsage();
			}
		}
		auto &config = Exteraless::Config::Instance();
		config.setHideGiftButton(flags[0]);
		config.setHideCallButton(flags[1]);
		config.setHideChannelSearchButton(flags[2]);
		PrintJson(QJsonObject{
			{ QStringLiteral("gift"), config.hideGiftButton() },
			{ QStringLiteral("call"), config.hideCallButton() },
			{ QStringLiteral("search"),
				config.hideChannelSearchButton() },
		});
		return 0;
	}
	if (mode == QStringLiteral("storiesheader")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("storiesHeader"),
				Exteraless::Config::Instance()
					.chatsSettings().hideStoriesHeader },
		});
		return 0;
	}
	if (mode == QStringLiteral("storiesheader-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setChatsHideStoriesHeader(value);
		PrintJson(QJsonObject{
			{ QStringLiteral("storiesHeader"),
				Exteraless::Config::Instance()
					.chatsSettings().hideStoriesHeader },
		});
		return 0;
	}
	if (mode == QStringLiteral("plugin-filename")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		const auto name = Exteraless::Plugins::SanitizePluginFileName(
			arguments[2]);
		std::cout << (name.isEmpty() ? QStringLiteral("invalid") : name).toUtf8().constData()
			<< '\n';
		return 0;
	}
	if (mode == QStringLiteral("plugin-install")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		const auto installed = Exteraless::Plugins::InstallPluginFile(
			arguments[2],
			arguments[3]);
		std::cout << (installed.isEmpty() ? QStringLiteral("failed") : installed)
			.toUtf8().constData() << '\n';
		return 0;
	}
	if (mode == QStringLiteral("plugin-delete")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		const auto removed = Exteraless::Plugins::DeletePluginFile(
			arguments[2],
			arguments[3]);
		std::cout << (removed ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("ai-editor")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("hidden"),
				Exteraless::Config::Instance().hideAiEditor() },
		});
		return 0;
	}
	if (mode == QStringLiteral("ai-editor-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setHideAiEditor(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("settings-sections")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("hidePremium"),
				Exteraless::Config::Instance().hidePremiumSection() },
			{ QStringLiteral("hideHelp"),
				Exteraless::Config::Instance().hideHelpSection() },
		});
		return 0;
	}
	if (mode == QStringLiteral("settings-sections-set")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		auto premium = false;
		auto help = false;
		if (!ReadFlag(arguments[2], &premium)
			|| !ReadFlag(arguments[3], &help)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setHidePremiumSection(premium);
		Exteraless::Config::Instance().setHideHelpSection(help);
		PrintJson(QJsonObject{
			{ QStringLiteral("hidePremium"), premium },
			{ QStringLiteral("hideHelp"), help },
		});
		return 0;
	}
	if (mode == QStringLiteral("silent")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().silentByDefault() },
		});
		return 0;
	}
	if (mode == QStringLiteral("silent-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setSilentByDefault(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("pangu")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().panguOnSending() },
		});
		return 0;
	}
	if (mode == QStringLiteral("pangu-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setPanguOnSending(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("pangu-apply")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		std::cout
			<< Exteraless::ApplyPangu(arguments[2]).toUtf8().constData()
			<< '\n';
		return 0;
	}
	if (mode == QStringLiteral("pangu-tags")) {
		if (arguments.size() != 5) {
			return PrintUsage();
		}
		auto ok = false;
		const auto offset = arguments[3].toInt(&ok);
		if (!ok || offset < 0) {
			return PrintUsage();
		}
		const auto length = arguments[4].toInt(&ok);
		if (!ok || length < 0) {
			return PrintUsage();
		}
		const auto &text = arguments[2];
		const auto pangu = Exteraless::ApplyPanguWithMap(text);
		const auto from = std::clamp(offset, 0, int(text.size()));
		const auto to = std::clamp(offset + length, from, int(text.size()));
		PrintJson(QJsonObject{
			{ QStringLiteral("text"), pangu.text },
			{ QStringLiteral("offset"), pangu.map[from] },
			{ QStringLiteral("length"),
				pangu.map[to] - pangu.map[from] },
		});
		return 0;
	}
	if (mode == QStringLiteral("zalgo")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("zalgoFilter"),
				Exteraless::Config::Instance().zalgoFilter() },
		});
		return 0;
	}
	if (mode == QStringLiteral("zalgo-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setZalgoFilter(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("zalgo-apply")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		std::cout
			<< Exteraless::ApplyZalgoFilter(arguments[2])
				.toUtf8().constData()
			<< '\n';
		return 0;
	}
	if (mode == QStringLiteral("zalgo-tags")) {
		if (arguments.size() != 5) {
			return PrintUsage();
		}
		auto ok = false;
		const auto offset = arguments[3].toInt(&ok);
		if (!ok || offset < 0) {
			return PrintUsage();
		}
		const auto length = arguments[4].toInt(&ok);
		if (!ok || length < 0) {
			return PrintUsage();
		}
		const auto &text = arguments[2];
		const auto zalgo = Exteraless::ApplyZalgoFilterWithMap(text);
		const auto from = std::clamp(offset, 0, int(text.size()));
		const auto to = std::clamp(offset + length, from, int(text.size()));
		PrintJson(QJsonObject{
			{ QStringLiteral("text"), zalgo.text },
			{ QStringLiteral("offset"), zalgo.map[from] },
			{ QStringLiteral("length"),
				zalgo.map[to] - zalgo.map[from] },
		});
		return 0;
	}
	if (mode == QStringLiteral("spoilers-direct")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("direct"),
				Exteraless::Config::Instance().showSpoilersDirectly() },
		});
		return 0;
	}
	if (mode == QStringLiteral("spoilers-direct-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setShowSpoilersDirectly(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("formatting-buttons")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().showFormattingButtons() },
		});
		return 0;
	}
	if (mode == QStringLiteral("formatting-buttons-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setShowFormattingButtons(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("proxy-rule")) {
		if (arguments.size() != 6) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 4>();
		for (auto i = 0; i != 4; ++i) {
			if (!ReadFlag(arguments[2 + i], &flags[i])) {
				return PrintUsage();
			}
		}
		const auto action = Exteraless::DecideProxyRule(
			{ .conditionActive = flags[0], .autoDisabled = flags[1] },
			flags[2],
			flags[3]);
		PrintJson(QJsonObject{
			{ QStringLiteral("conditionActive"),
				action.state.conditionActive },
			{ QStringLiteral("autoDisabled"), action.state.autoDisabled },
			{ QStringLiteral("disable"), action.disableProxy },
			{ QStringLiteral("enable"), action.enableProxy },
		});
		return 0;
	}
	if (mode == QStringLiteral("vpn-name")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		std::cout
			<< (Exteraless::IsVpnInterfaceName(arguments[2]) ? "1" : "0")
			<< '\n';
		return 0;
	}
	if (mode == QStringLiteral("deletedmark")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintDeletedMark();
		return 0;
	}
	if (mode == QStringLiteral("deletedmark-set")) {
		if (arguments.size() != 6) {
			return PrintUsage();
		}
		auto translucent = false;
		auto icon = false;
		if (!ReadFlag(arguments[2], &translucent)
			|| !ReadFlag(arguments[3], &icon)) {
			return PrintUsage();
		}
		auto ok = false;
		const auto color = arguments[5].toInt(&ok);
		if (!ok) {
			return PrintUsage();
		}
		auto &config = Exteraless::Config::Instance();
		config.setTranslucentDeletedMessages(translucent);
		config.setUseDeletedIcon(icon);
		config.setCustomDeletedMark(arguments[4]);
		config.setDeletedMarkColor(color);
		PrintDeletedMark();
		return 0;
	}
	if (mode == QStringLiteral("ghost")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintGhost();
		return 0;
	}
	if (mode == QStringLiteral("ghost-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto active = false;
		if (!ReadFlag(arguments[2], &active)) {
			return PrintUsage();
		}
		Exteraless::SetGhostActive(active);
		PrintGhost();
		return 0;
	}
	if (mode == QStringLiteral("ghost-flags")) {
		if (arguments.size() != 6) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 4>();
		for (auto i = 0; i != 4; ++i) {
			if (!ReadFlag(arguments[2 + i], &flags[i])) {
				return PrintUsage();
			}
		}
		auto &config = Exteraless::Config::Instance();
		config.setGhostSendReadPackets(!flags[0]);
		config.setGhostSendReadStories(!flags[1]);
		config.setGhostSendOnline(!flags[2]);
		config.setGhostSendUploadProgress(!flags[3]);
		PrintGhost();
		return 0;
	}
	if (mode == QStringLiteral("ghost-lock")) {
		if (arguments.size() != 6) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 4>();
		for (auto i = 0; i != 4; ++i) {
			if (!ReadFlag(arguments[2 + i], &flags[i])) {
				return PrintUsage();
			}
		}
		auto &config = Exteraless::Config::Instance();
		config.setGhostSendReadLocked(flags[0]);
		config.setGhostSendStoriesLocked(flags[1]);
		config.setGhostSendOnlineLocked(flags[2]);
		config.setGhostSendUploadProgressLocked(flags[3]);
		PrintGhost();
		return 0;
	}
	if (mode == QStringLiteral("ghost-schedule")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("enabled"),
				Exteraless::Config::Instance().ghostScheduledSend() },
		});
		return 0;
	}
	if (mode == QStringLiteral("ghost-schedule-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setGhostScheduledSend(value);
		std::cout << (value ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("ghost-schedule-decide")) {
		if (arguments.size() != 8) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 6>();
		for (auto i = 0; i != 6; ++i) {
			if (!ReadFlag(arguments[2 + i], &flags[i])) {
				return PrintUsage();
			}
		}
		const auto schedule = Exteraless::ShouldGhostSchedule({
			.enabled = flags[0],
			.sendOnlinePackets = flags[1],
			.self = flags[2],
			.secret = flags[3],
			.bot = flags[4],
			.user = flags[5],
		});
		std::cout << (schedule ? "1" : "0") << '\n';
		return 0;
	}
	if (mode == QStringLiteral("ghost-show")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setGhostShowStatus(value);
		PrintGhost();
		return 0;
	}
	if (mode == QStringLiteral("pillorder")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintPillOrder();
		return 0;
	}
	if (mode == QStringLiteral("pillorder-reset")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setPillStackOrder(QString());
		PrintPillOrder();
		return 0;
	}
	if (mode == QStringLiteral("pillorder-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setPillStackOrder(arguments[2]);
		PrintPillOrder();
		return 0;
	}
	if (mode == QStringLiteral("stickertime-set")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		auto side = false;
		auto hidden = false;
		if (!ReadFlag(arguments[2], &side)
			|| !ReadFlag(arguments[3], &hidden)) {
			return PrintUsage();
		}
		auto &config = Exteraless::Config::Instance();
		config.setChatsStickerTimeSide(side);
		config.setChatsHideStickerTime(hidden);
		PrintStickerTime();
		return 0;
	}
	if (mode == QStringLiteral("deleted")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintDeleted();
		return 0;
	}
	if (mode == QStringLiteral("deleted-set")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setSaveDeletedMessages(value);
		PrintDeleted();
		return 0;
	}
	if (mode == QStringLiteral("deleted-entry")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto error = QJsonParseError{0, QJsonParseError::NoError};
		const auto document = QJsonDocument::fromJson(
			arguments[2].toUtf8(),
			&error);
		if (error.error != QJsonParseError::NoError
			|| !document.isObject()) {
			return PrintUsage();
		}
		const auto entry = Exteraless::DeserializeDeletedEntry(
			document.object());
		if (!entry) {
			PrintJson(QJsonObject{ { QStringLiteral("valid"), false } });
			return 0;
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("valid"), true },
			{ QStringLiteral("entry"),
				Exteraless::SerializeDeletedEntry(*entry) },
		});
		return 0;
	}
	if (mode == QStringLiteral("deleted-trim")) {
		if (arguments.size() != 3 && arguments.size() != 5) {
			return PrintUsage();
		}
		auto maxEntries = int(Exteraless::kDeletedMessagesMaxEntries);
		auto maxPerPeer = int(Exteraless::kDeletedMessagesMaxPerPeer);
		if (arguments.size() == 5) {
			auto ok = false;
			maxEntries = arguments[2].toInt(&ok);
			if (!ok) {
				return PrintUsage();
			}
			maxPerPeer = arguments[3].toInt(&ok);
			if (!ok) {
				return PrintUsage();
			}
		}
		const auto document = QJsonDocument::fromJson(
			arguments[arguments.size() - 1].toUtf8());
		if (!document.isArray()) {
			return PrintUsage();
		}
		struct TrimEntry {
			qint64 peerId = 0;
			qint64 msgId = 0;
		};
		auto entries = std::vector<TrimEntry>();
		for (const auto &value : document.array()) {
			const auto pair = value.toArray();
			if (pair.size() != 2) {
				return PrintUsage();
			}
			entries.push_back({
				qint64(pair[0].toDouble()),
				qint64(pair[1].toDouble()),
			});
		}
		Exteraless::TrimDeletedEntries(
			&entries,
			[](const TrimEntry &entry) { return entry.peerId; },
			maxEntries,
			maxPerPeer);
		auto result = QJsonArray();
		for (const auto &entry : entries) {
			result.append(QJsonArray{
				double(entry.peerId),
				double(entry.msgId) });
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("count"), int(result.size()) },
			{ QStringLiteral("entries"), result },
		});
		return 0;
	}
	if (mode == QStringLiteral("config-set")) {
		if (arguments.size() != 9) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 7>();
		if (!ReadFlags(arguments, 2, &flags)) {
			return PrintUsage();
		}
		ApplyFlags(flags);
		PrintChats();
		return 0;
	}
	if (mode == QStringLiteral("config-fromjson")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		const auto document = QJsonDocument::fromJson(arguments[2].toUtf8());
		if (!document.isObject()) {
			return PrintUsage();
		}
		auto unknown = QStringList();
		const auto accepted = Exteraless::Config::Instance().fromJson(
			document.object(),
			&unknown);
		PrintJson(QJsonObject{
			{ QStringLiteral("accepted"), accepted },
			{ QStringLiteral("unknown"),
				QJsonArray::fromStringList(unknown) },
			{ QStringLiteral("chats"),
				ChatsArray(Exteraless::Config::Instance().chatsSettings()) },
		});
		return 0;
	}
	if (mode == QStringLiteral("config-raw")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintJson(Exteraless::Config::Instance().toJson());
		return 0;
	}
	if (mode == QStringLiteral("config-reset")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().reset();
		PrintJson(Exteraless::Config::Instance().toJson());
		return 0;
	}
	if (mode == QStringLiteral("backup-export")) {
		if (arguments.size() != 9) {
			return PrintUsage();
		}
		auto flags = std::array<bool, 7>();
		if (!ReadFlags(arguments, 2, &flags)) {
			return PrintUsage();
		}
		ApplyFlags(flags);
		PrintBackup();
		return 0;
	}
	if (mode == QStringLiteral("backup-export-current")) {
		if (arguments.size() != 2) {
			return PrintUsage();
		}
		PrintBackup();
		return 0;
	}
	if (mode == QStringLiteral("backup-import")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		const auto encoded = FromBase64(arguments[2]);
		PrintImportResult(Exteraless::ApplyBackup(encoded));
		return 0;
	}
	if (mode == QStringLiteral("backup-import-json")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		const auto document = QJsonDocument::fromJson(arguments[2].toUtf8());
		if (!document.isObject()) {
			return PrintUsage();
		}
		const auto encoded = Exteraless::EncodeInvisible(
			document.toJson(QJsonDocument::Compact));
		PrintImportResult(Exteraless::ApplyBackup(encoded));
		return 0;
	}
	if (mode == QStringLiteral("subtitles-parse")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto bytes = QByteArray();
		if (!ReadFileBytes(arguments[2], &bytes)) {
			return PrintUsage();
		}
		auto cues = QJsonArray();
		for (const auto &cue : Exteraless::ParseSubtitles(bytes)) {
			cues.append(QJsonObject{
				{ QStringLiteral("start"), cue.start },
				{ QStringLiteral("end"), cue.end },
				{ QStringLiteral("text"), cue.text },
			});
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("cues"), cues },
		});
		return 0;
	}
	if (mode == QStringLiteral("subtitles-active")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		auto ok = false;
		const auto position = arguments[3].toLongLong(&ok);
		if (!ok || position < 0) {
			return PrintUsage();
		}
		auto bytes = QByteArray();
		if (!ReadFileBytes(arguments[2], &bytes)) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("text"),
				Exteraless::ActiveCueText(
					Exteraless::ParseSubtitles(bytes),
					position) },
		});
		return 0;
	}
	if (mode == QStringLiteral("subtitles-key")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto ok = false;
		const auto docId = arguments[2].toLongLong(&ok);
		if (!ok || docId < 0) {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("key"), Exteraless::VideoKey(docId) },
		});
		return 0;
	}
	if (mode == QStringLiteral("lyrics-parse")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto bytes = QByteArray();
		if (!ReadFileBytes(arguments[2], &bytes)) {
			return PrintUsage();
		}
		const auto lyrics = Exteraless::ParseLyrics(bytes);
		auto lines = QJsonArray();
		for (const auto &line : lyrics.lines) {
			lines.append(QJsonObject{
				{ QStringLiteral("ms"), line.ms },
				{ QStringLiteral("text"), line.text },
			});
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("lines"), lines },
			{ QStringLiteral("synced"), lyrics.synced },
		});
		return 0;
	}
	if (mode == QStringLiteral("lyrics-active")) {
		if (arguments.size() != 4) {
			return PrintUsage();
		}
		auto ok = false;
		const auto position = arguments[3].toLongLong(&ok);
		if (!ok || position < 0) {
			return PrintUsage();
		}
		auto bytes = QByteArray();
		if (!ReadFileBytes(arguments[2], &bytes)) {
			return PrintUsage();
		}
		const auto lyrics = Exteraless::ParseLyrics(bytes);
		const auto index = Exteraless::ActiveIndex(lyrics, position);
		PrintJson(QJsonObject{
			{ QStringLiteral("index"), index },
			{ QStringLiteral("text"),
				(index >= 0) ? lyrics.lines[index].text : QString() },
		});
		return 0;
	}
	if (mode == QStringLiteral("lyrics-search-parse")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto bytes = QByteArray();
		if (!ReadFileBytes(arguments[2], &bytes)) {
			return PrintUsage();
		}
		auto results = QJsonArray();
		for (const auto &result : Exteraless::ParseSearch(bytes)) {
			results.append(QJsonObject{
				{ QStringLiteral("id"), result.id },
				{ QStringLiteral("track"), result.track },
				{ QStringLiteral("artist"), result.artist },
				{ QStringLiteral("hasSynced"), result.hasSynced },
			});
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("results"), results },
		});
		return 0;
	}
	if (mode == QStringLiteral("lyrics-get-parse")) {
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto bytes = QByteArray();
		if (!ReadFileBytes(arguments[2], &bytes)) {
			return PrintUsage();
		}
		const auto lyrics = Exteraless::ParseGet(bytes);
		PrintJson(QJsonObject{
			{ QStringLiteral("ok"), lyrics.ok },
			{ QStringLiteral("synced"), lyrics.synced },
			{ QStringLiteral("text"), lyrics.text },
		});
		return 0;
	}
	if (mode == QStringLiteral("extra-pills-parse")) {
		if (arguments.size() != 4 && arguments.size() != 5) {
			return PrintUsage();
		}
		auto bytes = QByteArray();
		if (!ReadFileBytes(arguments[3], &bytes)) {
			return PrintUsage();
		}
		const auto domain = arguments[2];
		const auto code = (arguments.size() == 5)
			? arguments[4]
			: QString();
		auto text = QString();
		if (domain == QStringLiteral("weather")) {
			text = Exteraless::ParseWeather(bytes);
		} else if (domain == QStringLiteral("coinbase")) {
			text = Exteraless::ParseCoinbase(bytes);
		} else if (domain == QStringLiteral("ecb")) {
			text = Exteraless::FormatFiatRate(
				Exteraless::ParseEcbXml(bytes),
				code.isEmpty() ? QStringLiteral("USD") : code);
		} else if (domain == QStringLiteral("gold")) {
			text = Exteraless::ParseGoldApi(bytes);
		} else {
			return PrintUsage();
		}
		PrintJson(QJsonObject{
			{ QStringLiteral("text"), text },
		});
		return 0;
	}
	return PrintUsage();
}
