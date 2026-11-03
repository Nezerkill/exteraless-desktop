#include "settings/sections/settings_exteraless.h"

#include "settings/settings_common_session.h"

#include "exteraless/backup.h"
#include "exteraless/config.h"
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
		Exteraless::Config::Instance().ghostSendReadPackets(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendReadPackets(value);
		});
	addGhostToggle(
		u"exteraless/ghost/stories"_q,
		rpl::single(u"Don't send story views"_q),
		{ u"ghost"_q, u"stories"_q },
		Exteraless::Config::Instance().ghostSendReadStories(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendReadStories(value);
		});
	addGhostToggle(
		u"exteraless/ghost/online"_q,
		rpl::single(u"Don't send online status"_q),
		{ u"ghost"_q, u"online"_q },
		Exteraless::Config::Instance().ghostSendOnline(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendOnline(value);
		});
	addGhostToggle(
		u"exteraless/ghost/typing"_q,
		rpl::single(u"Don't send typing status"_q),
		{ u"ghost"_q, u"typing"_q },
		Exteraless::Config::Instance().ghostSendUploadProgress(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendUploadProgress(value);
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
