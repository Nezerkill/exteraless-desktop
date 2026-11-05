#pragma once

#include <optional>

namespace Exteraless {

enum class DoubleTapAction {
	Telegram = -1,
	None = 0,
	SendReaction = 1,
	Reply = 4,
};

[[nodiscard]] std::optional<DoubleTapAction> ResolveDoubleTap(int value);

[[nodiscard]] std::optional<DoubleTapAction> DoubleTapActionFor(bool outgoing);

[[nodiscard]] int SanitizeDoubleTapValue(int value);

} // namespace Exteraless
