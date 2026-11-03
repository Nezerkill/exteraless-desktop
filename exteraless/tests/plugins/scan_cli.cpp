#include "exteraless/plugins/scanner.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <iostream>

namespace {

[[nodiscard]] QJsonObject EntryToJson(const Exteraless::Plugins::Entry &entry) {
	auto permissions = QJsonArray();
	for (const auto &permission : entry.permissions) {
		permissions.append(permission);
	}
	return QJsonObject{
		{ QStringLiteral("file"), entry.file },
		{ QStringLiteral("id"), entry.id },
		{ QStringLiteral("name"), entry.name },
		{ QStringLiteral("description"), entry.description },
		{ QStringLiteral("author"), entry.author },
		{ QStringLiteral("version"), entry.version },
		{ QStringLiteral("permissions"), permissions },
		{ QStringLiteral("error"), entry.error },
	};
}

} // namespace

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	const auto arguments = app.arguments();
	if (arguments.size() != 2) {
		std::cerr << "Usage: exteraless-plugins-scan <directory>\n";
		return 2;
	}
	const auto result = Exteraless::Plugins::ScanPlugins(arguments[1]);
	auto entries = QJsonArray();
	for (const auto &entry : result.entries) {
		entries.append(EntryToJson(entry));
	}
	const auto root = QJsonObject{
		{ QStringLiteral("error"), result.error },
		{ QStringLiteral("entries"), entries },
	};
	const auto output = QJsonDocument(root).toJson(QJsonDocument::Compact);
	std::cout.write(output.constData(), output.size());
	std::cout << '\n';
	std::cout.flush();
	return 0;
}
