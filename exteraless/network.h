#pragma once

namespace Exteraless {

enum class Transport {
	Unknown,
	Ethernet,
	WiFi,
	Cellular,
	VPN,
};

[[nodiscard]] Transport CurrentTransport();

} // namespace Exteraless
