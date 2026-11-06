#pragma once

namespace Exteraless {

struct ChatsSettings {
	bool removeMessageTail = true;
	bool hideReactionsChannels = false;
	bool hideReactionsGroups = false;
	bool hideReactionsPrivate = false;
	bool stripTrackingOnOpen = true;
	bool stripTrackingOnPaste = true;
	bool unreadCapsule = false;
	bool stickerTimeSide = false;
	bool hideStickerTime = false;
	int stickerSize = 14;
	int stickerShape = 0;
	bool replyColors = true;
	bool replyEmoji = true;
	bool replyBackground = true;
	bool hideStoriesHeader = false;
	bool hideArchive = false;
	bool sortByUnread = false;
	bool wideChannelPosts = false;
	bool hideGreetingSticker = false;
	bool hidePanelsOnScroll = false;
	int channelBottomButton = 1;
	int downloadSpeedBoost = 0;
	bool uploadBoost = false;
	bool disableAutoDownloadingArchive = false;
	bool blockWin32Executables = true;
	int avatarRadius = -1; // -1 circle, 0 square, N > 0 is N px
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

[[nodiscard]] constexpr bool ArchiveRowSkipped(
		bool inMainMenu,
		bool hideArchive) {
	return inMainMenu || hideArchive;
}

} // namespace Exteraless
