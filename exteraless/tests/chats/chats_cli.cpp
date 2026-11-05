#include "exteraless/backup.h"
#include "exteraless/chats.h"
#include "exteraless/config.h"
#include "exteraless/deleted_entry.h"
#include "exteraless/deleted_messages_trim.h"
#include "exteraless/doubletap.h"
#include "exteraless/format.h"
#include "exteraless/ghost_status.h"
#include "exteraless/links.h"
#include "exteraless/network.h"
#include "exteraless/pangu.h"
#include "exteraless/proxy_rules.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

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
		<< "  exteraless-chats-cli clock <hh> <mm> <ss> <0|1> [<locale>]\n"
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
		<< "  exteraless-chats-cli ghost\n"
		<< "  exteraless-chats-cli ghost-set <0|1>\n"
		<< "  exteraless-chats-cli ghost-flags <read> <stories> <online> "
		<< "<typing>\n"
		<< "  exteraless-chats-cli ghost-show <0|1>\n"
		<< "  exteraless-chats-cli ghost-lock <read> <stories> <online> "
		<< "<typing> <offline>\n"
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
		<< "  exteraless-chats-cli backup-import-json <json>\n";
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

void PrintJson(const QJsonObject &root) {
	const auto bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
	std::cout.write(bytes.constData(), bytes.size());
	std::cout << '\n';
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
		{ QStringLiteral("offlineLocked"),
			config.ghostSendOfflineAfterOnlineLocked() },
		{ QStringLiteral("unlocked"), Exteraless::GhostUnlockedCount() },
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
	case Exteraless::DoubleTapAction::Reply:
		return QStringLiteral("reply");
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
		config.setGhostSendReadLocked(flags[0]);
		config.setGhostSendStoriesLocked(flags[1]);
		config.setGhostSendOnlineLocked(flags[2]);
		config.setGhostSendUploadProgressLocked(flags[3]);
		config.setGhostSendOfflineAfterOnlineLocked(flags[4]);
		PrintGhost();
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
	return PrintUsage();
}
