#pragma once

#include "mtproto/mtproto_request_filter.h"

namespace MTP {
class Instance;
} // namespace MTP

namespace Exteraless {

class GhostMode final {
public:
	static MTP::RequestFilterResult Filter(const MTP::RequestFilterInfo &info);
	static void AllowReadPackets(int count = 1);

private:
	static bool ConsumeReadAllowance();
};

void InstallGhostMode(MTP::Instance &instance);

} // namespace Exteraless
