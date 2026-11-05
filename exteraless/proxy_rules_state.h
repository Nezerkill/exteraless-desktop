#pragma once

namespace Exteraless {

struct ProxyRuleState {
	bool conditionActive = false;
	bool autoDisabled = false;
};

struct ProxyRuleAction {
	ProxyRuleState state;
	bool disableProxy = false;
	bool enableProxy = false;
};

[[nodiscard]] ProxyRuleAction DecideProxyRule(
	ProxyRuleState state,
	bool shouldDisable,
	bool proxyEnabled);

} // namespace Exteraless
