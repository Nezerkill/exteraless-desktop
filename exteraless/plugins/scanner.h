#pragma once

#include <QString>
#include <QStringList>

#include <vector>

namespace Exteraless::Plugins {

struct Entry {
	QString file;
	QString id;
	QString name;
	QString description;
	QString author;
	QString version;
	QStringList permissions;
	QString error;
};

struct ScanResult {
	std::vector<Entry> entries;
	QString error;
};

[[nodiscard]] ScanResult ScanPlugins(const QString &directory);

} // namespace Exteraless::Plugins
