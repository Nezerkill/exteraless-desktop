#include "exteraless/plugins_box.h"

#include "exteraless/config.h"
#include "exteraless/plugin_runtime.h"
#include "exteraless/plugins/installer.h"
#include "exteraless/plugins/scanner.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "settings.h"
#include "lang/lang_keys.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <rpl/rpl.h>

#include <memory>
#include <vector>

namespace Exteraless {
namespace {

[[nodiscard]] QString PluginsDirectory() {
	return cWorkingDir() + u"tdata/plugins"_q;
}

[[nodiscard]] QString AuditLogPath() {
	return cWorkingDir() + u"tdata/plugins-audit.log"_q;
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

[[nodiscard]] QString ButtonText(const Plugins::Entry &entry) {
	auto text = entry.name.isEmpty() ? entry.file : entry.name;
	if (!entry.version.isEmpty()) {
		text += u" "_q + entry.version;
	}
	return text;
}

struct Row {
	Plugins::Entry entry;
	rpl::variable<bool> enabled;
	bool loaded = false;
	QString error;
	Ui::FlatLabel *details = nullptr;
	Ui::SettingsButton *button = nullptr;
};

[[nodiscard]] QString StateText(const Row &row) {
	if (!row.enabled.current()) {
		return row.error.isEmpty()
			? u"Disabled"_q
			: u"Disabled: "_q + row.error;
	} else if (row.loaded) {
		return u"Enabled (running)"_q;
	}
	return row.error.isEmpty()
		? u"Enabled (not running)"_q
		: u"Enabled (not running): "_q + row.error;
}

[[nodiscard]] QString DetailsText(
		const Plugins::Entry &entry,
		const QString &status) {
	auto text = QString();
	if (!entry.author.isEmpty()) {
		text += entry.author + u"\n"_q;
	}
	text += u"permissions: "_q
		+ (entry.permissions.isEmpty()
			? u"none"_q
			: entry.permissions.join(u", "_q));
	text += u"\nfile: "_q + entry.file;
	text += u"\n"_q + status;
	return text;
}

[[nodiscard]] QString HostStatusText(const Plugins::Runtime &runtime) {
	return runtime.isRunning()
		? u"Plugin host: running"_q
		: u"Plugin host: stopped"_q;
}

[[nodiscard]] QString ReadAuditTail(const QString &path, int maxLines) {
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return u"No audit records yet."_q;
	}
	const auto lines = QString::fromUtf8(file.readAll()).split('\n');
	auto tail = QStringList();
	for (auto i = lines.size() - 1; i >= 0 && tail.size() < maxLines; --i) {
		if (!lines[i].isEmpty()) {
			tail.push_front(lines[i]);
		}
	}
	return tail.isEmpty()
		? u"No audit records yet."_q
		: tail.join('\n');
}

void ReopenPluginsBox(
		not_null<Window::SessionController*> controller,
		not_null<Ui::GenericBox*> box) {
	box->closeBox();
	crl::on_main(controller, [=] {
		ShowPluginsBox(controller);
	});
}

void ExportAuditLog(
		not_null<Window::SessionController*> controller,
		const QString &path) {
	FileDialog::GetWritePath(
		Core::App().getFileDialogParent(),
		u"Export plugin audit log"_q,
		u"Log files (*.log)"_q,
		u"plugins-audit.log"_q,
		[=](QString &&destination) mutable {
			if (destination.isEmpty()) {
				return;
			}
			QFile source(path);
			if (!source.open(QIODevice::ReadOnly)) {
				return;
			}
			QFile target(destination);
			if (!target.open(QIODevice::WriteOnly)) {
				return;
			}
			target.write(source.readAll());
		});
}

void ShowAuditLogBox(
		not_null<Window::SessionController*> controller,
		const QString &path) {
	const auto text = ReadAuditTail(path, 24);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(u"Plugin audit log"_q);
		const auto label = box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(text),
				st::boxLabel),
			st::boxRowPadding);
		label->setSelectable(true);
		box->addLeftButton(rpl::single(u"Export"_q), [=] {
			ExportAuditLog(controller, path);
		});
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

void RefreshRows(
		const std::vector<std::unique_ptr<Row>> &rows,
		Plugins::Runtime *runtime) {
	for (const auto &row : rows) {
		row->loaded = runtime && runtime->isLoaded(row->entry.id);
		row->details->setText(DetailsText(row->entry, StateText(*row)));
	}
}

} // namespace

void ShowPluginsBox(not_null<Window::SessionController*> controller) {
	const auto directory = PluginsDirectory();
	const auto auditLog = AuditLogPath();
	QDir().mkpath(directory);
	Plugins::Runtime::Start(directory, auditLog);
	const auto result = Plugins::ScanPlugins(directory);
	const auto runtime = Plugins::Runtime::Instance();
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(u"Plugins (%1)"_q.arg(int(result.entries.size())));
		const auto layout = box->verticalLayout();
		const auto rows = box->lifetime().make_state<
			std::vector<std::unique_ptr<Row>>>();

		const auto hostStatus = layout->add(
			object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(
					runtime
						? HostStatusText(*runtime)
						: u"Plugin host: stopped"_q),
				st::boxLabel),
			st::boxRowPadding);
		layout->add(
			object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(u"Enabled plugins start when this window opens. The host keeps running until Exteraless exits. Manifest permissions are informational in this stage; network, subprocess, native and out-of-directory file access are denied for every plugin."_q),
				st::boxLabel),
			st::boxRowPadding);
		const auto auditButton = layout->add(
			object_ptr<Ui::SettingsButton>(
				box,
				rpl::single(u"Audit log"_q),
				st::settingsButtonNoIcon));
		auditButton->setClickedCallback([=] {
			ShowAuditLogBox(controller, auditLog);
		});
		const auto installButton = layout->add(
			object_ptr<Ui::SettingsButton>(
				box,
				rpl::single(u"Install plugin..."_q),
				st::settingsButtonNoIcon));
		installButton->setClickedCallback([=] {
			FileDialog::GetOpenPath(
				Core::App().getFileDialogParent(),
				u"Install plugin"_q,
				u"Plugin files (*.plugin *.py);;"_q
					+ FileDialog::AllFilesFilter(),
				[=](FileDialog::OpenResult &&result) {
					if (result.paths.isEmpty()) {
						return;
					}
					const auto installed = Plugins::InstallPluginFile(
						result.paths.front(),
						directory);
					if (installed.isEmpty()) {
						controller->showToast(
							u"Could not install "_q
								+ QFileInfo(
									result.paths.front()).fileName());
						return;
					}
					ReopenPluginsBox(controller, box);
				});
		});

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
		}
		for (const auto &entry : result.entries) {
			if (!entry.error.isEmpty()) {
				layout->add(
					object_ptr<Ui::FlatLabel>(
						box,
						rpl::single(RowText(entry)),
						st::boxLabel),
					st::boxRowPadding);
				continue;
			}
			auto row = std::make_unique<Row>();
			row->entry = entry;
			row->enabled = Config::Instance().pluginEnabled(entry.id);
			const auto rowPtr = row.get();
			row->button = layout->add(
				object_ptr<Ui::SettingsButton>(
					box,
					rpl::single(ButtonText(entry)),
					st::settingsButtonNoIcon));
			row->button->toggleOn(row->enabled.value(), true);
			row->details = layout->add(
				object_ptr<Ui::FlatLabel>(
					box,
					rpl::single(DetailsText(entry, StateText(*row))),
					st::boxLabel),
				st::boxRowPadding);
			const auto deleteLink = layout->add(
				object_ptr<Ui::LinkButton>(box, u"Delete plugin"_q),
				st::boxRowPadding);
			const auto deleteEntry = entry;
			deleteLink->setClickedCallback([=] {
				controller->show(Ui::MakeConfirmBox({
					.text = u"Delete plugin \"%1\"?"_q.arg(
						deleteEntry.name.isEmpty()
							? deleteEntry.file
							: deleteEntry.name),
					.confirmed = [=](Fn<void()> &&close) {
						close();
						if (!Plugins::DeletePluginFile(
								directory,
								deleteEntry.file)) {
							controller->showToast(
								u"Could not delete the plugin file."_q);
							return;
						}
						const auto runtime
							= Plugins::Runtime::Instance();
						if (runtime
							&& runtime->isLoaded(deleteEntry.id)) {
							runtime->disable(deleteEntry.id);
						}
						Config::Instance().setPluginEnabled(
							deleteEntry.id,
							false);
						ReopenPluginsBox(controller, box);
					},
					.confirmText = u"Delete"_q,
				}));
			});
			row->button->setClickedCallback([=] {
				const auto runtime = Plugins::Runtime::Instance();
				if (!runtime) {
					return;
				}
				const auto enable = !rowPtr->enabled.current();
				const auto path = directory + u"/"_q + rowPtr->entry.file;
				if (enable) {
					rowPtr->details->setText(DetailsText(
						rowPtr->entry,
						u"Loading..."_q));
					const auto result = runtime->enable(
						rowPtr->entry.id,
						path);
					Config::Instance().setPluginEnabled(
						rowPtr->entry.id,
						true);
					rowPtr->error = result.error;
				} else {
					const auto result = runtime->disable(
						rowPtr->entry.id);
					Config::Instance().setPluginEnabled(
						rowPtr->entry.id,
						false);
					rowPtr->error = result.error;
				}
				rowPtr->enabled.force_assign(enable);
				RefreshRows(*rows, runtime);
				hostStatus->setText(HostStatusText(*runtime));
			});
			rows->push_back(std::move(row));
		}

		box->addLeftButton(rpl::single(u"Open folder"_q), [=] {
			QDir().mkpath(directory);
			File::Launch(directory);
		});
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });

		box->showFinishes() | rpl::take(1) | rpl::on_next([=] {
			const auto runtime = Plugins::Runtime::Instance();
			if (!runtime) {
				return;
			}
			for (const auto &row : *rows) {
				if (!row->enabled.current()
					|| !row->entry.error.isEmpty()) {
					if (row->entry.id.isEmpty()
						|| !runtime->isLoaded(row->entry.id)) {
						continue;
					}
					const auto result = runtime->disable(
						row->entry.id);
					row->error = result.error;
					continue;
				}
				row->details->setText(DetailsText(
					row->entry,
					u"Enabled (not running)"_q));
				const auto result = runtime->enable(
					row->entry.id,
					directory + u"/"_q + row->entry.file);
				row->error = result.error;
			}
			RefreshRows(*rows, runtime);
			hostStatus->setText(HostStatusText(*runtime));
		}, box->lifetime());
	}));
}

} // namespace Exteraless
