#include "exteraless/ai_client.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <utility>

namespace Exteraless {
namespace {

[[nodiscard]] QString ContentText(const QJsonValue &content) {
	if (content.isString()) {
		return content.toString();
	}
	auto result = QString();
	for (const auto &part : content.toArray()) {
		result += part.toObject().value(
			QStringLiteral("text")).toString();
	}
	return result;
}

[[nodiscard]] QString ErrorMessage(const QJsonObject &root) {
	const auto error = root.value(QStringLiteral("error"));
	if (error.isString()) {
		return error.toString();
	}
	const auto object = error.toObject();
	const auto message = object.value(QStringLiteral("message")).toString();
	return message.isEmpty()
		? object.value(QStringLiteral("type")).toString()
		: message;
}

constexpr auto kMaxAiRoles = 32;
constexpr auto kMaxAiRolePromptBytes = 4096;

[[nodiscard]] bool ValidAiRole(const QJsonValue &value, AiRole *role) {
	if (!value.isObject()) {
		return false;
	}
	const auto object = value.toObject();
	const auto name = object.value(QStringLiteral("name"));
	const auto prompt = object.value(QStringLiteral("prompt"));
	if (!name.isString() || name.toString().trimmed().isEmpty()) {
		return false;
	}
	if (!prompt.isString()
		|| prompt.toString().toUtf8().size() > kMaxAiRolePromptBytes) {
		return false;
	}
	*role = AiRole{ name.toString(), prompt.toString() };
	return true;
}

[[nodiscard]] bool ParseAiRolesInto(
		const QByteArray &json,
		AiRoles *roles) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(json, &error);
	if (error.error != QJsonParseError::NoError || !document.isArray()) {
		return false;
	}
	const auto array = document.array();
	if (array.size() > kMaxAiRoles) {
		return false;
	}
	auto result = AiRoles();
	result.reserve(size_t(array.size()));
	for (const auto &value : array) {
		auto role = AiRole();
		if (!ValidAiRole(value, &role)) {
			return false;
		}
		result.push_back(std::move(role));
	}
	*roles = std::move(result);
	return true;
}

const auto kDataPrefix = QByteArrayLiteral("data:");
const auto kDoneMarker = QByteArrayLiteral("[DONE]");

[[nodiscard]] QByteArray EventPayload(const QByteArray &line) {
	auto trimmed = line.trimmed();
	if (!trimmed.startsWith(kDataPrefix)) {
		return QByteArray();
	}
	return trimmed.mid(kDataPrefix.size()).trimmed();
}

[[nodiscard]] QString DeltaContent(const QByteArray &payload) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(payload, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return QString();
	}
	const auto root = document.object();
	if (!root.value(QStringLiteral("error")).isUndefined()) {
		return QString();
	}
	for (const auto &choice : root.value(
			QStringLiteral("choices")).toArray()) {
		const auto delta = choice.toObject().value(
			QStringLiteral("delta")).toObject();
		const auto content = delta.value(QStringLiteral("content"));
		if (!content.isUndefined() && !content.isNull()) {
			return ContentText(content);
		}
	}
	return QString();
}

} // namespace

QString ChatCompletionsUrl(const QString &endpoint) {
	auto result = endpoint.trimmed();
	while (result.endsWith(QLatin1Char('/'))) {
		result.chop(1);
	}
	if (result.isEmpty()) {
		result = QStringLiteral("https://api.openai.com/v1");
	}
	const auto kPath = QStringLiteral("/chat/completions");
	return result.endsWith(kPath) ? result : (result + kPath);
}

AiRoles ParseAiRoles(const QByteArray &json) {
	auto roles = AiRoles();
	if (!ParseAiRolesInto(json, &roles)) {
		return AiRoles();
	}
	return roles;
}

AiRoles ParseAiRoles(const QString &json) {
	return ParseAiRoles(json.toUtf8());
}

bool IsValidAiRolesJson(const QByteArray &json) {
	auto roles = AiRoles();
	return ParseAiRolesInto(json, &roles);
}

bool IsValidAiRolesJson(const QString &json) {
	return IsValidAiRolesJson(json.toUtf8());
}

QByteArray BuildRequestJson(
		const QString &endpoint,
		const QString &model,
		const QString &prompt,
		const QString &system,
		const QList<QPair<QString, QString>> &history) {
	auto messages = QJsonArray();
	if (!system.isEmpty()) {
		messages.append(QJsonObject{
			{ QStringLiteral("role"), QStringLiteral("system") },
			{ QStringLiteral("content"), system },
		});
	}
	for (const auto &entry : history) {
		const auto role = entry.first.trimmed().toLower();
		if (role != QStringLiteral("user")
			&& role != QStringLiteral("assistant")
			&& role != QStringLiteral("system")) {
			continue;
		}
		if (entry.second.isEmpty()) {
			continue;
		}
		messages.append(QJsonObject{
			{ QStringLiteral("role"), role },
			{ QStringLiteral("content"), entry.second },
		});
	}
	messages.append(QJsonObject{
		{ QStringLiteral("role"), QStringLiteral("user") },
		{ QStringLiteral("content"), prompt },
	});
	const auto root = QJsonObject{
		{ QStringLiteral("model"), model },
		{ QStringLiteral("messages"), messages },
		{ QStringLiteral("stream"), false },
	};
	return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray BuildRequestJson(
		const QString &endpoint,
		const QString &model,
		const AiRequest &request) {
	return BuildRequestJson(
		endpoint,
		model,
		request.prompt,
		request.system,
		QList<QPair<QString, QString>>());
}

AiResponse ParseResponse(const QByteArray &body) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(body, &error);
	if (error.error != QJsonParseError::NoError) {
		return AiResponse{
			.ok = false,
			.error = QStringLiteral("Invalid JSON response"),
		};
	}
	const auto root = document.object();
	if (const auto failure = ErrorMessage(root); !failure.isEmpty()) {
		return AiResponse{ .ok = false, .error = failure };
	}
	const auto choices = root.value(QStringLiteral("choices"));
	if (!choices.isArray()) {
		return AiResponse{
			.ok = false,
			.error = QStringLiteral("No choices in response"),
		};
	}
	for (const auto &choice : choices.toArray()) {
		const auto message = choice.toObject().value(
			QStringLiteral("message")).toObject();
		const auto text = ContentText(
			message.value(QStringLiteral("content"))).trimmed();
		if (!text.isEmpty()) {
			return AiResponse{ .ok = true, .text = text };
		}
	}
	return AiResponse{
		.ok = false,
		.error = QStringLiteral("Empty response"),
	};
}

QString ParseStreamEvent(const QByteArray &line) {
	const auto payload = EventPayload(line);
	if (payload.isEmpty() || payload == kDoneMarker) {
		return QString();
	}
	return DeltaContent(payload);
}

QString SseParser::feed(const QByteArray &chunk) {
	_buffer += chunk;
	auto text = QString();
	auto consumed = qsizetype(0);
	while (true) {
		const auto end = _buffer.indexOf('\n', consumed);
		if (end < 0) {
			break;
		}
		auto line = _buffer.mid(consumed, end - consumed);
		consumed = end + 1;
		if (line.endsWith('\r')) {
			line.chop(1);
		}
		if (EventPayload(line) == kDoneMarker) {
			_done = true;
		} else {
			text += ParseStreamEvent(line);
		}
	}
	_buffer.remove(0, consumed);
	return text;
}

bool SseParser::isDone() const {
	return _done;
}

void SseParser::reset() {
	_buffer.clear();
	_done = false;
}

} // namespace Exteraless
