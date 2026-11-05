#pragma once

#include <QString>

#include <vector>

namespace Exteraless {

struct PanguResult {
	QString text;
	std::vector<int> map;
};

[[nodiscard]] PanguResult ApplyPanguWithMap(const QString &text);

[[nodiscard]] QString ApplyPangu(const QString &text);

} // namespace Exteraless
