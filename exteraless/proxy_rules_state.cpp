#include "exteraless/proxy_rules_state.h"

namespace Exteraless {

ProxyRuleAction DecideProxyRule(
		ProxyRuleState state,
		bool shouldDisable,
		bool proxyEnabled) {
	auto result = ProxyRuleAction{ .state = state };
	if (result.state.conditionActive
		&& result.state.autoDisabled
		&& proxyEnabled) {
		result.state.autoDisabled = false;
	}
	if (shouldDisable && !result.state.conditionActive) {
		result.state.conditionActive = true;
		if (proxyEnabled) {
			result.state.autoDisabled = true;
			result.disableProxy = true;
		}
	} else if (!shouldDisable && result.state.conditionActive) {
		result.state.conditionActive = false;
		if (result.state.autoDisabled) {
			result.state.autoDisabled = false;
			result.enableProxy = !proxyEnabled;
		}
	}
	return result;
}

} // namespace Exteraless
