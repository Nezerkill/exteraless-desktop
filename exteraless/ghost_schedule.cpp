#include "exteraless/ghost_schedule.h"

namespace Exteraless {

bool ShouldGhostSchedule(const GhostScheduleInput &input) {
	if (!input.enabled || input.sendOnlinePackets) {
		return false;
	}
	if (input.self || input.secret) {
		return false;
	}
	return !input.user || !input.bot;
}

} // namespace Exteraless
