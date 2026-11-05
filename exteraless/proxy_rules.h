#pragma once

#include "exteraless/proxy_rules_state.h"

namespace Exteraless {

enum class ProxyCondition : int {
	None = 0,
	VPN = 1,
	Mobile = 2,
	WiFi = 4,
};

[[nodiscard]] constexpr bool HasProxyCondition(
		int mask,
		ProxyCondition condition) {
	return (mask & int(condition)) != 0;
}

[[nodiscard]] constexpr int WithProxyCondition(
		int mask,
		ProxyCondition condition,
		bool enabled) {
	return enabled
		? (mask | int(condition))
		: (mask & ~int(condition));
}

static_assert(int(ProxyCondition::VPN) == 1);
static_assert(int(ProxyCondition::Mobile) == 2);
static_assert(int(ProxyCondition::WiFi) == 4);
static_assert(HasProxyCondition(1, ProxyCondition::VPN));
static_assert(!HasProxyCondition(0, ProxyCondition::VPN));
static_assert(WithProxyCondition(0, ProxyCondition::WiFi, true) == 4);
static_assert(WithProxyCondition(4, ProxyCondition::WiFi, false) == 0);

void StartProxyRulesWatcher();
void ApplyProxyRules();

} // namespace Exteraless
