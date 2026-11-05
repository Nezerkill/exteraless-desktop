#pragma once

#include <QtGlobal>

namespace Exteraless {

struct GhostScheduleInput {
	bool enabled = false;
	bool sendOnlinePackets = true;
	bool self = false;
	bool secret = false;
	bool bot = false;
	bool user = false;
};

[[nodiscard]] bool ShouldGhostSchedule(const GhostScheduleInput &input);

constexpr auto kGhostScheduleDelay = 12;

} // namespace Exteraless
