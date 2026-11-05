#pragma once

#include <QString>

namespace Exteraless {

enum class Transport {
	Unknown,
	Ethernet,
	WiFi,
	Cellular,
	VPN,
};

[[nodiscard]] bool IsVpnInterfaceName(const QString &name);

[[nodiscard]] Transport CurrentTransport();

} // namespace Exteraless
