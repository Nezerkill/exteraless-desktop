#include "settings/sections/settings_exteraless.h"

#include "settings/settings_common_session.h"

#include "exteraless/backup.h"
#include "exteraless/config.h"
#include "exteraless/doubletap.h"
#include "exteraless/plugins_box.h"
#include "exteraless/proxy_rules.h"
#include "base/openssl_help.h"
#include "base/options.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/file_utilities.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "settings.h"
#include "settings/cloud_password/settings_cloud_password_common.h"
#include "settings/sections/settings_local_passcode.h"
#include "storage/storage_domain.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/widgets/labels.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "data/data_session.h"
#include "data/data_chat_filters.h"
#include "data/data_user.h"
#include "dialogs/dialogs_entry.h"
#include "lang/lang_keys.h"
#include "settings/settings_builder.h"
#include "settings/sections/settings_main.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/rp_widget.h"
#include "ui/ui_utility.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/discrete_sliders.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

#include <QFile>
#include <QSaveFile>

namespace Settings {
namespace {

constexpr auto kStickerSizeMin = 4;
constexpr auto kStickerSizeMax = 20;

class ExteralessSection final : public Section<ExteralessSection> {
public:
	ExteralessSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();
};

bool &FalseBottomUnlocked() {
	static auto result = false;
	return result;
}

void FalseBottomUnlockBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		Fn<void()> unlocked) {
	box->setTitle(tr::lng_settings_exteraless_false_bottom());
	box->setWidth(st::boxWideWidth);
	box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_settings_exteraless_false_bottom_about(),
			st::boxLabel),
		st::boxRowPadding);
	const auto field = CloudPassword::AddPasswordField(
		box->verticalLayout(),
		tr::lng_settings_exteraless_false_bottom_enter(),
		QString());
	const auto error = CloudPassword::AddError(box->verticalLayout(), field);
	const auto submit = [=] {
		if (!passcodeCanTry()) {
			field->showError();
			error->show();
			error->setText(tr::lng_flood_error(tr::now));
			return;
		}
		auto utf8 = field->text().toUtf8();
		const auto cleanse = gsl::finally([&] {
			if (!utf8.isEmpty()) {
				OPENSSL_cleanse(utf8.data(), utf8.size());
			}
		});
		const auto session = &controller->session();
		Storage::DeriveOnWorker(
			session->domain().local().prepareOpen(utf8),
			crl::guard(box, [=](Storage::PasscodeDerivation &&derived) {
				if (session->domain().local().checkPasscode(
						std::move(derived))) {
					cSetPasscodeBadTries(0);
					unlocked();
					return;
				}
				cSetPasscodeBadTries(cPasscodeBadTries() + 1);
				cSetPasscodeLastTry(crl::now());
				field->selectAll();
				field->setFocus();
				field->showError();
				error->show();
				error->setText(tr::lng_passcode_wrong(tr::now));
			}));
	};
	box->addButton(tr::lng_settings_exteraless_false_bottom_unlock(), submit);
	QObject::connect(field, &Ui::MaskedInputField::submitted, submit);
	box->setFocusCallback([=] { field->setFocusFast(); });
}

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

	const auto updateChannelLabel = [](int value) {
		switch (value) {
		case 0:
			return tr::lng_settings_exteraless_update_channel_off(tr::now);
		case 2:
			return tr::lng_settings_exteraless_update_channel_beta(tr::now);
		}
		return tr::lng_settings_exteraless_update_channel_release(tr::now);
	};
	const auto updateChannel = std::make_shared<rpl::variable<QString>>(
		updateChannelLabel(
			Exteraless::Config::Instance().autoUpdateChannel()));
	builder.addButton({
		.id = u"exteraless/update-channel"_q,
		.title = tr::lng_settings_exteraless_update_channel(),
		.st = &st::settingsButtonNoIcon,
		.label = updateChannel->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
				Exteraless::Config::Instance().autoUpdateChannel());
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(tr::lng_settings_exteraless_update_channel());
				const auto inner = box->addRow(
					object_ptr<Ui::VerticalLayout>(box));
				for (auto value = 0; value != 3; ++value) {
					inner->add(
						object_ptr<Ui::Radioenum<int>>(
							inner,
							group,
							value,
							updateChannelLabel(value),
							st::settingsSendType),
						st::settingsSendTypePadding);
				}
				group->setChangedCallback([=](int value) {
					Exteraless::Config::Instance().setAutoUpdateChannel(
						value);
					updateChannel->force_assign(updateChannelLabel(value));
					box->closeBox();
				});
				box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"update"_q, u"channel"_q, u"beta"_q },
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

	const auto persianCalendar = builder.addButton({
		.id = u"exteraless/use-persian-calendar"_q,
		.title = tr::lng_settings_exteraless_persian_calendar(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().usePersianCalendar()),
		.keywords = { u"persian"_q, u"calendar"_q, u"jalali"_q },
	});
	const auto persianLatin = builder.addButton({
		.id = u"exteraless/persian-calendar-latin"_q,
		.title = tr::lng_settings_exteraless_persian_calendar_latin(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().displayPersianCalendarByLatin()),
		.keywords = { u"persian"_q, u"latin"_q, u"digits"_q },
	});
	if (persianCalendar) {
		persianCalendar->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value
				!= Exteraless::Config::Instance().usePersianCalendar());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setUsePersianCalendar(value);
		}, persianCalendar->lifetime());
	}
	if (persianLatin) {
		persianLatin->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value
				!= Exteraless::Config::Instance()
					.displayPersianCalendarByLatin());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance()
				.setDisplayPersianCalendarByLatin(value);
		}, persianLatin->lifetime());
	}
	if (persianCalendar && persianLatin) {
		persianLatin->setEnabled(
			Exteraless::Config::Instance().usePersianCalendar());
		persianCalendar->toggledChanges(
		) | rpl::on_next([=](bool value) {
			persianLatin->setEnabled(value);
		}, persianCalendar->lifetime());
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

	const auto showSeconds = builder.addButton({
		.id = u"exteraless/show-seconds"_q,
		.title = tr::lng_settings_exteraless_show_seconds(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().showSeconds()),
		.keywords = { u"time"_q, u"seconds"_q, u"clock"_q },
	});
	if (showSeconds) {
		showSeconds->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value != Exteraless::Config::Instance().showSeconds());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setShowSeconds(value);
		}, showSeconds->lifetime());
	}

	const auto showIdDc = builder.addButton({
		.id = u"exteraless/show-id-dc"_q,
		.title = tr::lng_settings_exteraless_show_id_dc(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().showIdDc()),
		.keywords = { u"id"_q, u"dc"_q, u"profile"_q },
	});
	if (showIdDc) {
		showIdDc->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value != Exteraless::Config::Instance().showIdDc());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setShowIdDc(value);
		}, showIdDc->lifetime());
	}

	const auto hideAi = builder.addButton({
		.id = u"exteraless/hide-ai-editor"_q,
		.title = tr::lng_settings_exteraless_hide_ai_editor(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().hideAiEditor()),
		.keywords = { u"ai"_q, u"editor"_q, u"hide"_q },
	});
	if (hideAi) {
		hideAi->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value
				!= Exteraless::Config::Instance().hideAiEditor());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setHideAiEditor(value);
		}, hideAi->lifetime());
	}

	const auto decorationLabel = [](int value) {
		switch (value) {
		case 1: return tr::lng_settings_exteraless_decoration_snow(tr::now);
		case 2:
			return tr::lng_settings_exteraless_decoration_fireworks(tr::now);
		case 3:
			return tr::lng_settings_exteraless_decoration_date(tr::now);
		}
		return tr::lng_settings_exteraless_decoration_off(tr::now);
	};
	const auto decoration = std::make_shared<rpl::variable<QString>>(
		decorationLabel(Exteraless::Config::Instance().decoration()));
	builder.addButton({
		.id = u"exteraless/chat-decoration"_q,
		.title = tr::lng_settings_exteraless_decoration(),
		.st = &st::settingsButtonNoIcon,
		.label = decoration->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
				Exteraless::Config::Instance().decoration());
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(tr::lng_settings_exteraless_decoration());
				const auto inner = box->addRow(
					object_ptr<Ui::VerticalLayout>(box));
				for (auto value = 0; value != 4; ++value) {
					inner->add(
						object_ptr<Ui::Radioenum<int>>(
							inner,
							group,
							value,
							decorationLabel(value),
							st::settingsSendType),
						st::settingsSendTypePadding);
				}
				group->setChangedCallback([=](int value) {
					Exteraless::Config::Instance().setDecoration(value);
					decoration->force_assign(decorationLabel(value));
					box->closeBox();
				});
				box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"decoration"_q, u"snow"_q, u"fireworks"_q },
	});

	const auto saveSubfolder = builder.addButton({
		.id = u"exteraless/save-chat-subfolder"_q,
		.title = tr::lng_settings_exteraless_save_chat_subfolder(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().saveToChatSubfolder()),
		.keywords = { u"save"_q, u"folder"_q, u"chat"_q },
	});
	if (saveSubfolder) {
		saveSubfolder->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value
				!= Exteraless::Config::Instance().saveToChatSubfolder());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setSaveToChatSubfolder(value);
		}, saveSubfolder->lifetime());
	}

	const auto hidePremium = builder.addButton({
		.id = u"exteraless/hide-premium"_q,
		.title = tr::lng_settings_exteraless_hide_premium(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().hidePremiumSection()),
		.keywords = { u"premium"_q, u"settings"_q, u"hide"_q },
	});
	if (hidePremium) {
		hidePremium->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value
				!= Exteraless::Config::Instance().hidePremiumSection());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setHidePremiumSection(value);
		}, hidePremium->lifetime());
	}

	const auto hideHelp = builder.addButton({
		.id = u"exteraless/hide-help"_q,
		.title = tr::lng_settings_exteraless_hide_help(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().hideHelpSection()),
		.keywords = { u"help"_q, u"settings"_q, u"hide"_q },
	});
	if (hideHelp) {
		hideHelp->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value
				!= Exteraless::Config::Instance().hideHelpSection());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setHideHelpSection(value);
		}, hideHelp->lifetime());
	}

	const auto hidePhone = builder.addButton({
		.id = u"exteraless/hide-phone"_q,
		.title = tr::lng_settings_exteraless_hide_phone(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().hidePhone()),
		.keywords = { u"phone"_q, u"hide"_q, u"number"_q },
	});
	if (hidePhone) {
		hidePhone->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value != Exteraless::Config::Instance().hidePhone());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setHidePhone(value);
		}, hidePhone->lifetime());
	}

	const auto botPrivacyEnabled = builder.addButton({
		.id = u"exteraless/bot-privacy/enabled"_q,
		.title = tr::lng_settings_exteraless_bot_privacy_enabled(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().botPrivacyEnabled()),
		.keywords = { u"bot"_q, u"privacy"_q, u"account"_q },
	});
	if (botPrivacyEnabled) {
		botPrivacyEnabled->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value
				!= Exteraless::Config::Instance().botPrivacyEnabled());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setBotPrivacyEnabled(value);
		}, botPrivacyEnabled->lifetime());
	}

	const auto botPrivacyAccountLabel = [](int value) {
		const auto accounts = Core::App().domain().orderedAccounts();
		if (value <= 0 || value > int(accounts.size())) {
			return tr::lng_settings_exteraless_bot_privacy_account_off(
				tr::now);
		}
		const auto session = accounts[value - 1]->maybeSession();
		return session
			? session->user()->name()
			: tr::lng_settings_exteraless_bot_privacy_account(
				tr::now);
	};
	const auto botPrivacyAccount = std::make_shared<rpl::variable<QString>>(
		botPrivacyAccountLabel(
			Exteraless::Config::Instance().botPrivacyAccount()));
	builder.addButton({
		.id = u"exteraless/bot-privacy/account"_q,
		.title = tr::lng_settings_exteraless_bot_privacy_account(),
		.st = &st::settingsButtonNoIcon,
		.label = botPrivacyAccount->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			const auto currentAccount
				= Exteraless::Config::Instance().botPrivacyAccount();
			const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
				(currentAccount > 0) ? currentAccount : 0);
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(
					tr::lng_settings_exteraless_bot_privacy_account());
				const auto inner = box->addRow(
					object_ptr<Ui::VerticalLayout>(box));
				inner->add(
					object_ptr<Ui::Radioenum<int>>(
						inner,
						group,
						0,
						tr::lng_settings_exteraless_bot_privacy_account_off(
							tr::now),
						st::settingsSendType),
					st::settingsSendTypePadding);
				const auto accounts
					= Core::App().domain().orderedAccounts();
				for (auto i = 0; i != int(accounts.size()); ++i) {
					inner->add(
						object_ptr<Ui::Radioenum<int>>(
							inner,
							group,
							i + 1,
							botPrivacyAccountLabel(i + 1),
							st::settingsSendType),
						st::settingsSendTypePadding);
				}
				group->setChangedCallback([=](int value) {
					Exteraless::Config::Instance()
						.setBotPrivacyAccount(value);
					botPrivacyAccount->force_assign(
						botPrivacyAccountLabel(value));
					box->closeBox();
				});
				box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"bot"_q, u"privacy"_q, u"account"_q, u"ask"_q },
	});

	const auto botPrivacyAlwaysAsk = builder.addButton({
		.id = u"exteraless/bot-privacy/always-ask"_q,
		.title = tr::lng_settings_exteraless_bot_privacy_always_ask(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().botPrivacyAlwaysAsk()),
		.keywords = { u"bot"_q, u"privacy"_q, u"ask"_q, u"account"_q },
	});
	if (botPrivacyAlwaysAsk) {
		botPrivacyAlwaysAsk->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value
				!= Exteraless::Config::Instance().botPrivacyAlwaysAsk());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setBotPrivacyAlwaysAsk(value);
		}, botPrivacyAlwaysAsk->lifetime());
	}

	builder.addSubsectionTitle({
		.id = u"exteraless/media"_q,
		.title = tr::lng_settings_exteraless_media(),
	});

	const auto qualityLabel = [](int value) {
		switch (value) {
		case 1: return u"720p"_q;
		case 2: return u"1080p"_q;
		}
		return tr::lng_settings_exteraless_media_quality_auto(tr::now);
	};
	const auto mediaQuality = std::make_shared<rpl::variable<QString>>(
		qualityLabel(Exteraless::Config::Instance().defaultVideoQuality()));
	builder.addButton({
		.id = u"exteraless/media/quality"_q,
		.title = tr::lng_settings_exteraless_media_quality(),
		.st = &st::settingsButtonNoIcon,
		.label = mediaQuality->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
				Exteraless::Config::Instance().defaultVideoQuality());
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(tr::lng_settings_exteraless_media_quality());
				const auto inner = box->addRow(
					object_ptr<Ui::VerticalLayout>(box));
				for (auto value = 0; value != 3; ++value) {
					inner->add(
						object_ptr<Ui::Radioenum<int>>(
							inner,
							group,
							value,
							qualityLabel(value),
							st::settingsSendType),
						st::settingsSendTypePadding);
				}
				group->setChangedCallback([=](int value) {
					Exteraless::Config::Instance()
						.setDefaultVideoQuality(value);
					mediaQuality->force_assign(qualityLabel(value));
					box->closeBox();
				});
				box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"video"_q, u"quality"_q, u"hls"_q },
	});

	// WHY: the toggle aliases the upstream core setting that playback reads,
	// while the Exteraless key keeps backups and the CLI in sync with it.
	const auto mediaHwAccel = builder.addButton({
		.id = u"exteraless/media/hw-accel"_q,
		.title = tr::lng_settings_exteraless_media_hw_accel(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Core::App().settings().hardwareAcceleratedVideo()),
		.keywords = { u"hardware"_q, u"acceleration"_q, u"decoder"_q },
	});
	if (mediaHwAccel) {
		mediaHwAccel->toggledChanges(
		) | rpl::filter([](bool value) {
			return (value
				!= Core::App().settings().hardwareAcceleratedVideo());
		}) | rpl::on_next([=](bool value) {
			Core::App().settings().setHardwareAcceleratedVideo(value);
			Core::App().saveSettingsDelayed();
			Exteraless::Config::Instance().setHardwareAcceleration(value);
		}, mediaHwAccel->lifetime());
	}

	const auto mediaGifAsVideo = builder.addButton({
		.id = u"exteraless/media/gif-as-video"_q,
		.title = tr::lng_settings_exteraless_media_gif_as_video(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			Exteraless::Config::Instance().takeGifAsVideo()),
		.keywords = { u"gif"_q, u"video"_q, u"mp4"_q },
	});
	if (mediaGifAsVideo) {
		mediaGifAsVideo->toggledChanges(
		) | rpl::filter([=](bool value) {
			return (value
				!= Exteraless::Config::Instance().takeGifAsVideo());
		}) | rpl::on_next([=](bool value) {
			Exteraless::Config::Instance().setTakeGifAsVideo(value);
		}, mediaGifAsVideo->lifetime());
	}

	const auto bitrateLabel = [](int value) {
		return u"%1 kbps"_q.arg(value);
	};
	const auto mediaAudioBitrate = std::make_shared<rpl::variable<QString>>(
		bitrateLabel(Exteraless::Config::Instance().customAudioBitrate()));
	builder.addButton({
		.id = u"exteraless/media/audio-bitrate"_q,
		.title = tr::lng_settings_exteraless_media_audio_bitrate(),
		.st = &st::settingsButtonNoIcon,
		.label = mediaAudioBitrate->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
				Exteraless::Config::Instance().customAudioBitrate());
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(
					tr::lng_settings_exteraless_media_audio_bitrate());
				const auto inner = box->addRow(
					object_ptr<Ui::VerticalLayout>(box));
				for (const auto value : { 16, 24, 32, 48 }) {
					inner->add(
						object_ptr<Ui::Radioenum<int>>(
							inner,
							group,
							value,
							bitrateLabel(value),
							st::settingsSendType),
						st::settingsSendTypePadding);
				}
				group->setChangedCallback([=](int value) {
					Exteraless::Config::Instance()
						.setCustomAudioBitrate(value);
					mediaAudioBitrate->force_assign(bitrateLabel(value));
					box->closeBox();
				});
				box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"audio"_q, u"bitrate"_q, u"opus"_q },
	});

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
	addGhostToggle(
		u"exteraless/ghost/scheduled-send"_q,
		tr::lng_settings_exteraless_ghost_scheduled_send(),
		{ u"ghost"_q, u"scheduled"_q, u"send"_q },
		Exteraless::Config::Instance().ghostScheduledSend(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostScheduledSend(value);
		});
	addGhostToggle(
		u"exteraless/ghost/status"_q,
		tr::lng_settings_exteraless_ghost_show_status(),
		{ u"ghost"_q, u"status"_q, u"menu"_q },
		Exteraless::Config::Instance().ghostShowStatus(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostShowStatus(value);
		});

	builder.addSubsectionTitle({
		.id = u"exteraless/ghost/locks"_q,
		.title = tr::lng_settings_exteraless_ghost_locks(),
	});
	const auto addGhostLock = [&](
			QString id,
			rpl::producer<QString> title,
			bool current,
			Fn<void(bool)> setter) {
		const auto toggle = builder.addButton({
			.id = std::move(id),
			.title = std::move(title),
			.st = &st::settingsButtonNoIcon,
			.toggled = rpl::single(current),
			.keywords = { u"ghost"_q, u"lock"_q },
		});
		if (toggle) {
			toggle->toggledChanges(
			) | rpl::on_next([=](bool value) {
				setter(value);
			}, toggle->lifetime());
		}
	};
	addGhostLock(
		u"exteraless/ghost/lock/read"_q,
		tr::lng_settings_exteraless_ghost_lock_read(),
		Exteraless::Config::Instance().ghostSendReadLocked(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendReadLocked(value);
		});
	addGhostLock(
		u"exteraless/ghost/lock/stories"_q,
		tr::lng_settings_exteraless_ghost_lock_stories(),
		Exteraless::Config::Instance().ghostSendStoriesLocked(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendStoriesLocked(value);
		});
	addGhostLock(
		u"exteraless/ghost/lock/online"_q,
		tr::lng_settings_exteraless_ghost_lock_online(),
		Exteraless::Config::Instance().ghostSendOnlineLocked(),
		[](bool value) {
			Exteraless::Config::Instance().setGhostSendOnlineLocked(value);
		});
	addGhostLock(
		u"exteraless/ghost/lock/typing"_q,
		tr::lng_settings_exteraless_ghost_lock_typing(),
		Exteraless::Config::Instance().ghostSendUploadProgressLocked(),
		[](bool value) {
			Exteraless::Config::Instance()
				.setGhostSendUploadProgressLocked(value);
		});
	builder.addSubsectionTitle({
		.id = u"exteraless/chat-menu"_q,
		.title = tr::lng_settings_exteraless_chat_menu(),
	});
	const auto addChatMenuToggle = [&](
			QString id,
			rpl::producer<QString> title,
			bool current,
			Fn<void(bool)> setter) {
		const auto toggle = builder.addCheckbox({
			.id = std::move(id),
			.title = std::move(title),
			.checked = current,
			.keywords = { u"chat"_q, u"menu"_q },
		});
		if (toggle) {
			toggle->checkedChanges(
			) | rpl::on_next([=](bool value) {
				setter(value);
			}, toggle->lifetime());
		}
	};
	addChatMenuToggle(
		u"exteraless/chat-menu/boost"_q,
		tr::lng_settings_exteraless_chat_menu_boost(),
		Exteraless::Config::Instance().chatMenuBoost(),
		[](bool value) {
			Exteraless::Config::Instance().setChatMenuBoost(value);
		});
	addChatMenuToggle(
		u"exteraless/chat-menu/linked"_q,
		tr::lng_settings_exteraless_chat_menu_linked(),
		Exteraless::Config::Instance().chatMenuLinked(),
		[](bool value) {
			Exteraless::Config::Instance().setChatMenuLinked(value);
		});
	addChatMenuToggle(
		u"exteraless/chat-menu/view-deleted"_q,
		tr::lng_settings_exteraless_chat_menu_view_deleted(),
		Exteraless::Config::Instance().chatMenuViewDeleted(),
		[](bool value) {
			Exteraless::Config::Instance().setChatMenuViewDeleted(value);
		});
	addChatMenuToggle(
		u"exteraless/chat-menu/clear-deleted"_q,
		tr::lng_settings_exteraless_chat_menu_clear_deleted(),
		Exteraless::Config::Instance().chatMenuClearDeleted(),
		[](bool value) {
			Exteraless::Config::Instance().setChatMenuClearDeleted(value);
		});

	builder.addSubsectionTitle({
		.id = u"exteraless/media-viewer-menu"_q,
		.title = tr::lng_settings_exteraless_media_menu(),
	});
	const auto addMediaToggle = [&](
			QString id,
			rpl::producer<QString> title,
			bool current,
			Fn<void(bool)> setter) {
		const auto toggle = builder.addCheckbox({
			.id = std::move(id),
			.title = std::move(title),
			.checked = current,
			.keywords = { u"media"_q, u"menu"_q },
		});
		if (toggle) {
			toggle->checkedChanges(
			) | rpl::on_next([=](bool value) {
				setter(value);
			}, toggle->lifetime());
		}
	};
	addMediaToggle(
		u"exteraless/media-menu/forward"_q,
		tr::lng_settings_exteraless_media_menu_forward(),
		Exteraless::Config::Instance().mediaViewerMenuForward(),
		[](bool value) {
			Exteraless::Config::Instance().setMediaViewerMenuForward(value);
		});
	addMediaToggle(
		u"exteraless/media-menu/copy-frame"_q,
		tr::lng_settings_exteraless_media_menu_copy_frame(),
		Exteraless::Config::Instance().mediaViewerMenuCopyFrame(),
		[](bool value) {
			Exteraless::Config::Instance().setMediaViewerMenuCopyFrame(value);
		});
	addMediaToggle(
		u"exteraless/media-menu/copy-photo"_q,
		tr::lng_settings_exteraless_media_menu_copy_photo(),
		Exteraless::Config::Instance().mediaViewerMenuCopyPhoto(),
		[](bool value) {
			Exteraless::Config::Instance().setMediaViewerMenuCopyPhoto(value);
		});
	addMediaToggle(
		u"exteraless/media-menu/set-profile"_q,
		tr::lng_settings_exteraless_media_menu_set_profile(),
		Exteraless::Config::Instance().mediaViewerMenuSetProfile(),
		[](bool value) {
			Exteraless::Config::Instance().setMediaViewerMenuSetProfile(value);
		});
	addMediaToggle(
		u"exteraless/media-menu/subtitles"_q,
		tr::lng_settings_exteraless_media_menu_subtitles(),
		Exteraless::Config::Instance().mediaViewerMenuSubtitles(),
		[](bool value) {
			Exteraless::Config::Instance().setMediaViewerMenuSubtitles(value);
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
	const auto headerStyleLabel = [](int value) {
		switch (value) {
		case 1:
			return tr::lng_settings_exteraless_chats_header_style_ios(
				tr::now);
		case 2:
			return tr::lng_settings_exteraless_chats_header_style_m3(
				tr::now);
		}
		return tr::lng_settings_exteraless_chats_header_style_upstream(
			tr::now);
	};
	const auto headerStyle = std::make_shared<rpl::variable<QString>>(
		headerStyleLabel(
			Exteraless::Config::Instance().chatHeaderStyle()));
	builder.addButton({
		.id = u"exteraless/chats/header-style"_q,
		.title = tr::lng_settings_exteraless_chats_header_style(),
		.st = &st::settingsButtonNoIcon,
		.label = headerStyle->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
				Exteraless::Config::Instance().chatHeaderStyle());
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(
					tr::lng_settings_exteraless_chats_header_style());
				const auto inner = box->addRow(
					object_ptr<Ui::VerticalLayout>(box));
				for (auto value = 0; value != 3; ++value) {
					inner->add(
						object_ptr<Ui::Radioenum<int>>(
							inner,
							group,
							value,
							headerStyleLabel(value),
							st::settingsSendType),
						st::settingsSendTypePadding);
				}
				group->setChangedCallback([=](int value) {
					Exteraless::Config::Instance()
						.setChatHeaderStyle(value);
					headerStyle->force_assign(headerStyleLabel(value));
					box->closeBox();
				});
				box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"chat"_q, u"header"_q, u"ios"_q, u"m3"_q },
	});

	addChatsToggle(
		u"exteraless/chats/remove-tail"_q,
		tr::lng_settings_exteraless_chats_remove_tail(),
		{ u"chat"_q, u"tail"_q, u"bubble"_q },
		Exteraless::Config::Instance().chatsSettings().removeMessageTail,
		[](bool value) {
			Exteraless::Config::Instance().setChatsRemoveMessageTail(value);
		});
	addChatsToggle(
		u"exteraless/chats/wide-channel-posts"_q,
		tr::lng_settings_exteraless_chats_wide_channel_posts(),
		{ u"chat"_q, u"channel"_q, u"wide"_q, u"post"_q },
		Exteraless::Config::Instance().chatsSettings().wideChannelPosts,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsWideChannelPosts(value);
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
		u"exteraless/chats/hide-stories-header"_q,
		tr::lng_settings_exteraless_chats_hide_stories_header(),
		{ u"chat"_q, u"stories"_q, u"header"_q },
		Exteraless::Config::Instance().chatsSettings().hideStoriesHeader,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsHideStoriesHeader(value);
		});
	addChatsToggle(
		u"exteraless/chats/save-deleted"_q,
		tr::lng_settings_exteraless_chats_save_deleted(),
		{ u"chat"_q, u"deleted"_q, u"history"_q, u"ayu"_q },
		Exteraless::Config::Instance().saveDeletedMessages(),
		[](bool value) {
			Exteraless::Config::Instance().setSaveDeletedMessages(value);
		});
	addChatsToggle(
		u"exteraless/chats/translucent-deleted"_q,
		tr::lng_settings_exteraless_chats_translucent_deleted(),
		{ u"deleted"_q, u"translucent"_q },
		Exteraless::Config::Instance().translucentDeletedMessages(),
		[](bool value) {
			Exteraless::Config::Instance()
				.setTranslucentDeletedMessages(value);
		});
	addChatsToggle(
		u"exteraless/chats/use-deleted-icon"_q,
		tr::lng_settings_exteraless_chats_use_deleted_icon(),
		{ u"deleted"_q, u"icon"_q },
		Exteraless::Config::Instance().useDeletedIcon(),
		[](bool value) {
			Exteraless::Config::Instance().setUseDeletedIcon(value);
		});
	addChatsToggle(
		u"exteraless/chats/pangu"_q,
		tr::lng_settings_exteraless_chats_pangu(),
		{ u"pangu"_q, u"cjk"_q, u"spaces"_q },
		Exteraless::Config::Instance().panguOnSending(),
		[](bool value) {
			Exteraless::Config::Instance().setPanguOnSending(value);
		});
	addChatsToggle(
		u"exteraless/chats/zalgo-filter"_q,
		tr::lng_settings_exteraless_chats_zalgo_filter(),
		{ u"zalgo"_q, u"filter"_q },
		Exteraless::Config::Instance().zalgoFilter(),
		[](bool value) {
			Exteraless::Config::Instance().setZalgoFilter(value);
		});
	addChatsToggle(
		u"exteraless/chats/show-spoilers-directly"_q,
		tr::lng_settings_exteraless_chats_show_spoilers_directly(),
		{ u"chat"_q, u"spoiler"_q, u"hidden"_q },
		Exteraless::Config::Instance().showSpoilersDirectly(),
		[](bool value) {
			Exteraless::Config::Instance().setShowSpoilersDirectly(value);
		});
	addChatsToggle(
		u"exteraless/chats/silent-by-default"_q,
		tr::lng_settings_exteraless_chats_silent_by_default(),
		{ u"silent"_q, u"sound"_q, u"send"_q },
		Exteraless::Config::Instance().silentByDefault(),
		[](bool value) {
			Exteraless::Config::Instance().setSilentByDefault(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-send-as"_q,
		tr::lng_settings_exteraless_chats_hide_send_as(),
		{ u"send as"_q, u"channel"_q, u"hide"_q },
		Exteraless::Config::Instance().hideSendAsPeer(),
		[](bool value) {
			Exteraless::Config::Instance().setHideSendAsPeer(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-gift"_q,
		tr::lng_settings_exteraless_chats_hide_gift(),
		{ u"gift"_q, u"button"_q, u"profile"_q },
		Exteraless::Config::Instance().hideGiftButton(),
		[](bool value) {
			Exteraless::Config::Instance().setHideGiftButton(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-call"_q,
		tr::lng_settings_exteraless_chats_hide_call(),
		{ u"call"_q, u"button"_q, u"profile"_q },
		Exteraless::Config::Instance().hideCallButton(),
		[](bool value) {
			Exteraless::Config::Instance().setHideCallButton(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-channel-search"_q,
		tr::lng_settings_exteraless_chats_hide_channel_search(),
		{ u"search"_q, u"button"_q, u"channel"_q },
		Exteraless::Config::Instance().hideChannelSearchButton(),
		[](bool value) {
			Exteraless::Config::Instance().setHideChannelSearchButton(value);
		});
	addChatsToggle(
		u"exteraless/chats/forward-date"_q,
		tr::lng_settings_exteraless_chats_forward_date(),
		{ u"forward"_q, u"date"_q },
		Exteraless::Config::Instance().showForwardDate(),
		[](bool value) {
			Exteraless::Config::Instance().setShowForwardDate(value);
		});
	addChatsToggle(
		u"exteraless/chats/sender-mini-avatars"_q,
		tr::lng_settings_exteraless_chats_sender_mini_avatars(),
		{ u"sender"_q, u"avatar"_q },
		Exteraless::Config::Instance().senderMiniAvatars(),
		[](bool value) {
			Exteraless::Config::Instance().setSenderMiniAvatars(value);
		});
	addChatsToggle(
		u"exteraless/chats/disable-link-preview"_q,
		tr::lng_settings_exteraless_chats_disable_link_preview(),
		{ u"link"_q, u"preview"_q },
		Exteraless::Config::Instance().disableLinkPreviewByDefault(),
		[](bool value) {
			Exteraless::Config::Instance()
				.setDisableLinkPreviewByDefault(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-all-chats"_q,
		tr::lng_settings_exteraless_chats_hide_all_chats(),
		{ u"folder"_q, u"chats"_q, u"tab"_q },
		Exteraless::Config::Instance().hideAllChatsFolder(),
		[=](bool value) {
			Exteraless::Config::Instance().setHideAllChatsFolder(value);
			if (const auto controller = builder.controller()) {
				controller->session().data().chatsFilters().notifyListChanged();
			}
		});
	addChatsToggle(
		u"exteraless/chats/hide-archive"_q,
		tr::lng_settings_exteraless_chats_hide_archive(),
		{ u"archive"_q, u"folder"_q, u"hide"_q },
		Exteraless::Config::Instance().chatsSettings().hideArchive,
		[=](bool value) {
			Exteraless::Config::Instance().setChatsHideArchive(value);
			if (const auto controller = builder.controller()) {
				controller->session().data().chatsFilters().notifyListChanged();
			}
		});
	addChatsToggle(
		u"exteraless/chats/hide-greeting-sticker"_q,
		tr::lng_settings_exteraless_chats_hide_greeting_sticker(),
		{ u"greeting"_q, u"sticker"_q, u"hide"_q },
		Exteraless::Config::Instance()
			.chatsSettings().hideGreetingSticker,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsHideGreetingSticker(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-panels-on-scroll"_q,
		tr::lng_settings_exteraless_chats_hide_panels_on_scroll(),
		{ u"panel"_q, u"scroll"_q, u"hide"_q },
		Exteraless::Config::Instance()
			.chatsSettings().hidePanelsOnScroll,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsHidePanelsOnScroll(value);
		});
	addChatsToggle(
		u"exteraless/chats/sort-unread"_q,
		tr::lng_settings_exteraless_chats_sort_by_unread(),
		{ u"sort"_q, u"unread"_q, u"chats"_q },
		Exteraless::Config::Instance().chatsSettings().sortByUnread,
		[](bool value) {
			Exteraless::Config::Instance().setChatsSortByUnread(value);
			base::options::lookup<bool>(
				Dialogs::kOptionDialogsUnreadOnTop).set(value);
		});
	addChatsToggle(
		u"exteraless/chats/disable-archive-autodownload"_q,
		tr::lng_settings_exteraless_chats_disable_archive_autodownload(),
		{ u"archive"_q, u"download"_q, u"auto"_q },
		Exteraless::Config::Instance()
			.chatsSettings().disableAutoDownloadingArchive,
		[](bool value) {
			Exteraless::Config::Instance()
				.setDisableAutoDownloadingArchive(value);
		});
	addChatsToggle(
		u"exteraless/chats/block-win32-executables"_q,
		tr::lng_settings_exteraless_chats_block_win32_executables(),
		{ u"executable"_q, u"exe"_q, u"download"_q },
		Exteraless::Config::Instance()
			.chatsSettings().blockWin32Executables,
		[](bool value) {
			Exteraless::Config::Instance()
				.setBlockWin32Executables(value);
		});
	addChatsToggle(
		u"exteraless/chats/always-show-download"_q,
		tr::lng_settings_exteraless_chats_always_show_download(),
		{ u"download"_q, u"icon"_q, u"media"_q },
		Exteraless::Config::Instance().alwaysShowDownloadIcon(),
		[](bool value) {
			Exteraless::Config::Instance().setAlwaysShowDownloadIcon(value);
		});
	addChatsToggle(
		u"exteraless/chats/use-edited-icon"_q,
		tr::lng_settings_exteraless_chats_use_edited_icon(),
		{ u"edited"_q, u"icon"_q },
		Exteraless::Config::Instance().useEditedIcon(),
		[](bool value) {
			Exteraless::Config::Instance().setUseEditedIcon(value);
		});
	addChatsToggle(
		u"exteraless/chats/show-results-before-voting"_q,
		tr::lng_settings_exteraless_chats_show_results_before_vote(),
		{ u"poll"_q, u"results"_q, u"vote"_q },
		Exteraless::Config::Instance().showResultsBeforeVoting(),
		[](bool value) {
			Exteraless::Config::Instance()
				.setShowResultsBeforeVoting(value);
		});

	builder.addSubsectionTitle({
		.id = u"exteraless/chats/replies"_q,
		.title = tr::lng_settings_exteraless_chats_replies(),
	});
	addChatsToggle(
		u"exteraless/chats/reply-colors"_q,
		tr::lng_settings_exteraless_chats_reply_colors(),
		{ u"reply"_q, u"quote"_q, u"color"_q },
		Exteraless::Config::Instance().replyColors(),
		[](bool value) {
			Exteraless::Config::Instance().setChatsReplyColors(value);
		});
	addChatsToggle(
		u"exteraless/chats/reply-emoji"_q,
		tr::lng_settings_exteraless_chats_reply_emoji(),
		{ u"reply"_q, u"quote"_q, u"emoji"_q },
		Exteraless::Config::Instance().replyEmoji(),
		[](bool value) {
			Exteraless::Config::Instance().setChatsReplyEmoji(value);
		});
	addChatsToggle(
		u"exteraless/chats/reply-background"_q,
		tr::lng_settings_exteraless_chats_reply_background(),
		{ u"reply"_q, u"quote"_q, u"background"_q },
		Exteraless::Config::Instance().replyBackground(),
		[](bool value) {
			Exteraless::Config::Instance().setChatsReplyBackground(value);
		});
	const auto editedMark = std::make_shared<rpl::variable<QString>>(
		Exteraless::Config::Instance().customEditedMark().isEmpty()
		? tr::lng_settings_exteraless_default(tr::now)
		: Exteraless::Config::Instance().customEditedMark());
	builder.addButton({
		.id = u"exteraless/chats/custom-edited-mark"_q,
		.title = tr::lng_settings_exteraless_chats_custom_edited(),
		.st = &st::settingsButtonNoIcon,
		.label = editedMark->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(
					tr::lng_settings_exteraless_chats_custom_edited());
				const auto field = box->addRow(
					object_ptr<Ui::InputField>(
						box,
						st::defaultInputField,
						Ui::InputField::Mode::SingleLine,
						tr::lng_settings_exteraless_chats_custom_edited_hint(),
						TextWithTags{
							Exteraless::Config::Instance()
								.customEditedMark() }),
					st::boxRowPadding);
				field->setMaxLength(16);
				box->addButton(tr::lng_settings_save(), [=] {
					const auto value = field->getTextWithTags()
						.text.trimmed().left(16);
					Exteraless::Config::Instance()
						.setCustomEditedMark(value);
					editedMark->force_assign(value.isEmpty()
						? tr::lng_settings_exteraless_default(tr::now)
						: value);
					box->closeBox();
				});
				box->addButton(tr::lng_cancel(), [=] {
					box->closeBox();
				});
			}));
		},
		.keywords = { u"edited"_q, u"mark"_q, u"label"_q },
	});
	const auto seekDuration = std::make_shared<rpl::variable<QString>>(
		u"%1 s"_q.arg(Exteraless::Config::Instance().seekDuration()));
	builder.addButton({
		.id = u"exteraless/chats/seek-duration"_q,
		.title = tr::lng_settings_exteraless_chats_seek_duration(),
		.st = &st::settingsButtonNoIcon,
		.label = seekDuration->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
				Exteraless::Config::Instance().seekDuration());
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(
					tr::lng_settings_exteraless_chats_seek_duration());
				const auto inner = box->addRow(
					object_ptr<Ui::VerticalLayout>(box));
				for (const auto seconds : { 5, 10, 15, 30 }) {
					inner->add(
						object_ptr<Ui::Radioenum<int>>(
							inner,
							group,
							seconds,
							u"%1 s"_q.arg(seconds),
							st::settingsSendType),
						st::settingsSendTypePadding);
				}
				group->setChangedCallback([=](int seconds) {
					Exteraless::Config::Instance().setSeekDuration(seconds);
					seekDuration->force_assign(u"%1 s"_q.arg(seconds));
					box->closeBox();
				});
				box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"seek"_q, u"duration"_q, u"player"_q },
	});
	addChatsToggle(
		u"exteraless/chats/inline-math"_q,
		tr::lng_settings_exteraless_chats_inline_math(),
		{ u"math"_q, u"inline"_q, u"result"_q },
		Exteraless::Config::Instance().inlineMathResult(),
		[](bool value) {
			Exteraless::Config::Instance().setInlineMathResult(value);
		});

	builder.addSubsectionTitle({
		.id = u"exteraless/chats/message-menu"_q,
		.title = tr::lng_settings_exteraless_message_menu(),
	});
	addChatsToggle(
		u"exteraless/chats/grouped-message-menu"_q,
		tr::lng_settings_exteraless_grouped_menu(),
		{ u"menu"_q, u"grouped"_q, u"delete"_q },
		Exteraless::Config::Instance().groupedMessageMenu(),
		[](bool value) {
			Exteraless::Config::Instance().setGroupedMessageMenu(value);
		});
	addChatsToggle(
		u"exteraless/chats/default-delete-menu"_q,
		tr::lng_settings_exteraless_default_delete(),
		{ u"delete"_q, u"remember"_q, u"menu"_q },
		Exteraless::Config::Instance().defaultDeleteMenu(),
		[](bool value) {
			Exteraless::Config::Instance().setDefaultDeleteMenu(value);
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
		.id = u"exteraless/chats/shortcuts"_q,
		.title = tr::lng_settings_exteraless_shortcuts(),
	});
	addChatsToggle(
		u"exteraless/chats/shortcuts/administrators"_q,
		tr::lng_settings_exteraless_shortcut_administrators(),
		{ u"admin"_q, u"shortcut"_q, u"profile"_q },
		Exteraless::Config::Instance().chatShortcutAdministrators(),
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatShortcutAdministrators(value);
		});
	addChatsToggle(
		u"exteraless/chats/shortcuts/recent-actions"_q,
		tr::lng_settings_exteraless_shortcut_recent_actions(),
		{ u"admin"_q, u"log"_q, u"shortcut"_q },
		Exteraless::Config::Instance().chatShortcutRecentActions(),
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatShortcutRecentActions(value);
		});
	addChatsToggle(
		u"exteraless/chats/shortcuts/statistics"_q,
		tr::lng_settings_exteraless_shortcut_statistics(),
		{ u"admin"_q, u"stats"_q, u"shortcut"_q },
		Exteraless::Config::Instance().chatShortcutStatistics(),
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatShortcutStatistics(value);
		});
	addChatsToggle(
		u"exteraless/chats/shortcuts/permissions"_q,
		tr::lng_settings_exteraless_shortcut_permissions(),
		{ u"admin"_q, u"rights"_q, u"shortcut"_q },
		Exteraless::Config::Instance().chatShortcutPermissions(),
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatShortcutPermissions(value);
		});
	addChatsToggle(
		u"exteraless/chats/shortcuts/members"_q,
		tr::lng_settings_exteraless_shortcut_members(),
		{ u"admin"_q, u"members"_q, u"shortcut"_q },
		Exteraless::Config::Instance().chatShortcutMembers(),
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatShortcutMembers(value);
		});

	builder.addSubsectionTitle({
		.id = u"exteraless/chats/double-tap"_q,
		.title = tr::lng_settings_exteraless_double_tap_title(),
	});

	const auto doubleTapLabel = [](int value) {
		switch (value) {
		case -1:
			return tr::lng_settings_exteraless_double_tap_telegram(tr::now);
		case 1:
		case 2:
			return tr::lng_settings_exteraless_double_tap_reaction(tr::now);
		case 3:
			return tr::lng_settings_exteraless_double_tap_translate(tr::now);
		case 4:
			return tr::lng_settings_exteraless_double_tap_reply(tr::now);
		case 5:
			return tr::lng_settings_exteraless_double_tap_save(tr::now);
		case 8:
			return tr::lng_settings_exteraless_double_tap_edit(tr::now);
		case 10:
			return tr::lng_settings_exteraless_double_tap_delete(tr::now);
		case 11:
			return tr::lng_settings_exteraless_double_tap_forward(tr::now);
		case 12:
			return tr::lng_settings_exteraless_double_tap_download(tr::now);
		}
		return tr::lng_settings_exteraless_double_tap_none(tr::now);
	};
	const auto doubleTapIn = std::make_shared<rpl::variable<QString>>(
		doubleTapLabel(Exteraless::Config::Instance().chatsDoubleTapIn()));
	const auto doubleTapOut = std::make_shared<rpl::variable<QString>>(
		doubleTapLabel(Exteraless::Config::Instance().chatsDoubleTapOut()));

	const auto addDoubleTapButton = [&](
			QString id,
			rpl::producer<QString> title,
			std::shared_ptr<rpl::variable<QString>> label,
			bool outgoing) {
		const auto keywords = QStringList{
			u"double"_q,
			u"tap"_q,
			u"click"_q,
			u"reaction"_q,
			u"reply"_q,
			u"translate"_q,
			u"forward"_q,
			u"save"_q,
			u"delete"_q,
			u"edit"_q,
			u"download"_q,
		};
		builder.addButton({
			.id = std::move(id),
			.title = std::move(title),
			.st = &st::settingsButtonNoIcon,
			.label = label->value(),
			.onClick = [=] {
				const auto controller = builder.controller();
				if (!controller) {
					return;
				}
				const auto &config = Exteraless::Config::Instance();
				const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
					outgoing
						? config.chatsDoubleTapOut()
						: config.chatsDoubleTapIn());
				controller->show(Box([=](not_null<Ui::GenericBox*> box) {
					box->setTitle(outgoing
						? tr::lng_settings_exteraless_double_tap_out()
						: tr::lng_settings_exteraless_double_tap_in());
					const auto options = std::vector<std::pair<int, QString>>{
						{ -1, tr::lng_settings_exteraless_double_tap_telegram(
							tr::now) },
						{ 0, tr::lng_settings_exteraless_double_tap_none(
							tr::now) },
						{ 1, tr::lng_settings_exteraless_double_tap_reaction(
							tr::now) },
						{ 4, tr::lng_settings_exteraless_double_tap_reply(
							tr::now) },
						{ 3, tr::lng_settings_exteraless_double_tap_translate(
							tr::now) },
						{ 5, tr::lng_settings_exteraless_double_tap_save(
							tr::now) },
						{ 8, tr::lng_settings_exteraless_double_tap_edit(
							tr::now) },
						{ 10, tr::lng_settings_exteraless_double_tap_delete(
							tr::now) },
						{ 11, tr::lng_settings_exteraless_double_tap_forward(
							tr::now) },
						{ 12, tr::lng_settings_exteraless_double_tap_download(
							tr::now) },
					};
					const auto inner = box->addRow(
						object_ptr<Ui::VerticalLayout>(box));
					for (const auto &[value, text] : options) {
						inner->add(
							object_ptr<Ui::Radioenum<int>>(
								inner,
								group,
								value,
								text,
								st::settingsSendType),
							st::settingsSendTypePadding);
					}
					group->setChangedCallback([=](int value) {
						auto &config = Exteraless::Config::Instance();
						if (outgoing) {
							config.setChatsDoubleTapOut(value);
						} else {
							config.setChatsDoubleTapIn(value);
						}
						label->force_assign(doubleTapLabel(value));
						box->closeBox();
					});
					box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
				}));
			},
			.keywords = keywords,
		});
	};
	addDoubleTapButton(
		u"exteraless/chats/double-tap/in"_q,
		tr::lng_settings_exteraless_double_tap_in(),
		doubleTapIn,
		false);
	addDoubleTapButton(
		u"exteraless/chats/double-tap/out"_q,
		tr::lng_settings_exteraless_double_tap_out(),
		doubleTapOut,
		true);

	const auto stickerTimeMode = [] {
		const auto chats = Exteraless::Config::Instance().chatsSettings();
		return chats.hideStickerTime ? 2 : (chats.stickerTimeSide ? 1 : 0);
	};
	const auto stickerTimeLabel = [](int mode) {
		switch (mode) {
		case 1:
			return tr::lng_settings_exteraless_sticker_time_side(tr::now);
		case 2:
			return tr::lng_settings_exteraless_sticker_time_hidden(tr::now);
		}
		return tr::lng_settings_exteraless_sticker_time_default(tr::now);
	};
	const auto stickerTime = std::make_shared<rpl::variable<QString>>(
		stickerTimeLabel(stickerTimeMode()));
	builder.addButton({
		.id = u"exteraless/chats/sticker-time"_q,
		.title = tr::lng_settings_exteraless_sticker_time(),
		.st = &st::settingsButtonNoIcon,
		.label = stickerTime->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
				stickerTimeMode());
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(tr::lng_settings_exteraless_sticker_time());
				const auto options = std::vector<std::pair<int, QString>>{
					{ 0, tr::lng_settings_exteraless_sticker_time_default(
						tr::now) },
					{ 1, tr::lng_settings_exteraless_sticker_time_side(
						tr::now) },
					{ 2, tr::lng_settings_exteraless_sticker_time_hidden(
						tr::now) },
				};
				const auto inner = box->addRow(
					object_ptr<Ui::VerticalLayout>(box));
				for (const auto &[value, text] : options) {
					inner->add(
						object_ptr<Ui::Radioenum<int>>(
							inner,
							group,
							value,
							text,
							st::settingsSendType),
						st::settingsSendTypePadding);
				}
				group->setChangedCallback([=](int mode) {
					auto &config = Exteraless::Config::Instance();
					config.setChatsHideStickerTime(mode == 2);
					config.setChatsStickerTimeSide(mode == 1);
					stickerTime->force_assign(stickerTimeLabel(mode));
					box->closeBox();
				});
				box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"sticker"_q, u"time"_q, u"timestamp"_q },
	});

	const auto stickerSize = Exteraless::Config::Instance().stickerSize();
	builder.addSubsectionTitle({
		.id = u"exteraless/chats/sticker-size"_q,
		.title = tr::lng_settings_exteraless_chats_sticker_size(),
		.keywords = { u"sticker"_q, u"size"_q },
	});
	builder.add([=](const Builder::WidgetContext &ctx) {
		const auto slider = ctx.container->add(
			object_ptr<Ui::SettingsSlider>(
				ctx.container,
				st::settingsSlider),
			st::settingsBigScalePadding);
		for (auto size = kStickerSizeMin; size <= kStickerSizeMax; ++size) {
			slider->addSection(QString::number(size));
		}
		slider->setActiveSectionFast(stickerSize - kStickerSizeMin);
		slider->sectionActivated(
		) | rpl::on_next([=](int section) {
			Exteraless::Config::Instance().setStickerSize(
				section + kStickerSizeMin);
		}, slider->lifetime());
		return Builder::SectionBuilder::WidgetToAdd{};
	});

	const auto stickerShapeLabel = [](int shape) {
		switch (shape) {
		case 1:
			return tr::lng_settings_exteraless_chats_sticker_shape_rounded(
				tr::now);
		case 2:
			return tr::lng_settings_exteraless_chats_sticker_shape_bubble(
				tr::now);
		}
		return tr::lng_settings_exteraless_chats_sticker_shape_square(
			tr::now);
	};
	const auto stickerShapeMode = [] {
		return Exteraless::Config::Instance().stickerShape();
	};
	const auto stickerShape = std::make_shared<rpl::variable<QString>>(
		stickerShapeLabel(stickerShapeMode()));
	builder.addButton({
		.id = u"exteraless/chats/sticker-shape"_q,
		.title = tr::lng_settings_exteraless_chats_sticker_shape(),
		.st = &st::settingsButtonNoIcon,
		.label = stickerShape->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
				stickerShapeMode());
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(
					tr::lng_settings_exteraless_chats_sticker_shape());
				const auto inner = box->addRow(
					object_ptr<Ui::VerticalLayout>(box));
				for (auto value = 0; value != 3; ++value) {
					inner->add(
						object_ptr<Ui::Radioenum<int>>(
							inner,
							group,
							value,
							stickerShapeLabel(value),
							st::settingsSendType),
						st::settingsSendTypePadding);
				}
				group->setChangedCallback([=](int value) {
					Exteraless::Config::Instance().setStickerShape(value);
					stickerShape->force_assign(stickerShapeLabel(value));
					box->closeBox();
				});
				box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"sticker"_q, u"shape"_q, u"rounded"_q, u"bubble"_q },
	});

	const auto avatarCornersLabel = [](int radius) {
		if (radius < 0) {
			return tr::lng_settings_exteraless_chats_avatar_corners_circle(
				tr::now);
		} else if (radius == 0) {
			return tr::lng_settings_exteraless_chats_avatar_corners_square(
				tr::now);
		}
		return tr::lng_settings_exteraless_chats_avatar_corners_rounded(
			tr::now,
			lt_radius,
			QString::number(radius));
	};
	const auto avatarCornersPreset = [](int radius) {
		return (radius == -1)
			|| (radius == 0)
			|| (radius == 4)
			|| (radius == 8)
			|| (radius == 12)
			|| (radius == 16)
			|| (radius == 24);
	};
	const auto avatarCorners = std::make_shared<rpl::variable<QString>>(
		avatarCornersLabel(Exteraless::Config::Instance().avatarRadius()));
	builder.addButton({
		.id = u"exteraless/chats/avatar-corners"_q,
		.title = tr::lng_settings_exteraless_chats_avatar_corners(),
		.st = &st::settingsButtonNoIcon,
		.label = avatarCorners->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			const auto current = Exteraless::Config::Instance().avatarRadius();
			auto values = std::vector<int>{ -1, 0, 4, 8, 12, 16, 24 };
			if (!avatarCornersPreset(current)) {
				values.push_back(current);
			}
			const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
				current);
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(
					tr::lng_settings_exteraless_chats_avatar_corners());
				const auto inner = box->addRow(
					object_ptr<Ui::VerticalLayout>(box));
				for (const auto radius : values) {
					inner->add(
						object_ptr<Ui::Radioenum<int>>(
							inner,
							group,
							radius,
							avatarCornersLabel(radius),
							st::settingsSendType),
						st::settingsSendTypePadding);
				}
				group->setChangedCallback([=](int radius) {
					Exteraless::Config::Instance().setAvatarRadius(radius);
					avatarCorners->force_assign(
						avatarCornersLabel(radius));
					box->closeBox();
				});
				box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"avatar"_q, u"corners"_q, u"circle"_q, u"square"_q,
			u"rounded"_q },
	});

	const auto channelBottomButtonLabel = [](int mode) {
		switch (mode) {
		case 0:
			return tr::lng_settings_exteraless_chats_channel_bottom_button_hide(
				tr::now);
		case 2:
			return tr::lng_settings_exteraless_chats_channel_bottom_button_discuss(
				tr::now);
		}
		return tr::lng_settings_exteraless_chats_channel_bottom_button_mute(
			tr::now);
	};
	const auto channelBottomButtonMode = [] {
		return Exteraless::Config::Instance()
			.chatsSettings().channelBottomButton;
	};
	const auto channelBottomButton = std::make_shared<rpl::variable<QString>>(
		channelBottomButtonLabel(channelBottomButtonMode()));
	builder.addButton({
		.id = u"exteraless/chats/channel-bottom-button"_q,
		.title = tr::lng_settings_exteraless_chats_channel_bottom_button(),
		.st = &st::settingsButtonNoIcon,
		.label = channelBottomButton->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
				channelBottomButtonMode());
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(
					tr::lng_settings_exteraless_chats_channel_bottom_button());
				const auto inner = box->addRow(
					object_ptr<Ui::VerticalLayout>(box));
				for (auto value = 0; value != 3; ++value) {
					inner->add(
						object_ptr<Ui::Radioenum<int>>(
							inner,
							group,
							value,
							channelBottomButtonLabel(value),
							st::settingsSendType),
						st::settingsSendTypePadding);
				}
				group->setChangedCallback([=](int value) {
					Exteraless::Config::Instance()
						.setChatsChannelBottomButton(value);
					channelBottomButton->force_assign(
						channelBottomButtonLabel(value));
					box->closeBox();
				});
				box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"channel"_q, u"bottom"_q, u"button"_q, u"mute"_q,
			u"discuss"_q },
	});

	const auto downloadBoostLabel = [](int mode) {
		switch (mode) {
		case 0:
			return tr::lng_settings_exteraless_chats_download_boost_off(
				tr::now);
		case 2:
			return tr::lng_settings_exteraless_chats_download_boost_ultra(
				tr::now);
		}
		return tr::lng_settings_exteraless_chats_download_boost_boost(
			tr::now);
	};
	const auto downloadBoostMode = [] {
		return Exteraless::Config::Instance()
			.chatsSettings().downloadSpeedBoost;
	};
	const auto downloadBoost = std::make_shared<rpl::variable<QString>>(
		downloadBoostLabel(downloadBoostMode()));
	builder.addButton({
		.id = u"exteraless/chats/download-boost"_q,
		.title = tr::lng_settings_exteraless_chats_download_boost(),
		.st = &st::settingsButtonNoIcon,
		.label = downloadBoost->value(),
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			const auto group = std::make_shared<Ui::RadioenumGroup<int>>(
				downloadBoostMode());
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(
					tr::lng_settings_exteraless_chats_download_boost());
				const auto inner = box->addRow(
					object_ptr<Ui::VerticalLayout>(box));
				for (auto value = 0; value != 3; ++value) {
					inner->add(
						object_ptr<Ui::Radioenum<int>>(
							inner,
							group,
							value,
							downloadBoostLabel(value),
							st::settingsSendType),
						st::settingsSendTypePadding);
				}
				group->setChangedCallback([=](int value) {
					Exteraless::Config::Instance()
						.setChatsDownloadSpeedBoost(value);
					downloadBoost->force_assign(
						downloadBoostLabel(value));
					box->closeBox();
				});
				box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"download"_q, u"boost"_q, u"speed"_q },
	});

	addChatsToggle(
		u"exteraless/chats/upload-boost"_q,
		tr::lng_settings_exteraless_chats_upload_boost(),
		{ u"upload"_q, u"boost"_q, u"speed"_q },
		Exteraless::Config::Instance().chatsSettings().uploadBoost,
		[](bool value) {
			Exteraless::Config::Instance().setChatsUploadBoost(value);
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

	builder.addDividerText(tr::lng_settings_exteraless_pill_order_hint());

	const auto resetPillOrder = builder.addButton({
		.id = u"exteraless/pill-stack/reset-order"_q,
		.title = tr::lng_settings_exteraless_pill_order_reset(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			Exteraless::Config::Instance().setPillStackOrder(QString());
			if (const auto controller = builder.controller()) {
				controller->showToast(
					tr::lng_settings_exteraless_pill_order_reset_done(
						tr::now));
			}
		},
		.keywords = { u"pill"_q, u"order"_q, u"reset"_q },
	});

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

	if (Exteraless::Config::Instance().falseBottom()) {
		const auto session = controller
			? &controller->session()
			: nullptr;
		const auto hasPasscode = session
			&& session->domain().local().hasPasscode();
		if (!FalseBottomUnlocked()) {
			builder.addButton({
				.id = u"exteraless/false-bottom/unlock"_q,
				.title = tr::lng_settings_exteraless_false_bottom(),
				.icon = { &st::menuIconLock },
				.onClick = [=] {
					if (!controller) {
						return;
					} else if (!controller->session()
						.domain().local().hasPasscode()) {
						FalseBottomUnlocked() = true;
						controller->showSettings(ExteralessSectionId());
						return;
					}
					controller->show(Box(
						FalseBottomUnlockBox,
						controller,
						[=] {
							FalseBottomUnlocked() = true;
							controller->showSettings(ExteralessSectionId());
						}));
				},
				.keywords = { u"passcode"_q, u"hidden"_q, u"private"_q },
			});
		} else {
			builder.addSubsectionTitle({
				.id = u"exteraless/false-bottom"_q,
				.title = tr::lng_settings_exteraless_false_bottom(),
			});
			const auto showOther = builder.showOther();
			builder.addButton({
				.id = u"exteraless/false-bottom/passcode"_q,
				.title = tr::lng_settings_exteraless_false_bottom_passcode(),
				.st = &st::settingsButtonNoIcon,
				.label = rpl::single(
					hasPasscode
					? tr::lng_settings_cloud_password_on(tr::now)
					: tr::lng_settings_cloud_password_off(tr::now)),
				.onClick = [=] {
					if (!showOther) {
						return;
					}
					showOther(hasPasscode
						? LocalPasscodeCheckId()
						: LocalPasscodeCreateId());
				},
				.keywords = { u"passcode"_q, u"lock"_q, u"pin"_q },
			});
			if (hasPasscode) {
				builder.addButton({
					.id = u"exteraless/false-bottom/lock"_q,
					.title = tr::lng_settings_exteraless_false_bottom_lock(),
					.st = &st::settingsButtonNoIcon,
					.onClick = [=] {
						FalseBottomUnlocked() = false;
						Core::App().lockByPasscode();
					},
					.keywords = { u"lock"_q, u"passcode"_q },
				});
			}
			builder.addDividerText(rpl::single(hasPasscode
				? tr::lng_settings_exteraless_false_bottom_about(tr::now)
				: tr::lng_settings_exteraless_false_bottom_no_passcode(
					tr::now)));
		}
	}
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
