#include "exteraless/backup.h"
#include "exteraless/chats.h"
#include "exteraless/config.h"
#include "exteraless/deleted_messages_trim.h"
#include "exteraless/links.h"

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
		<< "  exteraless-chats-cli general-set <0|1>\n"
		<< "  exteraless-chats-cli deleted\n"
		<< "  exteraless-chats-cli deleted-set <0|1>\n"
		<< "  exteraless-chats-cli deleted-trim [<maxEntries> <maxPerPeer>] "
		<< "<json>\n"
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
	});
}

void PrintDeleted() {
	PrintJson(QJsonObject{
		{ QStringLiteral("saveDeletedMessages"),
			Exteraless::Config::Instance().saveDeletedMessages() },
	});
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
		if (arguments.size() != 3) {
			return PrintUsage();
		}
		auto value = false;
		if (!ReadFlag(arguments[2], &value)) {
			return PrintUsage();
		}
		Exteraless::Config::Instance().setDisableNumberRounding(value);
		PrintGeneral();
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
