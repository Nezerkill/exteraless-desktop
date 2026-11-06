#pragma once

#include <QByteArray>
#include <QString>

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

[[nodiscard]] QString ChatCompletionsUrl(const QString &endpoint);

[[nodiscard]] QByteArray BuildRequestJson(
	const QString &endpoint,
	const QString &model,
	const AiRequest &request);

[[nodiscard]] AiResponse ParseResponse(const QByteArray &body);

} // namespace Exteraless
