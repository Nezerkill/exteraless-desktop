#pragma once

#include <QString>

#include <optional>

namespace Exteraless {

[[nodiscard]] std::optional<QString> EvaluateMathExpression(
	const QString &expression);

} // namespace Exteraless
