#include "exteraless/doubletap.h"

#include "exteraless/config.h"

namespace Exteraless {

std::optional<DoubleTapAction> ResolveDoubleTap(int value) {
	switch (value) {
	case -1: return std::nullopt;
	case 0: return DoubleTapAction::None;
	case 1: return DoubleTapAction::SendReaction;
	case 2: return DoubleTapAction::SendReaction;
	case 3: return DoubleTapAction::Translate;
	case 4: return DoubleTapAction::Reply;
	case 5: return DoubleTapAction::Save;
	// WHY: repeat and repeat-as-copy need an AI provider this build has no
	// configuration for, and 9 has no desktop API at all, so all three stay
	// inert instead of failing at the call site.
	case 6: return DoubleTapAction::None;
	case 7: return DoubleTapAction::None;
	case 8: return DoubleTapAction::Edit;
	case 9: return DoubleTapAction::None;
	case 10: return DoubleTapAction::Delete;
	case 11: return DoubleTapAction::Forward;
	case 12: return DoubleTapAction::Download;
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
