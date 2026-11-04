#include "exteraless/chats.h"

#include "exteraless/config.h"

namespace Exteraless {

bool HiddenReactionsFor(
		const ChatsSettings &settings,
		bool channel,
		bool group,
		bool user) {
	return (channel && settings.hideReactionsChannels)
		|| (group && settings.hideReactionsGroups)
		|| (user && settings.hideReactionsPrivate);
}

bool HiddenReactionsFor(bool channel, bool group, bool user) {
	return HiddenReactionsFor(
		Config::Instance().chatsSettings(),
		channel,
		group,
		user);
}

bool UnreadCapsuleEnabled() {
	return Config::Instance().chatsSettings().unreadCapsule;
}

} // namespace Exteraless
