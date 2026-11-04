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
		.title = tr::lng_settings_exteraless_exact_counters(),
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
		.title = tr::lng_settings_exteraless_ghost(),
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
		tr::lng_settings_exteraless_ghost_read(),
		{ u"ghost"_q, u"read"_q, u"receipts"_q },
		!Exteraless::Config::Instance().ghostSendReadPackets(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendReadPackets(!value);
		});
	addGhostToggle(
		u"exteraless/ghost/stories"_q,
		tr::lng_settings_exteraless_ghost_stories(),
		{ u"ghost"_q, u"stories"_q },
		!Exteraless::Config::Instance().ghostSendReadStories(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendReadStories(!value);
		});
	addGhostToggle(
		u"exteraless/ghost/online"_q,
		tr::lng_settings_exteraless_ghost_online(),
		{ u"ghost"_q, u"online"_q },
		!Exteraless::Config::Instance().ghostSendOnline(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendOnline(!value);
		});
	addGhostToggle(
		u"exteraless/ghost/typing"_q,
		tr::lng_settings_exteraless_ghost_typing(),
		{ u"ghost"_q, u"typing"_q },
		!Exteraless::Config::Instance().ghostSendUploadProgress(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendUploadProgress(!value);
		});

	builder.addSubsectionTitle({
		.id = u"exteraless/chats"_q,
		.title = tr::lng_settings_exteraless_chats(),
	});

	const auto addChatsToggle = [&](
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
	addChatsToggle(
		u"exteraless/chats/remove-tail"_q,
		tr::lng_settings_exteraless_chats_remove_tail(),
		{ u"chat"_q, u"tail"_q, u"bubble"_q },
		Exteraless::Config::Instance().chatsSettings().removeMessageTail,
		[](bool value) {
			Exteraless::Config::Instance().setChatsRemoveMessageTail(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-reactions-channels"_q,
		tr::lng_settings_exteraless_chats_hide_reactions_channels(),
		{ u"chat"_q, u"reactions"_q, u"channel"_q },
		Exteraless::Config::Instance().chatsSettings().hideReactionsChannels,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsHideReactionsChannels(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-reactions-groups"_q,
		tr::lng_settings_exteraless_chats_hide_reactions_groups(),
		{ u"chat"_q, u"reactions"_q, u"group"_q },
		Exteraless::Config::Instance().chatsSettings().hideReactionsGroups,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsHideReactionsGroups(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-reactions-private"_q,
		tr::lng_settings_exteraless_chats_hide_reactions_private(),
		{ u"chat"_q, u"reactions"_q, u"private"_q },
		Exteraless::Config::Instance().chatsSettings().hideReactionsPrivate,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsHideReactionsPrivate(value);
		});
	addChatsToggle(
		u"exteraless/chats/strip-tracking"_q,
		tr::lng_settings_exteraless_chats_strip_links(),
		{ u"chat"_q, u"links"_q, u"tracking"_q, u"utm"_q },
		Exteraless::Config::Instance().chatsSettings().stripTrackingOnOpen,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsStripTrackingOnOpen(value);
		});
	addChatsToggle(
		u"exteraless/chats/strip-tracking-paste"_q,
		tr::lng_settings_exteraless_chats_strip_paste(),
		{ u"chat"_q, u"paste"_q, u"tracking"_q, u"utm"_q },
		Exteraless::Config::Instance().chatsSettings().stripTrackingOnPaste,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsStripTrackingOnPaste(value);
		});
	addChatsToggle(
		u"exteraless/chats/unread-capsule"_q,
		tr::lng_settings_exteraless_chats_unread_capsule(),
		{ u"chat"_q, u"unread"_q, u"capsule"_q, u"label"_q },
		Exteraless::Config::Instance().chatsSettings().unreadCapsule,
		[](bool value) {
			Exteraless::Config::Instance().setChatsUnreadCapsule(value);
		});
	addChatsToggle(
		u"exteraless/chats/save-deleted"_q,
		tr::lng_settings_exteraless_chats_save_deleted(),
		{ u"chat"_q, u"deleted"_q, u"history"_q, u"ayu"_q },
		Exteraless::Config::Instance().saveDeletedMessages(),
		[](bool value) {
			Exteraless::Config::Instance().setSaveDeletedMessages(value);
		});

	builder.addSubsectionTitle({
		.id = u"exteraless/chats/menu"_q,
		.title = tr::lng_settings_exteraless_chats_menu_title(),
	});

	const auto addMenuToggle = [&](
			QString id,
			rpl::producer<QString> title,
			QStringList keywords,
			bool current,
			Fn<void(bool)> setter) {
		const auto checkbox = builder.addCheckbox({
			.id = std::move(id),
			.title = std::move(title),
			.checked = current,
			.keywords = std::move(keywords),
		});
		if (checkbox) {
			checkbox->checkedChanges(
			) | rpl::on_next([=](bool checked) {
				setter(checked);
			}, checkbox->lifetime());
		}
	};
	const auto menuSettings = Exteraless::Config::Instance().messageMenuSettings();
	addMenuToggle(
		u"exteraless/chats/menu/reply"_q,
		tr::lng_settings_exteraless_chats_menu_reply(),
		{ u"reply"_q, u"menu"_q },
		menuSettings.reply,
		[](bool value) {
			Exteraless::Config::Instance().setChatsMenuReply(value);
		});
	addMenuToggle(
		u"exteraless/chats/menu/edit"_q,
		tr::lng_settings_exteraless_chats_menu_edit(),
		{ u"edit"_q, u"menu"_q },
		menuSettings.edit,
		[](bool value) {
			Exteraless::Config::Instance().setChatsMenuEdit(value);
		});
	addMenuToggle(
		u"exteraless/chats/menu/pin"_q,
		tr::lng_settings_exteraless_chats_menu_pin(),
		{ u"pin"_q, u"menu"_q },
		menuSettings.pin,
		[](bool value) {
			Exteraless::Config::Instance().setChatsMenuPin(value);
		});
	addMenuToggle(
		u"exteraless/chats/menu/copy-text"_q,
		tr::lng_settings_exteraless_chats_menu_copy_text(),
		{ u"copy"_q, u"text"_q, u"menu"_q },
		menuSettings.copyText,
		[](bool value) {
			Exteraless::Config::Instance().setChatsMenuCopyText(value);
		});
	addMenuToggle(
		u"exteraless/chats/menu/copy-link"_q,
		tr::lng_settings_exteraless_chats_menu_copy_link(),
		{ u"copy"_q, u"link"_q, u"menu"_q },
		menuSettings.copyLink,
		[](bool value) {
			Exteraless::Config::Instance().setChatsMenuCopyLink(value);
		});
	addMenuToggle(
		u"exteraless/chats/menu/copy-image"_q,
		tr::lng_settings_exteraless_chats_menu_copy_image(),
		{ u"copy"_q, u"image"_q, u"photo"_q, u"menu"_q },
		menuSettings.copyImage,
		[](bool value) {
			Exteraless::Config::Instance().setChatsMenuCopyImage(value);
		});
	addMenuToggle(
		u"exteraless/chats/menu/forward"_q,
		tr::lng_settings_exteraless_chats_menu_forward(),
		{ u"forward"_q, u"menu"_q },
		menuSettings.forward,
		[](bool value) {
			Exteraless::Config::Instance().setChatsMenuForward(value);
		});
	addMenuToggle(
		u"exteraless/chats/menu/select"_q,
		tr::lng_settings_exteraless_chats_menu_select(),
		{ u"select"_q, u"menu"_q },
		menuSettings.select,
		[](bool value) {
			Exteraless::Config::Instance().setChatsMenuSelect(value);
		});
	addMenuToggle(
		u"exteraless/chats/menu/translate"_q,
		tr::lng_settings_exteraless_chats_menu_translate(),
		{ u"translate"_q, u"menu"_q },
		menuSettings.translate,
		[](bool value) {
			Exteraless::Config::Instance().setChatsMenuTranslate(value);
		});
	addMenuToggle(
		u"exteraless/chats/menu/report"_q,
		tr::lng_settings_exteraless_chats_menu_report(),
		{ u"report"_q, u"menu"_q },
		menuSettings.report,
		[](bool value) {
			Exteraless::Config::Instance().setChatsMenuReport(value);
		});
	addMenuToggle(
		u"exteraless/chats/menu/delete"_q,
		tr::lng_settings_exteraless_chats_menu_delete(),
		{ u"delete"_q, u"menu"_q },
		menuSettings.deleteMsg,
		[](bool value) {
			Exteraless::Config::Instance().setChatsMenuDelete(value);
		});

	builder.addSubsectionTitle({
		.id = u"exteraless/pill-stack"_q,
		.title = tr::lng_settings_exteraless_pill_stack(),
	});

	const auto pillStack = builder.addButton({
		.id = u"exteraless/pill-stack/enabled"_q,
		.title = tr::lng_settings_exteraless_pill_stack_enabled(),
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
		.title = tr::lng_settings_exteraless_pill_stack_visible(),
	});

	const auto addPillToggle = [&](
			Exteraless::PillStackPill pill,
			QString id,
			rpl::producer<QString> title,
			QStringList keywords) {
		const auto checkbox = builder.addCheckbox({
			.id = std::move(id),
			.title = std::move(title),
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
		tr::lng_settings_exteraless_pill_cpu(),
		{ u"cpu"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::Ram,
		u"exteraless/pill-stack/ram"_q,
		tr::lng_settings_exteraless_pill_ram(),
		{ u"ram"_q, u"memory"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::Net,
		u"exteraless/pill-stack/net"_q,
		tr::lng_settings_exteraless_pill_net(),
		{ u"network"_q, u"speed"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::Dc,
		u"exteraless/pill-stack/dc"_q,
		tr::lng_settings_exteraless_pill_dc(),
		{ u"dc"_q, u"ping"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::Proxy,
		u"exteraless/pill-stack/proxy"_q,
		tr::lng_settings_exteraless_pill_proxy(),
		{ u"proxy"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::Cache,
		u"exteraless/pill-stack/cache"_q,
		tr::lng_settings_exteraless_pill_cache(),
		{ u"cache"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::Ghost,
		u"exteraless/pill-stack/ghost"_q,
		tr::lng_settings_exteraless_pill_ghost(),
		{ u"ghost"_q, u"pill"_q });
	addPillToggle(
		Exteraless::PillStackPill::LastSeen,
		u"exteraless/pill-stack/last-seen"_q,
		tr::lng_settings_exteraless_pill_last_seen(),
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
				tr::lng_settings_exteraless_file_filter(tr::now),
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
							tr::lng_settings_exteraless_export_error(tr::now)));
						return;
					}
					controller->show(Ui::MakeInformBox(
						tr::lng_settings_exteraless_export_done(tr::now)));
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
				tr::lng_settings_exteraless_file_filter(tr::now),
				[=](FileDialog::OpenResult &&result) {
					if (result.paths.isEmpty()) {
						return;
					}
					auto file = QFile(result.paths.front());
					if (!file.open(QIODevice::ReadOnly)) {
						controller->show(Ui::MakeInformBox(
							tr::lng_settings_exteraless_import_read_error(
								tr::now)));
						return;
					}
					const auto applied = Exteraless::ApplyBackup(
						QString::fromUtf8(file.readAll()));
					if (!applied.ok) {
						controller->show(Ui::MakeInformBox(
							tr::lng_settings_exteraless_import_failed(
								tr::now,
								lt_error,
								applied.error)));
						return;
					}
					auto text = tr::lng_settings_exteraless_import_done(
						tr::now,
						lt_amount,
						QString::number(applied.applied));
					if (!applied.unknown.isEmpty()) {
						text += u"\n"_q
							+ tr::lng_settings_exteraless_import_unknown(
								tr::now,
								lt_keys,
								applied.unknown.join(", "));
					}
					controller->show(Ui::MakeInformBox(text));
				});
		},
		.keywords = { u"backup"_q, u"import"_q },
	});

	builder.addSubsectionTitle({
		.id = u"exteraless/reset"_q,
		.title = tr::lng_settings_exteraless_reset(),
	});

	builder.addButton({
		.id = u"exteraless/reset/all"_q,
		.title = tr::lng_settings_exteraless_reset_settings(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			if (!controller) {
				return;
			}
			controller->show(Ui::MakeConfirmBox({
				.text = tr::lng_settings_exteraless_reset_confirm(),
				.confirmed = [=] {
					Exteraless::Config::Instance().reset();
					Exteraless::ApplyProxyRules();
					controller->showSettings(ExteralessSectionId());
				},
				.confirmText = tr::lng_settings_exteraless_reset(tr::now),
			}));
		},
		.keywords = { u"reset"_q, u"defaults"_q, u"clear"_q },
	});

	builder.addSubsectionTitle({
		.id = u"exteraless/plugins"_q,
		.title = tr::lng_settings_exteraless_plugins(),
	});

	builder.addButton({
		.id = u"exteraless/plugins/open"_q,
		.title = tr::lng_settings_exteraless_plugins(),
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
