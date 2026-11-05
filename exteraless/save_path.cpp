#include "exteraless/save_path.h"

#include <QChar>
#include <QDir>
#include <QFileInfo>

namespace Exteraless {
namespace {

constexpr auto kMaxFolderNameLength = 64;
constexpr auto kForbidden = "<>:\"/\\|?*";

} // namespace

QString SanitizeChatFolderName(const QString &name) {
	auto result = QString();
	result.reserve(kMaxFolderNameLength);
	for (const auto ch : name) {
		if (result.size() == kMaxFolderNameLength) {
			break;
		}
		if (ch.unicode() < 32
			|| QStringLiteral("<>:\"/\\|?*").contains(ch)) {
			result += QChar::fromLatin1('_');
		} else {
			result += ch;
		}
	}
	result = result.trimmed();
	while (!result.isEmpty()
		&& (result.back() == QChar::fromLatin1('.')
			|| result.back() == QChar::fromLatin1(' '))) {
		result.chop(1);
	}
	return result.isEmpty() ? QStringLiteral("Chat") : result;
}

QString ChatSubfolderSavePath(
		const QString &filePath,
		const QString &peerName) {
	const auto info = QFileInfo(filePath);
	const auto folder = info.absolutePath()
		+ QChar::fromLatin1('/')
		+ SanitizeChatFolderName(peerName);
	if (!QDir().mkpath(folder)) {
		return filePath;
	}
	const auto fileName = info.fileName();
	const auto direct = folder + QChar::fromLatin1('/') + fileName;
	if (!QFileInfo::exists(direct)) {
		return direct;
	}
	const auto dot = fileName.lastIndexOf(QChar::fromLatin1('.'));
	const auto start = (dot > 0) ? fileName.left(dot) : fileName;
	const auto extension = (dot > 0) ? fileName.mid(dot) : QString();
	for (auto i = 2; i != 100; ++i) {
		const auto candidate = folder
			+ QChar::fromLatin1('/')
			+ start
			+ QStringLiteral(" (%1)").arg(i)
			+ extension;
		if (!QFileInfo::exists(candidate)) {
			return candidate;
		}
	}
	return filePath;
}

} // namespace Exteraless
