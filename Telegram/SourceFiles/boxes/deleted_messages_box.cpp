#include "boxes/deleted_messages_box.h"

#include "exteraless/deleted_messages.h"
#include "data/data_peer.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"

#include <QDateTime>
#include <QLocale>

#include <algorithm>
#include <rpl/rpl.h>

namespace Exteraless {

void ShowDeletedMessagesBox(
		Window::SessionController *controller,
		PeerData *peer) {
	if (!controller || !peer) {
		return;
	}
	const auto entries = DeletedMessages::Instance().list(peer->id);

	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(u"Deleted messages"_q);
		const auto layout = box->verticalLayout();

		if (entries.empty()) {
			layout->add(
				object_ptr<Ui::FlatLabel>(
					box,
					rpl::single(u"No deleted messages in this chat."_q),
					st::boxLabel),
				st::boxRowPadding);
		} else {
			const auto limit = std::min<int>(entries.size(), 100);
			for (auto i = 0; i != limit; ++i) {
				const auto &entry = entries[i];
				const auto date = QDateTime::fromSecsSinceEpoch(
					entry.date).toString(u"yyyy-MM-dd HH:mm"_q);
				auto text = entry.fromName.isEmpty()
					? entry.text
					: (entry.fromName + u": "_q + entry.text);
				if (!entry.media.isEmpty()) {
					text += u" [media]"_q;
				}
				layout->add(
					object_ptr<Ui::FlatLabel>(
						box,
						rpl::single(date + u"  "_q + text),
						st::boxLabel),
					st::boxRowPadding);
			}
		}

		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

} // namespace Exteraless
