#include "settings/sections/settings_exteraless.h"

#include "settings/settings_common_session.h"

#include "exteraless/backup.h"
#include "exteraless/config.h"
#include "exteraless/plugins_box.h"
#include "exteraless/proxy_rules.h"
#include "core/file_utilities.h"
#include "lang/lang_keys.h"
#include "settings/settings_builder.h"
#include "settings/sections/settings_main.h"
#include "ui/boxes/confirm_box.h"
#include "ui/rp_widget.h"
#include "ui/ui_utility.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

#include <QFile>
#include <QSaveFile>

namespace Settings {
namespace {

class ExteralessSection final : public Section<ExteralessSection> {
public:
	ExteralessSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();
};

const auto kMeta = Builder::BuildHelper({
	.id = ExteralessSection::Id(),
	.parentId = MainId(),
	.title = &tr::lng_settings_exteraless,
	.icon = &st::menuIconSettings,
}, [](Builder::SectionBuilder &builder) {
	builder.addSubsectionTitle({
		.id = u"exteraless/general"_q,
		.title = tr::lng_settings_exteraless_general(),
	});

	const auto relativeLastSeen = builder.addButton({
		.id = u"exteraless/relative-last-seen"_q,
		.title = tr::lng_settings_exteraless_relative_last_seen(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().relativeLastSeen()),
		.keywords = { u"last seen"_q, u"relative"_q },
	});
	if (relativeLastSeen) {
		relativeLastSeen->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value
				!= Exteraless::Config::Instance().relativeLastSeen());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setRelativeLastSeen(value);
		}, relativeLastSeen->lifetime());
	}

	const auto addComma = builder.addButton({
		.id = u"exteraless/add-comma-after-mention"_q,
		.title = tr::lng_settings_exteraless_add_comma(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().addCommaAfterMention()),
		.keywords = { u"mention"_q, u"comma"_q },
	});
	if (addComma) {
		addComma->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value
				!= Exteraless::Config::Instance().addCommaAfterMention());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setAddCommaAfterMention(value);
		}, addComma->lifetime());
	}

	const auto disableRounding = builder.addButton({
		.id = u"exteraless/disable-number-rounding"_q,
		.title = rpl::single(u"Show exact counters (1.23K → 1234)"_q),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().disableNumberRounding()),
		.keywords = { u"numbers"_q, u"rounding"_q, u"counters"_q },
	});
	if (disableRounding) {
		disableRounding->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value
				!= Exteraless::Config::Instance().disableNumberRounding());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setDisableNumberRounding(value);
		}, disableRounding->lifetime());
	}

	builder.addSubsectionTitle({
		.id = u"exteraless/ghost"_q,
		.title = rpl::single(u"Ghost Mode"_q),
	});

	const auto addGhostToggle = [&](
			QString id,
			rpl::producer<QString> title,
			QStringList keywords,
			bool current,
			Fn<void(bool)> setter) {
		const auto toggle = builder.addButton({
			.id = std::move(id),
			.title = std::move(title),
			.st = &st::settingsButtonNoIcon,
			.toggled = rpl::single(current),
			.keywords = std::move(keywords),
		});
		if (toggle) {
			toggle->toggledChanges(
			) | rpl::on_next([=](bool value) {
				setter(value);
			}, toggle->lifetime());
		}
	};
	addGhostToggle(
		u"exteraless/ghost/read"_q,
		rpl::single(u"Don't send read receipts"_q),
		{ u"ghost"_q, u"read"_q, u"receipts"_q },
		!Exteraless::Config::Instance().ghostSendReadPackets(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendReadPackets(!value);
		});
	addGhostToggle(
		u"exteraless/ghost/stories"_q,
		rpl::single(u"Don't send story views"_q),
		{ u"ghost"_q, u"stories"_q },
		!Exteraless::Config::Instance().ghostSendReadStories(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendReadStories(!value);
		});
	addGhostToggle(
		u"exteraless/ghost/online"_q,
		rpl::single(u"Don't send online status"_q),
		{ u"ghost"_q, u"online"_q },
		!Exteraless::Config::Instance().ghostSendOnline(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendOnline(!value);
		});
	addGhostToggle(
		u"exteraless/ghost/typing"_q,
		rpl::single(u"Don't send typing status"_q),
		{ u"ghost"_q, u"typing"_q },
		!Exteraless::Config::Instance().ghostSendUploadProgress(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendUploadProgress(!value);
		});

	builder.addSubsectionTitle({
		.id = u"exteraless/chats"_q,
		.title = rpl::single(u"Chats"_q),
	});

	const auto addChatsToggle = [&](
			QString id,
			QString title,
			QStringList keywords,
			bool current,
			Fn<void(bool)> setter) {
		const auto toggle = builder.addButton({
			.id = std::move(id),
			.title = rpl::single(std::move(title)),
			.st = &st::settingsButtonNoIcon,
			.toggled = rpl::single(current),
			.keywords = std::move(keywords),
		});
		if (toggle) {
			toggle->toggledChanges(
			) | rpl::on_next([=](bool value) {
				setter(value);
			}, toggle->lifetime());
		}
	};
	addChatsToggle(
		u"exteraless/chats/remove-tail"_q,
		u"Remove message tail"_q,
		{ u"chat"_q, u"tail"_q, u"bubble"_q },
		Exteraless::Config::Instance().chatsSettings().removeMessageTail,
		[](bool value) {
			Exteraless::Config::Instance().setChatsRemoveMessageTail(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-reactions-channels"_q,
		u"Hide reactions in channels"_q,
		{ u"chat"_q, u"reactions"_q, u"channel"_q },
		Exteraless::Config::Instance().chatsSettings().hideReactionsChannels,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsHideReactionsChannels(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-reactions-groups"_q,
		u"Hide reactions in groups"_q,
		{ u"chat"_q, u"reactions"_q, u"group"_q },
		Exteraless::Config::Instance().chatsSettings().hideReactionsGroups,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsHideReactionsGroups(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-reactions-private"_q,
		u"Hide reactions in private chats"_q,
		{ u"chat"_q, u"reactions"_q, u"private"_q },
		Exteraless::Config::Instance().chatsSettings().hideReactionsPrivate,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsHideReactionsPrivate(value);
		});
	addChatsToggle(
		u"exteraless/chats/strip-tracking"_q,
		u"Strip tracking parameters from links"_q,
		{ u"chat"_q, u"links"_q, u"tracking"_q, u"utm"_q },
		Exteraless::Config::Instance().chatsSettings().stripTrackingOnOpen,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsStripTrackingOnOpen(value);
		});
	addChatsToggle(
		u"exteraless/chats/strip-tracking-paste"_q,
		u"Strip tracking parameters from pasted text"_q,
		{ u"chat"_q, u"paste"_q, u"tracking"_q, u"utm"_q },
		Exteraless::Config::Instance().chatsSettings().stripTrackingOnPaste,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsStripTrackingOnPaste(value);
		});
	addChatsToggle(
		u"exteraless/chats/unread-capsule"_q,
		u"Compact unread messages label"_q,
		{ u"chat"_q, u"unread"_q, u"capsule"_q, u"label"_q },
		Exteraless::Config::Instance().chatsSettings().unreadCapsule,
		[](bool value) {
			Exteraless::Config::Instance().setChatsUnreadCapsule(value);
		});
	addChatsToggle(
		u"exteraless/chats/save-deleted"_q,
		u"Save deleted messages"_q,
		{ u"chat"_q, u"deleted"_q, u"history"_q, u"ayu"_q },
		Exteraless::Config::Instance().saveDeletedMessages(),
		[](bool value) {
			Exteraless::Config::Instance().setSaveDeletedMessages(value);
		});

	builder.addSubsectionTitle({
		.id = u"exteraless/pill-stack"_q,
		.title = rpl::single(u"Pill Stack"_q),
	});

	const auto pillStack = builder.addButton({
		.id = u"exteraless/pill-stack/enabled"_q,
		.title = rpl::single(u"Show pills above the chat list"_q),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().pillStackEnabled()),
		.keywords = { u"pill"_q, u"stack"_q, u"cpu"_q, u"ram"_q },
	});
	if (pillStack) {
		pillStack->toggledChanges(
		) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setPillStackEnabled(value);
		}, pillStack->lifetime());
	}

	builder.addSubsectionTitle({
		.id = u"exteraless/pill-stack/pills"_q,
		.title = rpl::single(u"Visible pills"_q),
	});

	const auto addPillToggle = [&](
			Exteraless::PillStackPill pill,
			QString id,
			QString title,
			QStringList keywords) {
		const auto checkbox = builder.addCheckbox({
			.id = std::move(id),
			.title = rpl::single(std::move(title)),
			.checked = Exteraless::HasPillStackPill(
				Exteraless::Config::Instance().pillStackPills(),
				pill),
			.keywords = std::move(keywords),
		});
		if (checkbox) {
			checkbox->checkedChanges(
			) | rpl::on_next([=](bool checked) {
				const auto mask = Exteraless::WithPillStackPill(
					Exteraless::Config::Instance().pillStackPills(),
					pill,
					checked);
				Exteraless::Config::Instance().setPillStackPills(mask);
			}, checkbox->lifetime());
		}
	};
	addPillToggle(
		Exteraless::PillStackPill::Cpu,
		u"exteraless/pill-stack/cpu"_q,
		u"CPU"_q,
		{ u"cpu"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::Ram,
		u"exteraless/pill-stack/ram"_q,
		u"RAM"_q,
		{ u"ram"_q, u"memory"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::Net,
		u"exteraless/pill-stack/net"_q,
		u"Network speed"_q,
		{ u"network"_q, u"speed"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::Dc,
		u"exteraless/pill-stack/dc"_q,
		u"DC ping"_q,
		{ u"dc"_q, u"ping"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::Proxy,
		u"exteraless/pill-stack/proxy"_q,
		u"Proxy"_q,
		{ u"proxy"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::Cache,
		u"exteraless/pill-stack/cache"_q,
		u"Cache"_q,
		{ u"cache"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::Ghost,
		u"exteraless/pill-stack/ghost"_q,
		u"Ghost Mode"_q,
		{ u"ghost"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::LastSeen,
		u"exteraless/pill-stack/last-seen"_q,
		u"Last seen"_q,
		{ u"last seen"_q, u"pill"_q });

	builder.addSubsectionTitle({
		.id = u"exteraless/proxy"_q,
		.title = tr::lng_settings_exteraless_proxy(),
	});

	const auto addProxyCondition = [&](
			Exteraless::ProxyCondition condition,
			QString id,
			rpl::producer<QString> title,
			QStringList keywords) {
		const auto checkbox = builder.addCheckbox({
			.id = std::move(id),
			.title = std::move(title),
			.checked = Exteraless::HasProxyCondition(
				Exteraless::Config::Instance().proxyDisableConditions(),
				condition),
			.keywords = std::move(keywords),
		});
		if (checkbox) {
			checkbox->checkedChanges(
			) | rpl::on_next([=](bool checked) {
				const auto mask = Exteraless::WithProxyCondition(
					Exteraless::Config::Instance().proxyDisableConditions(),
					condition,
					checked);
				Exteraless::Config::Instance()
					.setProxyDisableConditions(mask);
				Exteraless::ApplyProxyRules();
			}, checkbox->lifetime());
		}
	};
	addProxyCondition(
		Exteraless::ProxyCondition::VPN,
		u"exteraless/proxy/vpn"_q,
		tr::lng_settings_exteraless_proxy_vpn(),
		{ u"vpn"_q, u"proxy"_q });
	addProxyCondition(
		Exteraless::ProxyCondition::Mobile,
		u"exteraless/proxy/mobile"_q,
		tr::lng_settings_exteraless_proxy_mobile(),
		{ u"mobile"_q, u"cellular"_q, u"proxy"_q });
	addProxyCondition(
		Exteraless::ProxyCondition::WiFi,
		u"exteraless/proxy/wifi"_q,
		tr::lng_settings_exteraless_proxy_wifi(),
		{ u"wifi"_q, u"proxy"_q });

	builder.addSubsectionTitle({
		.id = u"exteraless/backup"_q,
		.title = tr::lng_settings_exteraless_backup(),
	});

	const auto controller = builder.controller();

	builder.addButton({
		.id = u"exteraless/backup/export"_q,
		.title = tr::lng_settings_exteraless_export(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			if (!controller) {
				return;
			}
			FileDialog::GetWritePath(
				nullptr,
				tr::lng_settings_exteraless_export(tr::now),
				u"Exteraless backup (*.extera)"_q,
				u"exteraless.extera"_q,
				[=](QString &&path) {
					if (path.isEmpty()) {
						return;
					}
					auto file = QSaveFile(path);
					if (!file.open(QIODevice::WriteOnly)
						|| !file.write(
							Exteraless::BuildBackup().toUtf8())
						|| !file.commit()) {
						controller->show(Ui::MakeInformBox(
							"Could not export settings."));
						return;
					}
					controller->show(Ui::MakeInformBox(
						"Settings exported."));
				});
		},
		.keywords = { u"backup"_q, u"export"_q },
	});

	builder.addButton({
		.id = u"exteraless/backup/import"_q,
		.title = tr::lng_settings_exteraless_import(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			if (!controller) {
				return;
			}
			FileDialog::GetOpenPath(
				nullptr,
				tr::lng_settings_exteraless_import(tr::now),
				u"Exteraless backup (*.extera)"_q,
				[=](FileDialog::OpenResult &&result) {
					if (result.paths.isEmpty()) {
						return;
					}
					auto file = QFile(result.paths.front());
					if (!file.open(QIODevice::ReadOnly)) {
						controller->show(Ui::MakeInformBox(
							"Could not read the backup."));
						return;
					}
					const auto applied = Exteraless::ApplyBackup(
						QString::fromUtf8(file.readAll()));
					if (!applied.ok) {
						controller->show(Ui::MakeInformBox(
							u"Could not import settings: %1"_q
								.arg(applied.error)));
						return;
					}
					auto text = u"Settings imported: %1."_q
						.arg(applied.applied);
					if (!applied.unknown.isEmpty()) {
						text += u"\nUnknown keys: %1"_q
							.arg(applied.unknown.join(", "));
					}
					controller->show(Ui::MakeInformBox(text));
				});
		},
		.keywords = { u"backup"_q, u"import"_q },
	});

	builder.addSubsectionTitle({
		.id = u"exteraless/reset"_q,
		.title = rpl::single(u"Reset"_q),
	});

	builder.addButton({
		.id = u"exteraless/reset/all"_q,
		.title = rpl::single(u"Reset Exteraless settings"_q),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			if (!controller) {
				return;
			}
			controller->show(Ui::MakeConfirmBox({
				.text = rpl::single(
					u"Reset all Exteraless settings to defaults? "
					u"This cannot be undone."_q),
				.confirmed = [=] {
					Exteraless::Config::Instance().reset();
					Exteraless::ApplyProxyRules();
					controller->showSettings(ExteralessSectionId());
				},
				.confirmText = u"Reset"_q,
			}));
		},
		.keywords = { u"reset"_q, u"defaults"_q, u"clear"_q },
	});

	builder.addSubsectionTitle({
		.id = u"exteraless/plugins"_q,
		.title = rpl::single(u"Plugins"_q),
	});

	builder.addButton({
		.id = u"exteraless/plugins/open"_q,
		.title = rpl::single(u"Plugins"_q),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			if (!controller) {
				return;
			}
			Exteraless::ShowPluginsBox(controller);
		},
		.keywords = { u"plugins"_q, u"plugin"_q },
	});
});

const auto kBuild = kMeta.build;

ExteralessSection::ExteralessSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> ExteralessSection::title() {
	return tr::lng_settings_exteraless();
}

void ExteralessSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kBuild);
	Ui::ResizeFitChild(this, content);
}

} // namespace

Type ExteralessSectionId() {
	return ExteralessSection::Id();
}

} // namespace Settings
