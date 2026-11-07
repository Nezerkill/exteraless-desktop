#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Exteraless {

struct BackupImportResult {
	bool ok = false;
	QString error;
	QStringList unknown;
	int applied = 0;
};

[[nodiscard]] QString EncodeInvisible(const QByteArray &json);
[[nodiscard]] QByteArray DecodeInvisible(const QString &text);

[[nodiscard]] QString BuildBackup(bool redactSecrets = false);
[[nodiscard]] BackupImportResult ApplyBackup(const QString &text);

} // namespace Exteraless
