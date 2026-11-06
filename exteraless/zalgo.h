#pragma once

#include <QString>

#include <vector>

namespace Exteraless {

struct ZalgoResult {
	QString text;
	std::vector<int> map;
};

[[nodiscard]] bool HasZalgoRun(const QString &text);

[[nodiscard]] ZalgoResult ApplyZalgoFilterWithMap(const QString &text);

[[nodiscard]] QString ApplyZalgoFilter(const QString &text);

} // namespace Exteraless
