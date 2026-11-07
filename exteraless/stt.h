#pragma once

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>

namespace Exteraless::Speech {

struct Result {
	bool ok = false;
	QString text;
	QString error;
};

using Done = std::function<void(Result)>;

[[nodiscard]] Result TranscribeBlocking(
	const QString &audioPath,
	const QByteArray &audioBytes,
	const QString &langCode);

void Transcribe(
	const QString &audioPath,
	const QByteArray &audioBytes,
	const QString &langCode,
	Done done);

// frames counts stereo pairs; the samples are copied before returning.
void TranscribePcm(
	const int16_t *stereo48k,
	size_t frames,
	const QString &langCode,
	Done done);

[[nodiscard]] QString ModelDir();

[[nodiscard]] QString EngineVersion();

} // namespace Exteraless::Speech
