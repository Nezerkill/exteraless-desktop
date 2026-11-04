#pragma once

namespace Exteraless {

struct ChatsSettings {
	bool removeMessageTail = true;
	bool hideReactionsChannels = false;
	bool hideReactionsGroups = false;
	bool hideReactionsPrivate = false;
	bool stripTrackingOnOpen = true;
	bool unreadCapsule = false;
};

[[nodiscard]] bool HiddenReactionsFor(
	const ChatsSettings &settings,
	bool channel,
	bool group,
	bool user);

[[nodiscard]] bool HiddenReactionsFor(
	bool channel,
	bool group,
	bool user);

[[nodiscard]] bool UnreadCapsuleEnabled();

} // namespace Exteraless
