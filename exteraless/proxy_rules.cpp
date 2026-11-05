#include "exteraless/proxy_rules.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "exteraless/config.h"
#include "exteraless/network.h"
#include "exteraless/proxy_rules_state.h"
#include "mtproto/mtproto_proxy_data.h"

#include <QTimer>
#include <memory>

namespace Exteraless {
namespace {

ProxyRuleState RuleState;

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
	const auto &config = Config::Instance();
	const auto condition = ConditionFor(CurrentTransport());
	const auto shouldDisable = (condition != ProxyCondition::None)
		&& HasProxyCondition(config.proxyDisableConditions(), condition);
	auto &proxy = Core::App().settings().proxy();

	const auto action = DecideProxyRule(
		RuleState,
		shouldDisable,
		proxy.isEnabled());
	RuleState = action.state;
	if (action.disableProxy) {
		Core::App().setCurrentProxy(
			proxy.selected(),
			MTP::ProxyData::Settings::Disabled);
		Core::App().saveSettingsDelayed();
	} else if (action.enableProxy) {
		Core::App().setCurrentProxy(
			proxy.selected(),
			MTP::ProxyData::Settings::Enabled);
		Core::App().saveSettingsDelayed();
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
