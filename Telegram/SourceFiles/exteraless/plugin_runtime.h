#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <memory>
#include <vector>

class QProcess;

namespace Exteraless::Plugins {

struct Result {
	bool ok = false;
	QString error;
};

struct PathCheck {
	bool ok = false;
	QString canonicalPath;
	QString error;
};

struct MenuRecord {
	QString pluginId;
	QString itemId;
	QString text;
	QString subtext;
	int priority = 0;
	QString type;
};

class Runtime final {
public:
	Runtime(QString pluginsDirectory, QString auditLogPath);
	~Runtime();

	Runtime(const Runtime &) = delete;
	Runtime &operator=(const Runtime &) = delete;

	static void Start(QString pluginsDirectory, QString auditLogPath);
	[[nodiscard]] static Runtime *Instance();
	static void Stop();

	[[nodiscard]] Result enable(const QString &id, const QString &path);
	[[nodiscard]] Result disable(const QString &id);
	void stop();

	[[nodiscard]] bool isRunning() const;
	[[nodiscard]] bool isLoaded(const QString &id) const;
	[[nodiscard]] qint64 hostPid() const;
	[[nodiscard]] const QString &pluginsDirectory() const;
	[[nodiscard]] QJsonObject settings(const QString &id) const;
	[[nodiscard]] QJsonArray settingsUi(const QString &id) const;
	void saveSetting(
		const QString &id,
		const QString &key,
		const QJsonValue &value);

	[[nodiscard]] std::vector<MenuRecord> menus(const QString &id) const;
	[[nodiscard]] Result click(
		const QString &id,
		const QString &item,
		const QString &context);

	[[nodiscard]] static PathCheck ValidatePath(
		const QString &pluginsDirectory,
		const QString &path);

private:
	void clearHost();
	[[nodiscard]] bool startHost(QString *error);
	void protocolViolation(QString *error);
	void parseMenuSnapshot(const QString &id, const QJsonValue &snapshot);
	[[nodiscard]] Result clickMenu(
		const QString &id,
		const QString &item,
		const QString &context);
	[[nodiscard]] bool sendRequest(
		const QJsonObject &request,
		QJsonObject *response,
		QString *error,
		int timeout);
	[[nodiscard]] bool readMessage(
		int timeout,
		int expectedSeq,
		bool returnReady,
		QJsonObject *response,
		QString *error);
	void writeAuditRecord(
		const QString &kind,
		const QString &id,
		const QString &event,
		const QString &detail);

	std::unique_ptr<QProcess> _process;
	QByteArray _readBuffer;
	QHash<QString, QString> _loaded;
	QHash<QString, QJsonObject> _settings;
	QHash<QString, QJsonArray> _settingsUi;
	QHash<QString, std::vector<MenuRecord>> _menus;
	bool _clicking = false;
	int _seq = 0;
	QString _pluginsDirectory;
	QString _auditLogPath;

};

} // namespace Exteraless::Plugins
