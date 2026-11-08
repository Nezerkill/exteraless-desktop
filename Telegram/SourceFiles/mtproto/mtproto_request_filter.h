#pragma once

#include "base/basic_types.h"
#include "mtproto/core_types.h"

namespace MTP {

struct RequestFilterInfo {
	mtpRequestId requestId = 0;
	mtpTypeId type = 0;
	const mtpPrime *body = nullptr;
	uint32 bodySize = 0;
};

enum class RequestFilterResult {
	Pass,
	Drop,
};

using RequestFilter = Fn<RequestFilterResult(const RequestFilterInfo&)>;

} // namespace MTP
