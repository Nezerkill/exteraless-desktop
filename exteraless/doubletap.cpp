#include "exteraless/doubletap.h"

#include "exteraless/config.h"

namespace Exteraless {

std::optional<DoubleTapAction> ResolveDoubleTap(int value) {
	switch (value) {
	case -1: return std::nullopt;
	case 0: return DoubleTapAction::None;
	case 1: return DoubleTapAction::SendReaction;
	case 2: return DoubleTapAction::SendReaction;
	case 4: return DoubleTapAction::Reply;
	}
	return DoubleTapAction::None;
}

std::optional<DoubleTapAction> DoubleTapActionFor(bool outgoing) {
	const auto &config = Config::Instance();
	return ResolveDoubleTap(
		outgoing ? config.chatsDoubleTapOut() : config.chatsDoubleTapIn());
}

int SanitizeDoubleTapValue(int value) {
	return (value >= -1 && value <= 12) ? value : -1;
}

} // namespace Exteraless
