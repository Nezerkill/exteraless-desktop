#include "exteraless/ayu_import.h"

#include "exteraless/deleted_entry.h"
#include "exteraless/deleted_messages_trim.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStringList>
#include <QUrl>
#include <QVariant>

#include <array>
#include <set>
#include <utility>
#include <vector>

namespace Exteraless {
namespace {

constexpr auto kMinAyuVersion = 21;
constexpr auto kMaxAyuVersion = 27;
// WHY: mirrors DeletedMessages::save(), the app rewrites this file itself.
constexpr auto kSidecarVersion = 2;

using Key = std::pair<qint64, qint64>;

[[nodiscard]] const std::array<const char*, 11> &AyuColumns() {
	static const auto result = std::array<const char*, 11>{
		"userId",
		"dialogId",
		"fromId",
		"messageId",
		"date",
		"editDate",
		"text",
		"replyMessageId",
		"replyQuoteText",
		"fwdName",
		"fwdDate",
	};
	return result;
}

[[nodiscard]] QString SidecarPath(const QString &dbPath) {
	return QFileInfo(dbPath).absolutePath()
		+ QStringLiteral("/exteraless/deleted_messages.json");
}

[[nodiscard]] QVariant ColumnValue(
		const QSqlRecord &record,
		const char *name) {
	const auto index = record.indexOf(QString::fromLatin1(name));
	return (index >= 0) ? record.value(index) : QVariant();
}

class ConnectionGuard final {
public:
	explicit ConnectionGuard(const QString &name) : _name(name) {
	}
	~ConnectionGuard() {
		QSqlDatabase::removeDatabase(_name);
	}

private:
	const QString _name;
};

struct AyuRows {
	std::vector<DeletedEntry> entries;
	int skipped = 0;
	int rejectedVersion = 0;
	QString error;
};

struct AyuOpenAttempt {
	QString connection;
	QString name;
	QString options;
};

[[nodiscard]] bool TryOpenAyu(
		const AyuOpenAttempt &attempt,
		QSqlDatabase *db,
		int *version,
		QString *error) {
	auto candidate = QSqlDatabase::addDatabase(
		QStringLiteral("QSQLITE"),
		attempt.connection);
	candidate.setDatabaseName(attempt.name);
	candidate.setConnectOptions(attempt.options);
	if (!candidate.open()) {
		*error = candidate.lastError().text();
		return false;
	}
	auto query = QSqlQuery(candidate);
	if (!query.exec(QStringLiteral("PRAGMA user_version"))
		|| !query.next()) {
		*error = query.lastError().text();
		return false;
	}
	*version = query.value(0).toInt();
	*db = candidate;
	return true;
}

[[nodiscard]] AyuRows ReadAyuRows(const QString &dbPath, qint64 accountId) {
	auto result = AyuRows();
	if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
		result.error = QStringLiteral("QSQLITE driver is not available");
		return result;
	}
	const auto readOnlyName = QStringLiteral("exteraless-ayu-import");
	const auto immutableName = readOnlyName + QStringLiteral("-immutable");
	auto readOnlyGuard = ConnectionGuard(readOnlyName);
	auto immutableGuard = ConnectionGuard(immutableName);
	const auto readOnly = AyuOpenAttempt{
		.connection = readOnlyName,
		.name = dbPath,
		.options = QStringLiteral("QSQLITE_OPEN_READONLY"),
	};
	const auto immutable = AyuOpenAttempt{
		.connection = immutableName,
		.name = QStringLiteral("file:")
			+ QString::fromLatin1(QUrl::toPercentEncoding(
				dbPath,
				QByteArrayLiteral("/")))
			+ QStringLiteral("?immutable=1"),
		.options = QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_OPEN_URI"),
	};
	// WHY: an immutable read never creates the -shm/-wal bookkeeping files,
	// but it ignores a pending WAL, so it is only preferred without one.
	const auto wal = QFileInfo(dbPath + QStringLiteral("-wal"));
	const auto immutableFirst = !wal.exists() || wal.size() == 0;
	auto db = QSqlDatabase();
	auto version = -1;
	auto firstError = QString();
	auto secondError = QString();
	if (!TryOpenAyu(
			immutableFirst ? immutable : readOnly,
			&db,
			&version,
			&firstError)
		&& !TryOpenAyu(
			immutableFirst ? readOnly : immutable,
			&db,
			&version,
			&secondError)) {
		result.error = QStringLiteral("cannot open ")
			+ dbPath
			+ QStringLiteral(": ")
			+ secondError
			+ QStringLiteral(" (first attempt: ")
			+ firstError
			+ QStringLiteral(")");
		return result;
	}
	if (version < kMinAyuVersion || version > kMaxAyuVersion) {
		result.rejectedVersion = version;
		result.error = QStringLiteral(
			"unsupported AyuGram database version %1 (expected %2..%3)")
			.arg(version)
			.arg(kMinAyuVersion)
			.arg(kMaxAyuVersion);
		return result;
	}

	auto columnsQuery = QSqlQuery(db);
	if (!columnsQuery.exec(QStringLiteral(
			"SELECT * FROM deletedmessage LIMIT 0"))) {
		result.error = QStringLiteral("cannot read deletedmessage: ")
			+ columnsQuery.lastError().text();
		return result;
	}
	const auto record = columnsQuery.record();
	auto names = QStringList();
	for (const auto name : AyuColumns()) {
		const auto field = QString::fromLatin1(name);
		if (record.indexOf(field) >= 0) {
			names.push_back(field);
		}
	}
	const auto complete = record.indexOf(QStringLiteral("dialogId")) >= 0
		&& record.indexOf(QStringLiteral("messageId")) >= 0
		&& record.indexOf(QStringLiteral("date")) >= 0
		&& (!accountId || record.indexOf(QStringLiteral("userId")) >= 0);
	if (!complete) {
		result.error = QStringLiteral(
			"deletedmessage lacks the columns this importer needs");
		return result;
	}

	auto statement = QStringLiteral("SELECT ")
		+ names.join(QStringLiteral(", "))
		+ QStringLiteral(" FROM deletedmessage");
	if (accountId) {
		statement += QStringLiteral(" WHERE userId = ?");
	}
	statement += QStringLiteral(" ORDER BY date ASC");
	auto rowsQuery = QSqlQuery(db);
	rowsQuery.setForwardOnly(true);
	if (!rowsQuery.prepare(statement)) {
		result.error = QStringLiteral("cannot prepare the query: ")
			+ rowsQuery.lastError().text();
		return result;
	}
	if (accountId) {
		rowsQuery.addBindValue(accountId);
	}
	if (!rowsQuery.exec()) {
		result.error = QStringLiteral("cannot read deletedmessage: ")
			+ rowsQuery.lastError().text();
		return result;
	}
	auto keys = std::set<Key>();
	while (rowsQuery.next()) {
		const auto row = rowsQuery.record();
		auto entry = DeletedEntry();
		entry.peerId = ColumnValue(row, "dialogId").toLongLong();
		entry.msgId = ColumnValue(row, "messageId").toLongLong();
		if (!entry.peerId || !entry.msgId) {
			++result.skipped;
			continue;
		}
		if (!keys.insert(Key(entry.peerId, entry.msgId)).second) {
			++result.skipped;
			continue;
		}
		entry.fromId = ColumnValue(row, "fromId").toLongLong();
		entry.date = ColumnValue(row, "date").toLongLong() * 1000;
		entry.editedDate = ColumnValue(row, "editDate").toLongLong() * 1000;
		entry.text = ColumnValue(row, "text").toString();
		entry.replyToMsgId = ColumnValue(row, "replyMessageId").toLongLong();
		entry.replyToText = ColumnValue(row, "replyQuoteText").toString();
		entry.forwardedFrom = ColumnValue(row, "fwdName").toString();
		entry.forwardedDate = ColumnValue(row, "fwdDate").toLongLong() * 1000;
		result.entries.push_back(std::move(entry));
	}
	return result;
}

} // namespace

AyuImportResult ImportAyuDatabase(const QString &dbPath, qint64 accountId) {
	auto result = AyuImportResult();
	const auto info = QFileInfo(dbPath);
	if (dbPath.isEmpty() || !info.isFile()) {
		result.error = QStringLiteral("database file not found: ") + dbPath;
		return result;
	}
	const auto rows = ReadAyuRows(dbPath, accountId);
	if (!rows.error.isEmpty()) {
		result.rejectedVersion = rows.rejectedVersion;
		result.error = rows.error;
		return result;
	}

	const auto sidecar = SidecarPath(dbPath);
	auto entries = std::vector<DeletedEntry>();
	auto known = std::set<Key>();
	auto file = QFile(sidecar);
	if (file.open(QIODevice::ReadOnly)) {
		const auto document = QJsonDocument::fromJson(file.readAll());
		file.close();
		for (const auto &value
			: document.object().value(QStringLiteral("messages")).toArray()) {
			if (auto entry = DeserializeDeletedEntry(value.toObject())) {
				known.insert(Key(entry->peerId, entry->msgId));
				entries.push_back(std::move(*entry));
			}
		}
	}
	auto skipped = rows.skipped;
	auto appended = std::vector<Key>();
	for (auto entry : rows.entries) {
		const auto key = Key(entry.peerId, entry.msgId);
		if (known.contains(key)) {
			++skipped;
			continue;
		}
		known.insert(key);
		appended.push_back(key);
		entries.push_back(std::move(entry));
	}
	if (appended.empty()) {
		result.skipped = skipped;
		return result;
	}

	TrimDeletedEntries(
		&entries,
		[](const DeletedEntry &entry) { return entry.peerId; });
	auto kept = std::set<Key>();
	for (const auto &entry : entries) {
		kept.insert(Key(entry.peerId, entry.msgId));
	}
	auto imported = 0;
	for (const auto &key : appended) {
		if (kept.contains(key)) {
			++imported;
		}
	}
	skipped += int(appended.size()) - imported;

	auto messages = QJsonArray();
	for (const auto &entry : entries) {
		messages.append(SerializeDeletedEntry(entry));
	}
	auto root = QJsonObject();
	root["version"] = kSidecarVersion;
	root["messages"] = messages;
	const auto directory = QFileInfo(sidecar).absolutePath();
	if (!QDir().mkpath(directory)) {
		result.error = QStringLiteral("cannot create ") + directory;
		return result;
	}
	auto out = QSaveFile(sidecar);
	if (!out.open(QIODevice::WriteOnly)) {
		result.error = QStringLiteral("cannot write ")
			+ sidecar
			+ QStringLiteral(": ")
			+ out.errorString();
		return result;
	}
	out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
	if (!out.commit()) {
		result.error = QStringLiteral("cannot write ")
			+ sidecar
			+ QStringLiteral(": ")
			+ out.errorString();
		return result;
	}
	result.imported = imported;
	result.skipped = skipped;
	return result;
}

} // namespace Exteraless
