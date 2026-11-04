#include "exteraless/backup.h"

#include "exteraless/config.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace Exteraless {
namespace {

const auto kPrefix = QString(QChar(0x2001)) + QChar(0x2002);
const auto kSeparator = QChar(0x2000);
const auto kAlphabet = QString::fromUtf16(
	u"\u200a\u200b\u200c\u200f\u202f\u206a\u206b\u206c\u206d\u206e\u206f");
constexpr auto kVersion = 1;

constexpr auto kRelativeLastSeen = "relativeLastSeen";
constexpr auto kAddCommaAfterMention = "addCommaAfterMention";
constexpr auto kDoNotUseProxy = "doNotUseProxy";
constexpr auto kRemoveMessageTail = "removeMessageTail";
constexpr auto kHideReactionsInChannels = "hideReactionsInChannels";
constexpr auto kHideReactionsInGroups = "hideReactionsInGroups";
constexpr auto kHideReactionsInPrivateChats = "hideReactionsInPrivateChats";

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

[[nodiscard]] QJsonObject ExteraSection(const QJsonObject &root) {
	const auto section = root.value("exteraconfig");
	return section.isObject() ? section.toObject() : root;
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
	const auto chats = Config::Instance().chatsSettings();
	auto extera = QJsonObject();
	extera[kRelativeLastSeen] = Config::Instance().relativeLastSeen();
	extera[kAddCommaAfterMention] = Config::Instance().addCommaAfterMention();
	extera[kDoNotUseProxy] = Config::Instance().proxyDisableConditions();
	extera[kRemoveMessageTail] = chats.removeMessageTail;
	extera[kHideReactionsInChannels] = chats.hideReactionsChannels;
	extera[kHideReactionsInGroups] = chats.hideReactionsGroups;
	extera[kHideReactionsInPrivateChats] = chats.hideReactionsPrivate;

	auto root = QJsonObject();
	root["version"] = kVersion;
	root["exteraconfig"] = extera;

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

	const auto section = ExteraSection(document.object());
	auto &config = Config::Instance();
	for (auto i = section.begin(); i != section.end(); ++i) {
		const auto key = i.key();
		const auto value = i.value();
		if (key == kRelativeLastSeen && value.isBool()) {
			config.setRelativeLastSeen(value.toBool());
			++result.applied;
		} else if (key == kAddCommaAfterMention && value.isBool()) {
			config.setAddCommaAfterMention(value.toBool());
			++result.applied;
		} else if (key == kDoNotUseProxy
			&& value.isDouble()
			&& value.toInt() >= 0
			&& value.toInt() <= 7) {
			config.setProxyDisableConditions(value.toInt());
			++result.applied;
		} else if (key == kRemoveMessageTail && value.isBool()) {
			config.setChatsRemoveMessageTail(value.toBool());
			++result.applied;
		} else if (key == kHideReactionsInChannels && value.isBool()) {
			config.setChatsHideReactionsChannels(value.toBool());
			++result.applied;
		} else if (key == kHideReactionsInGroups && value.isBool()) {
			config.setChatsHideReactionsGroups(value.toBool());
			++result.applied;
		} else if (key == kHideReactionsInPrivateChats && value.isBool()) {
			config.setChatsHideReactionsPrivate(value.toBool());
			++result.applied;
		} else {
			result.unknown.push_back(key);
		}
	}

	result.ok = true;
	return result;
}

} // namespace Exteraless
