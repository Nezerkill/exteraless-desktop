#pragma once

#include "base/basic_types.h"

#include <QString>

class History;

namespace Exteraless::AiSummary {

[[nodiscard]] QString CollectChatText(
	not_null<History*> history,
	int maxMessages = 30,
	int maxChars = 6000);
[[nodiscard]] QString SystemPrompt();
[[nodiscard]] QString WrapForSummary(const QString &text);

} // namespace Exteraless::AiSummary
