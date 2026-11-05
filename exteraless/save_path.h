#pragma once

#include <QString>

namespace Exteraless {

[[nodiscard]] QString SanitizeChatFolderName(const QString &name);

[[nodiscard]] QString ChatSubfolderSavePath(
	const QString &filePath,
	const QString &peerName);

} // namespace Exteraless
