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

[[nodiscard]] ProxyCondition ConditionFor(Transport transport) {
	switch (transport) {
	case Transport::VPN: return ProxyCondition::VPN;
	case Transport::Cellular: return ProxyCondition::Mobile;
	case Transport::WiFi: return ProxyCondition::WiFi;
	default: return ProxyCondition::None;
	}
}

} // namespace

void ApplyProxyRules() {
	auto &config = Config::Instance();
	const auto condition = ConditionFor(CurrentTransport());
	const auto shouldDisable = (condition != ProxyCondition::None)
		&& HasProxyCondition(config.proxyDisableConditions(), condition);
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
