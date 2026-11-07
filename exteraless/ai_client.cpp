#include "exteraless/ai_client.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

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

QByteArray BuildRequestJson(
		const QString &endpoint,
		const QString &model,
		const AiRequest &request) {
	auto messages = QJsonArray();
	if (!request.system.isEmpty()) {
		messages.append(QJsonObject{
			{ QStringLiteral("role"), QStringLiteral("system") },
			{ QStringLiteral("content"), request.system },
		});
	}
	messages.append(QJsonObject{
		{ QStringLiteral("role"), QStringLiteral("user") },
		{ QStringLiteral("content"), request.prompt },
	});
	const auto root = QJsonObject{
		{ QStringLiteral("model"), model },
		{ QStringLiteral("messages"), messages },
		{ QStringLiteral("stream"), false },
	};
	return QJsonDocument(root).toJson(QJsonDocument::Compact);
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
