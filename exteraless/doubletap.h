#pragma once

#include <optional>

namespace Exteraless {

enum class DoubleTapAction {
	Telegram = -1,
	None = 0,
	SendReaction = 1,
	Translate = 3,
	Reply = 4,
	Save = 5,
	Repeat = 6,
	RepeatAsCopy = 7,
	Edit = 8,
	Delete = 10,
	Forward = 11,
	Download = 12,
};

[[nodiscard]] std::optional<DoubleTapAction> ResolveDoubleTap(int value);

[[nodiscard]] std::optional<DoubleTapAction> DoubleTapActionFor(bool outgoing);

[[nodiscard]] int SanitizeDoubleTapValue(int value);

} // namespace Exteraless
