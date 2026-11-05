#pragma once

#include <QtGlobal>
#include <QString>

namespace Exteraless::Plugins {

constexpr auto kMaxPluginFileSize = qint64(2) * 1024 * 1024;

[[nodiscard]] QString SanitizePluginFileName(const QString &name);

[[nodiscard]] QString InstallPluginFile(
	const QString &sourcePath,
	const QString &directory);

[[nodiscard]] bool DeletePluginFile(
	const QString &directory,
	const QString &file);

} // namespace Exteraless::Plugins
