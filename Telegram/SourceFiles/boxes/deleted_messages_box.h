#pragma once

namespace Window {
class SessionController;
} // namespace Window

class PeerData;

namespace Exteraless {

void ShowDeletedMessagesBox(
	Window::SessionController *controller,
	PeerData *peer);

} // namespace Exteraless
