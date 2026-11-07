#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>

#include <vector>

namespace Exteraless {

struct AiRequest {
	QString prompt;
	QString system;
};

struct AiResponse {
	bool ok = false;
	QString text;
	QString error;
};

// Stored JSON keys are "name"/"prompt"; the box shows title and sends system.
struct AiRole {
	QString title;
	QString system;
};

using AiRoles = std::vector<AiRole>;

[[nodiscard]] QString ChatCompletionsUrl(const QString &endpoint);

[[nodiscard]] AiRoles ParseAiRoles(const QByteArray &json);
[[nodiscard]] AiRoles ParseAiRoles(const QString &json);
[[nodiscard]] bool IsValidAiRolesJson(const QByteArray &json);
[[nodiscard]] bool IsValidAiRolesJson(const QString &json);

[[nodiscard]] QByteArray BuildRequestJson(
	const QString &endpoint,
	const QString &model,
	const QString &prompt,
	const QString &system,
	const QList<QPair<QString, QString>> &history);

[[nodiscard]] QByteArray BuildRequestJson(
	const QString &endpoint,
	const QString &model,
	const AiRequest &request);

[[nodiscard]] AiResponse ParseResponse(const QByteArray &body);

class SseParser {
public:
	[[nodiscard]] QString feed(const QByteArray &chunk);
	[[nodiscard]] bool isDone() const;
	void reset();

private:
	QByteArray _buffer;
	bool _done = false;
};

[[nodiscard]] QString ParseStreamEvent(const QByteArray &line);

} // namespace Exteraless
