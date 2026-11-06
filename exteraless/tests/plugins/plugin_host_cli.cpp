#include "exteraless/config.h"
#include "exteraless/exteraless.h"
#include "exteraless/plugin_runtime.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStringList>

#include <iostream>

namespace {

using Exteraless::Plugins::Runtime;

void WriteJson(const QJsonObject &result) {
	const auto output = QJsonDocument(result).toJson(QJsonDocument::Compact);
	std::cout.write(output.constData(), output.size());
	std::cout << '\n';
	std::cout.flush();
}

void WriteJson(const QJsonArray &result) {
	const auto output = QJsonDocument(result).toJson(QJsonDocument::Compact);
	std::cout.write(output.constData(), output.size());
	std::cout << '\n';
	std::cout.flush();
}

int Usage() {
	std::cerr << "Usage: exteraless-plugin-host-cli "
		"<pluginsRoot> <auditLog> <command> [args...]\n";
	return 2;
}

[[nodiscard]] Runtime *StartRuntime(
		const QString &pluginsRoot,
		const QString &auditLog) {
	Runtime::Start(pluginsRoot, auditLog);
	return Runtime::Instance();
}

struct RuntimeGuard {
	~RuntimeGuard() {
		Runtime::Stop();
	}
};

[[nodiscard]] QJsonObject StateResult(const QString &id) {
	return QJsonObject{
		{ QStringLiteral("ok"), true },
		{ QStringLiteral("enabled"),
			Exteraless::Config::Instance().pluginEnabled(id) },
	};
}

[[nodiscard]] QJsonObject ParsedSettings() {
	auto parseError = QJsonParseError{0, QJsonParseError::NoError};
	const auto document = QJsonDocument::fromJson(
		Exteraless::Config::Instance().pluginsSettings().toUtf8(),
		&parseError);
	if (parseError.error != QJsonParseError::NoError
		|| !document.isObject()) {
		return QJsonObject();
	}
	return document.object();
}

[[nodiscard]] QJsonObject SettingsResult(const QString &id) {
	return QJsonObject{
		{ QStringLiteral("ok"), true },
		{ QStringLiteral("settings"), ParsedSettings().value(id).toObject() },
	};
}

[[nodiscard]] QJsonObject SetSetting(
		const QString &id,
		const QString &key,
		const QJsonValue &value) {
	auto config = &Exteraless::Config::Instance();
	auto settings = ParsedSettings();
	auto plugin = settings.value(id).toObject();
	if (value.isNull()) {
		plugin.remove(key);
	} else {
		plugin.insert(key, value);
	}
	if (plugin.isEmpty()) {
		settings.remove(id);
	} else {
		settings.insert(id, plugin);
	}
	config->setPluginsSettings(QString::fromUtf8(
		QJsonDocument(settings).toJson(QJsonDocument::Compact)));
	const auto stored = ParsedSettings().value(id).toObject();
	if (value.isNull()) {
		const auto removed = !stored.contains(key);
		return QJsonObject{
			{ QStringLiteral("ok"), removed },
			{ QStringLiteral("settings"), stored },
			{ QStringLiteral("error"), removed
				? QString()
				: QStringLiteral("The setting was not removed.") },
		};
	}
	const auto accepted = (stored.value(key) == value);
	return QJsonObject{
		{ QStringLiteral("ok"), accepted },
		{ QStringLiteral("settings"), stored },
		{ QStringLiteral("error"), accepted
			? QString()
			: QStringLiteral("Invalid settings value.") },
	};
}

[[nodiscard]] QJsonObject ImportStateResult(const QString &path) {
	auto output = QJsonObject{
		{ QStringLiteral("ok"), false },
		{ QStringLiteral("unknown"), QJsonArray() },
	};
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		output[QStringLiteral("error")] =
			QStringLiteral("Could not read the state file.");
		return output;
	}
	auto parseError = QJsonParseError{0, QJsonParseError::NoError};
	const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError
		|| !document.isObject()) {
		output[QStringLiteral("error")] =
			QStringLiteral("Invalid state file.");
		return output;
	}
	auto unknown = QStringList();
	const auto ok = Exteraless::Config::Instance().fromJson(
		document.object(),
		&unknown);
	auto unknownArray = QJsonArray();
	for (const auto &key : unknown) {
		unknownArray.append(key);
	}
	return QJsonObject{
		{ QStringLiteral("ok"), ok },
		{ QStringLiteral("unknown"), unknownArray },
	};
}

[[nodiscard]] QJsonObject MultiOperation(
		Runtime *runtime,
		const QJsonObject &operation) {
	auto id = operation.value(QStringLiteral("id")).toString();
	auto result = QJsonObject{
		{ QStringLiteral("op"), operation.value(QStringLiteral("op")).toString() },
		{ QStringLiteral("ok"), false },
		{ QStringLiteral("error"), QStringLiteral("Unknown operation.") },
		{ QStringLiteral("loaded"), false },
		{ QStringLiteral("pid"), double(runtime->hostPid()) },
	};
	const auto op = result.value(QStringLiteral("op")).toString();
	if (op == QStringLiteral("enable")) {
		const auto path = operation.value(QStringLiteral("path")).toString();
		if (id.isEmpty() || path.isEmpty()) {
			result[QStringLiteral("error")] =
				QStringLiteral("Missing plugin id or path.");
		} else {
			const auto enabled = runtime->enable(id, path);
			result[QStringLiteral("ok")] = enabled.ok;
			result[QStringLiteral("error")] = enabled.error;
		}
	} else if (op == QStringLiteral("disable")) {
		if (id.isEmpty()) {
			result[QStringLiteral("error")] =
				QStringLiteral("Missing plugin id.");
		} else {
			const auto disabled = runtime->disable(id);
			result[QStringLiteral("ok")] = disabled.ok;
			result[QStringLiteral("error")] = disabled.error;
		}
	} else if (op == QStringLiteral("stop")) {
		runtime->stop();
		result[QStringLiteral("ok")] = true;
		result[QStringLiteral("error")] = QString();
	} else if (op == QStringLiteral("pid")) {
		result[QStringLiteral("ok")] = true;
		result[QStringLiteral("error")] = QString();
	}
	result[QStringLiteral("loaded")] =
		!id.isEmpty() && runtime->isLoaded(id);
	result[QStringLiteral("pid")] = double(runtime->hostPid());
	return result;
}

} // namespace

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName(Exteraless::AppFileName());
	const auto arguments = app.arguments();
	if (arguments.size() < 4) {
		return Usage();
	}
	const auto pluginsRoot = arguments[1];
	const auto auditLog = arguments[2];
	const auto command = arguments[3];
	const auto rest = arguments.mid(4);

	if (command == QStringLiteral("validate")) {
		if (rest.size() != 1) {
			return Usage();
		}
		const auto check = Runtime::ValidatePath(pluginsRoot, rest[0]);
		WriteJson(QJsonObject{
			{ QStringLiteral("ok"), check.ok },
			{ QStringLiteral("path"), check.canonicalPath },
			{ QStringLiteral("error"), check.error },
		});
		return 0;
	}
	if (command == QStringLiteral("state-get")) {
		if (rest.size() != 1) {
			return Usage();
		}
		WriteJson(StateResult(rest[0]));
		return 0;
	}
	if (command == QStringLiteral("state-set")) {
		if (rest.size() != 2
			|| (rest[1] != QStringLiteral("true")
				&& rest[1] != QStringLiteral("false"))) {
			return Usage();
		}
		Exteraless::Config::Instance().setPluginEnabled(
			rest[0],
			rest[1] == QStringLiteral("true"));
		WriteJson(StateResult(rest[0]));
		return 0;
	}
	if (command == QStringLiteral("settings-get")) {
		if (rest.size() != 1) {
			return Usage();
		}
		WriteJson(SettingsResult(rest[0]));
		return 0;
	}
	if (command == QStringLiteral("settings-set")) {
		if (rest.size() != 3) {
			return Usage();
		}
		auto parseError = QJsonParseError{0, QJsonParseError::NoError};
		const auto value = QJsonValue::fromJson(rest[2].toUtf8(), &parseError);
		if (parseError.error != QJsonParseError::NoError) {
			WriteJson(QJsonObject{
				{ QStringLiteral("ok"), false },
				{ QStringLiteral("settings"), QJsonObject() },
				{ QStringLiteral("error"),
					QStringLiteral("Invalid JSON value.") },
			});
			return 0;
		}
		WriteJson(SetSetting(rest[0], rest[1], value));
		return 0;
	}
	if (command == QStringLiteral("import-state")) {
		if (rest.size() != 1) {
			return Usage();
		}
		WriteJson(ImportStateResult(rest[0]));
		return 0;
	}
	if (command == QStringLiteral("enable")) {
		if (rest.size() != 2) {
			return Usage();
		}
		const auto runtime = StartRuntime(pluginsRoot, auditLog);
		const auto enabled = runtime->enable(rest[0], rest[1]);
		const auto guard = RuntimeGuard();
		WriteJson(QJsonObject{
			{ QStringLiteral("ok"), enabled.ok },
			{ QStringLiteral("error"), enabled.error },
			{ QStringLiteral("pid"), double(runtime->hostPid()) },
			{ QStringLiteral("loaded"), runtime->isLoaded(rest[0]) },
		});
		return 0;
	}
	if (command == QStringLiteral("disable")) {
		if (rest.size() != 1) {
			return Usage();
		}
		const auto runtime = StartRuntime(pluginsRoot, auditLog);
		const auto disabled = runtime->disable(rest[0]);
		const auto guard = RuntimeGuard();
		WriteJson(QJsonObject{
			{ QStringLiteral("ok"), disabled.ok },
			{ QStringLiteral("error"), disabled.error },
			{ QStringLiteral("pid"), double(runtime->hostPid()) },
			{ QStringLiteral("loaded"), runtime->isLoaded(rest[0]) },
		});
		return 0;
	}
	if (command == QStringLiteral("stop")) {
		if (!rest.isEmpty()) {
			return Usage();
		}
		const auto runtime = StartRuntime(pluginsRoot, auditLog);
		runtime->stop();
		const auto guard = RuntimeGuard();
		WriteJson(QJsonObject{
			{ QStringLiteral("ok"), true },
			{ QStringLiteral("error"), QString() },
			{ QStringLiteral("pid"), double(runtime->hostPid()) },
			{ QStringLiteral("loaded"), false },
		});
		return 0;
	}
	if (command == QStringLiteral("multi")) {
		if (rest.size() != 1) {
			return Usage();
		}
		auto parseError = QJsonParseError{0, QJsonParseError::NoError};
		const auto document = QJsonDocument::fromJson(
			rest[0].toUtf8(),
			&parseError);
		if (parseError.error != QJsonParseError::NoError
			|| !document.isArray()) {
			return Usage();
		}
		const auto runtime = StartRuntime(pluginsRoot, auditLog);
		auto results = QJsonArray();
		const auto guard = RuntimeGuard();
		for (const auto &value : document.array()) {
			results.append(value.isObject()
				? MultiOperation(runtime, value.toObject())
				: QJsonObject{
					{ QStringLiteral("op"), QString() },
					{ QStringLiteral("ok"), false },
					{ QStringLiteral("error"),
						QStringLiteral("Unknown operation.") },
					{ QStringLiteral("loaded"), false },
					{ QStringLiteral("pid"), double(runtime->hostPid()) },
				});
		}
		WriteJson(results);
		return 0;
	}
	return Usage();
}
