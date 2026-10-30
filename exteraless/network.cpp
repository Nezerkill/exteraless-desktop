#include "exteraless/network.h"

#include <QNetworkInformation>
#include <QNetworkInterface>

namespace Exteraless {
namespace {

[[nodiscard]] bool HasVpnInterface() {
	const auto interfaces = QNetworkInterface::allInterfaces();
	for (const auto &interface : interfaces) {
		if (!interface.flags().testFlag(QNetworkInterface::IsUp)) {
			continue;
		}
		const auto name = interface.name().toLower();
		if (name.startsWith("tun")
			|| name.startsWith("tap")
			|| name.startsWith("wg")
			|| name.startsWith("ppp")
			|| name.startsWith("utun")) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] Transport FromInterfaceNames() {
	const auto interfaces = QNetworkInterface::allInterfaces();
	for (const auto &interface : interfaces) {
		if (!interface.flags().testFlag(QNetworkInterface::IsUp)) {
			continue;
		}
		const auto name = interface.name().toLower();
		if (name.startsWith("wlan")
			|| name.startsWith("wlp")
			|| name.startsWith("wifi")) {
			return Transport::WiFi;
		} else if (name.startsWith("wwan")
			|| name.startsWith("rmnet")
			|| name.startsWith("cdc")) {
			return Transport::Cellular;
		}
	}
	for (const auto &interface : interfaces) {
		if (!interface.flags().testFlag(QNetworkInterface::IsUp)) {
			continue;
		}
		const auto name = interface.name().toLower();
		if (name.startsWith("en")
			|| name.startsWith("eth")) {
			return Transport::Ethernet;
		}
	}
	return Transport::Unknown;
}

} // namespace

Transport CurrentTransport() {
	if (HasVpnInterface()) {
		return Transport::VPN;
	}
	static const auto backendLoaded = QNetworkInformation::loadDefaultBackend();
	if (!backendLoaded) {
		return FromInterfaceNames();
	}
	const auto info = QNetworkInformation::instance();
	if (!info) {
		return FromInterfaceNames();
	}
	switch (info->transportMedium()) {
	case QNetworkInformation::TransportMedium::Ethernet:
		return Transport::Ethernet;
	case QNetworkInformation::TransportMedium::WiFi:
		return Transport::WiFi;
	case QNetworkInformation::TransportMedium::Cellular:
		return Transport::Cellular;
	default:
		return FromInterfaceNames();
	}
}

} // namespace Exteraless
