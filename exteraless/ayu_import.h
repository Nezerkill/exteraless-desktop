#pragma once

#include <QString>
#include <QtGlobal>

namespace Exteraless {

struct [[nodiscard]] AyuImportResult {
	int imported = 0;
	int skipped = 0;
	int rejectedVersion = 0;
	QString error;
};

// Reads the AyuGram `deletedmessage` table and merges it into the deleted
// messages sidecar stored next to the database in `exteraless/`.
[[nodiscard]] AyuImportResult ImportAyuDatabase(
	const QString &dbPath,
	qint64 accountId);

} // namespace Exteraless
