#include "exteraless/proxy_rules.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "exteraless/config.h"
#include "exteraless/network.h"
#include "mtproto/mtproto_proxy_data.h"

#include <QTimer>
#include <memory>

namespace Exteraless {
namespace {

constexpr auto kVpn = 1;
constexpr auto kMobile = 2;
constexpr auto kWiFi = 4;

[[nodiscard]] int ConditionFor(Transport transport) {
	switch (transport) {
	case Transport::VPN: return kVpn;
	case Transport::Cellular: return kMobile;
	case Transport::WiFi: return kWiFi;
	default: return 0;
	}
}

} // namespace

void ApplyProxyRules() {
	auto &config = Config::Instance();
	const auto condition = ConditionFor(CurrentTransport());
	const auto shouldDisable = (condition != 0)
		&& ((config.proxyDisableConditions() & condition) != 0);
	auto &proxy = Core::App().settings().proxy();

	if (shouldDisable) {
		if (!proxy.isEnabled()) {
			return;
		}
		config.setProxyAutoDisabled(true);
		Core::App().setCurrentProxy(
			proxy.selected(),
			MTP::ProxyData::Settings::Disabled);
		Core::App().saveSettingsDelayed();
	} else if (config.proxyAutoDisabled()) {
		config.setProxyAutoDisabled(false);
		if (proxy.settings() == MTP::ProxyData::Settings::Disabled) {
			Core::App().setCurrentProxy(
				proxy.selected(),
				MTP::ProxyData::Settings::Enabled);
			Core::App().saveSettingsDelayed();
		}
	}
}

void StartProxyRulesWatcher() {
	static const auto timer = [] {
		auto result = std::make_unique<QTimer>();
		result->setInterval(2000);
		QObject::connect(
			result.get(),
			&QTimer::timeout,
			&Core::App(),
			[] { ApplyProxyRules(); });
		result->start();
		return result;
	}();
}

} // namespace Exteraless
