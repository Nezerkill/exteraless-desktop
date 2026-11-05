#include "exteraless/plugins/installer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace Exteraless::Plugins {
namespace {

[[nodiscard]] bool ValidExtension(const QString &extension) {
	const auto lower = extension.toLower();
	return (lower == QStringLiteral("plugin"))
		|| (lower == QStringLiteral("py"));
}

[[nodiscard]] QString SanitizeStem(const QString &stem) {
	auto result = QString();
	result.reserve(std::min(int(stem.size()), 64));
	for (const auto ch : stem) {
		if (result.size() == 64) {
			break;
		}
		if (ch.isLetterOrNumber() || ch == '.' || ch == '_' || ch == '-') {
			result += ch;
		} else {
			result += '_';
		}
	}
	while (!result.isEmpty()
		&& (result.front() == '.' || result.front() == '-')) {
		result.remove(0, 1);
	}
	return result;
}

} // namespace

QString SanitizePluginFileName(const QString &name) {
	const auto base = QFileInfo(name).fileName();
	const auto info = QFileInfo(base);
	const auto extension = info.suffix();
	if (!ValidExtension(extension)) {
		return {};
	}
	const auto stem = SanitizeStem(info.completeBaseName());
	if (stem.isEmpty()) {
		return {};
	}
	return stem + u'.' + extension.toLower();
}

QString InstallPluginFile(
		const QString &sourcePath,
		const QString &directory) {
	const auto source = QFileInfo(sourcePath);
	if (!source.isFile()
		|| source.isSymLink()
		|| source.size() <= 0
		|| source.size() > kMaxPluginFileSize) {
		return {};
	}
	const auto name = SanitizePluginFileName(source.fileName());
	if (name.isEmpty() || directory.isEmpty()) {
		return {};
	}
	if (!QDir().mkpath(directory)) {
		return {};
	}
	const auto base = QFileInfo(name);
	for (auto attempt = 0; attempt != 100; ++attempt) {
		const auto candidate = attempt
			? (base.completeBaseName()
				+ QStringLiteral("-%1.").arg(attempt + 1)
				+ base.suffix())
			: name;
		const auto target = QDir(directory).filePath(candidate);
		if (QFileInfo::exists(target)) {
			continue;
		}
		auto input = QFile(sourcePath);
		if (!input.open(QIODevice::ReadOnly)) {
			return {};
		}
		auto output = QFile(target);
		if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
			continue;
		}
		const auto bytes = input.readAll();
		const auto written = output.write(bytes);
		output.close();
		if (written != bytes.size()) {
			QFile::remove(target);
			return {};
		}
		return candidate;
	}
	return {};
}

bool DeletePluginFile(const QString &directory, const QString &file) {
	if (directory.isEmpty() || file.isEmpty()) {
		return false;
	}
	const auto base = QFileInfo(file).fileName();
	if (base != file) {
		return false;
	}
	const auto sanitized = SanitizePluginFileName(base);
	if (sanitized.compare(base, Qt::CaseInsensitive) != 0) {
		return false;
	}
	auto path = QDir(directory).filePath(base);
	auto info = QFileInfo(path);
	if (!info.isFile() && sanitized != base) {
		path = QDir(directory).filePath(sanitized);
		info = QFileInfo(path);
	}
	if (!info.isFile() || info.isSymLink()) {
		return false;
	}
	return QFile::remove(path);
}

} // namespace Exteraless::Plugins
