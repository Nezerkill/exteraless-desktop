#include "exteraless/plugins_box.h"

#include "exteraless/plugins/scanner.h"
#include "core/file_utilities.h"
#include "settings.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"

#include <QDir>
#include <rpl/rpl.h>

namespace Exteraless {
namespace {

[[nodiscard]] QString PluginsDirectory() {
	return cWorkingDir() + u"tdata/plugins"_q;
}

[[nodiscard]] QString RowText(const Plugins::Entry &entry) {
	auto text = entry.name.isEmpty() ? entry.file : entry.name;
	if (!entry.version.isEmpty()) {
		text += u" "_q + entry.version;
	}
	if (!entry.author.isEmpty()) {
		text += u"\n"_q + entry.author;
	}
	text += u"\npermissions: "_q
		+ (entry.permissions.isEmpty()
			? u"none"_q
			: entry.permissions.join(u", "_q));
	if (!entry.error.isEmpty()) {
		text += u"\nerror: "_q + entry.error;
	}
	return text;
}

} // namespace

void ShowPluginsBox(not_null<Window::SessionController*> controller) {
	const auto directory = PluginsDirectory();
	QDir().mkpath(directory);
	const auto result = Plugins::ScanPlugins(directory);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(u"Plugins (%1)"_q.arg(int(result.entries.size())));
		const auto layout = box->verticalLayout();
		if (!result.error.isEmpty()) {
			layout->add(
				object_ptr<Ui::FlatLabel>(
					box,
					rpl::single(result.error),
					st::boxLabel),
				st::boxRowPadding);
		} else if (result.entries.empty()) {
			layout->add(
				object_ptr<Ui::FlatLabel>(
					box,
					rpl::single(
						u"No plugins in "_q + directory + u"."_q),
					st::boxLabel),
				st::boxRowPadding);
		} else {
			for (const auto &entry : result.entries) {
				layout->add(
					object_ptr<Ui::FlatLabel>(
						box,
						rpl::single(RowText(entry)),
						st::boxLabel),
					st::boxRowPadding);
			}
		}

		box->addLeftButton(rpl::single(u"Open folder"_q), [=] {
			QDir().mkpath(directory);
			File::Launch(directory);
		});
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

} // namespace Exteraless
