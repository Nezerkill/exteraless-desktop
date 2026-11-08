/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "media/player/player_sheet.h"

#include "base/algorithm.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_click_handler.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "data/data_types.h"
#include "history/history_item.h"
#include "history/view/history_view_context_menu.h"
#include "history/view/history_view_element.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "media/audio/media_audio.h"
#include "media/media_common.h"
#include "media/player/media_player_instance.h"
#include "ui/image/image.h"
#include "ui/painter.h"
#include "ui/style/style_core.h"
#include "ui/text/format_song_document_name.h"
#include "ui/text/format_values.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/labels.h"
#include "window/window_peer_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_media_player.h"

#include <QtGui/QPainterPath>
#include <QtGui/QPen>

#include <algorithm>
#include <array>

namespace Exteraless {
namespace {

[[nodiscard]] Image *CoverImage(
		const std::shared_ptr<Data::DocumentMedia> &media) {
	if (!media) {
		return nullptr;
	}
	return media->thumbnail() ? media->thumbnail() : media->thumbnailInline();
}

[[nodiscard]] std::array<float64, 4> PlaybackRates() {
	return { 0.5, 1., 1.5, 2. };
}

} // namespace

PlayerSheet::PlayerSheet(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: LayerWidget(parent)
, _controller(controller)
, _title(this, st::playerSheetTitle)
, _performer(this, st::playerSheetPerformer)
, _elapsed(this, st::playerSheetTime)
, _total(this, st::playerSheetTime)
, _previous(this, st::playerSheetPrevious)
, _playPause(this, st::playerSheetPlay)
, _next(this, st::playerSheetNext)
, _seek(this, st::playerSheetSeek)
, _volume(this, st::playerSheetVolume)
, _show(this, st::playerSheetAction)
, _forward(this, st::playerSheetAction)
, _link(this, st::playerSheetAction)
, _download(this, st::playerSheetAction)
, _corners(
	Ui::PrepareCornerPixmaps(
		st::playerSheetRadius,
		st::playerSheetBg)) {
	_rate.create(this, rpl::single(QString()), st::playerSheetSpeed);

	style::PaletteChanged(
	) | rpl::on_next([=] {
		_corners = Ui::PrepareCornerPixmaps(
			st::playerSheetRadius,
			st::playerSheetBg);
		update();
	}, lifetime());

	setupControls();
	setupSubscriptions();

	refreshTrack();
	refreshRate();
	refreshVolume();

	const auto state = Media::Player::instance()->getState(
		AudioMsgId::Type::Song);
	updateState(state);
	layoutChildren();
}

PlayerSheet::~PlayerSheet() = default;

void PlayerSheet::Show(not_null<Window::SessionController*> controller) {
	controller->showLayer(
		std::make_unique<PlayerSheet>(nullptr, controller),
		Ui::LayerOption::CloseOther);
}

void PlayerSheet::setupControls() {
	_title->setAttribute(Qt::WA_TransparentForMouseEvents);
	_performer->setAttribute(Qt::WA_TransparentForMouseEvents);
	_elapsed->setAttribute(Qt::WA_TransparentForMouseEvents);
	_total->setAttribute(Qt::WA_TransparentForMouseEvents);

	_show->setIconOverride(&st::playerSheetShowIcon);
	_forward->setIconOverride(&st::playerSheetForwardIcon);
	_link->setIconOverride(&st::playerSheetLinkIcon);
	_download->setIconOverride(&st::playerSheetDownloadIcon);

	_playPause->setClickedCallback([=] {
		Media::Player::instance()->playPauseCancelClicked(AudioMsgId::Type::Song);
	});
	_previous->setClickedCallback([=] {
		Media::Player::instance()->previous(AudioMsgId::Type::Song);
	});
	_next->setClickedCallback([=] {
		Media::Player::instance()->next(AudioMsgId::Type::Song);
	});

	_playPause->setAccessibleName(tr::lng_shortcuts_media_play(tr::now));
	_previous->setAccessibleName(tr::lng_shortcuts_media_previous(tr::now));
	_next->setAccessibleName(tr::lng_shortcuts_media_next(tr::now));
	_show->setAccessibleName(tr::lng_reply_show_in_chat(tr::now));
	_forward->setAccessibleName(tr::lng_context_forward_msg(tr::now));
	_link->setAccessibleName(tr::lng_context_copy_message_link(tr::now));
	_download->setAccessibleName(tr::lng_mediaview_save_as(tr::now));

	_show->setClickedCallback([=] { showInChat(); });
	_forward->setClickedCallback([=] { forwardTrack(); });
	_link->setClickedCallback([=] { copyLink(); });
	_download->setClickedCallback([=] { downloadTrack(); });

	_seek->setChangeProgressCallback([=](float64 progress) {
		if (!Media::Player::instance()->isSeeking(AudioMsgId::Type::Song)) {
			Media::Player::instance()->startSeeking(AudioMsgId::Type::Song);
		}
		updateElapsed(crl::time(_durationMs * progress));
	});
	_seek->setChangeFinishedCallback([=](float64 progress) {
		Media::Player::instance()->finishSeeking(AudioMsgId::Type::Song, progress);
	});

	const auto applyVolume = [=](float64 volume) {
		auto &settings = Core::App().settings();
		if (volume != settings.songVolume()) {
			Media::Player::mixer()->setSongVolume(volume);
			settings.setSongVolume(volume);
		}
	};
	_volume->setChangeProgressCallback([=](float64 volume) {
		applyVolume(volume);
	});
	_volume->setChangeFinishedCallback([=](float64 volume) {
		if (volume > 0) {
			Core::App().settings().setRememberedSongVolume(volume);
		}
		applyVolume(volume);
		Core::App().saveSettingsDelayed();
	});

	_rate->setClickedCallback([=] {
		const auto rates = PlaybackRates();
		const auto current = Core::App().settings().audioPlaybackSpeed();
		auto index = 0;
		for (auto i = 0; i != int(rates.size()); ++i) {
			if (Media::EqualSpeeds(rates[i], current)) {
				index = i;
				break;
			}
		}
		const auto speed = rates[(index + 1) % rates.size()];
		Core::App().settings().setAudioPlaybackSpeed(speed);
		Core::App().saveSettingsDelayed();
		Media::Player::instance()->updatePlaybackSpeed();
		refreshRate();
	});
}

void PlayerSheet::setupSubscriptions() {
	Media::Player::instance()->updatedNotifier(
	) | rpl::on_next([=](const Media::Player::TrackState &state) {
		updateState(state);
	}, lifetime());

	Media::Player::instance()->trackChanged(
	) | rpl::filter([=](AudioMsgId::Type type) {
		return (type == AudioMsgId::Type::Song);
	}) | rpl::on_next([=] {
		refreshTrack();
	}, lifetime());

	Media::Player::instance()->playlistChanges(
		AudioMsgId::Type::Song
	) | rpl::on_next([=] {
		refreshControls();
	}, lifetime());

	Media::Player::instance()->stops(
		AudioMsgId::Type::Song
	) | rpl::on_next([=] {
		closeLayer();
	}, lifetime());

	Core::App().settings().songVolumeChanges(
	) | rpl::on_next([=] {
		refreshVolume();
	}, lifetime());

	Core::App().settings().audioPlaybackSpeedChanges(
	) | rpl::on_next([=] {
		refreshRate();
	}, lifetime());

	_controller->session().downloaderTaskFinished(
	) | rpl::on_next([=] {
		if (_coverPending && CoverImage(_coverMedia)) {
			_coverPending = false;
			update();
		}
	}, lifetime());
}

void PlayerSheet::parentResized() {
	layoutChildren();
}

void PlayerSheet::layoutChildren() {
	const auto width = st::playerSheetWidth;
	const auto padding = st::playerSheetPadding;
	const auto gap = st::playerSheetGap;
	const auto inner = width - 2 * padding;
	const auto noteHeight = st::playerSheetSeekHeight;
	const auto timeWidth = st::playerSheetTimeWidth;
	const auto timeSkip = st::playerSheetTimeSkip;
	auto y = padding + st::playerSheetCover + gap;

	_title->resizeToNaturalWidth(inner);
	_title->moveToLeft(padding + (inner - _title->width()) / 2, y);
	y += _title->height() + st::playerSheetTitleSkip;

	_performer->resizeToNaturalWidth(inner);
	_performer->moveToLeft(padding + (inner - _performer->width()) / 2, y);
	y += _performer->height() + gap;

	_elapsed->moveToLeft(
		padding,
		y + (noteHeight - _elapsed->height()) / 2);
	_total->moveToLeft(
		padding + inner - timeWidth + (timeWidth - _total->width()) / 2,
		y + (noteHeight - _total->height()) / 2);
	_seek->setGeometry(
		padding + timeWidth + timeSkip,
		y,
		inner - 2 * (timeWidth + timeSkip),
		noteHeight);
	y += noteHeight + gap;

	const auto transportSkip = st::playerSheetTransportSkip;
	const auto transportWidth = _previous->width()
		+ _playPause->width()
		+ _next->width()
		+ 2 * transportSkip;
	const auto transportHeight = st::playerSheetTransportHeight;
	auto left = (width - transportWidth) / 2;
	_previous->moveToLeft(
		left,
		y + (transportHeight - _previous->height()) / 2);
	left += _previous->width() + transportSkip;
	_playPause->moveToLeft(
		left,
		y + (transportHeight - _playPause->height()) / 2);
	left += _playPause->width() + transportSkip;
	_next->moveToLeft(
		left,
		y + (transportHeight - _next->height()) / 2);
	y += transportHeight + gap;

	_rate->moveToLeft(
		padding + inner - _rate->width(),
		y + (noteHeight - _rate->height()) / 2);
	_volume->setGeometry(
		padding,
		y,
		inner - _rate->width() - gap,
		noteHeight);
	y += noteHeight + gap;

	const auto actions = {
		_show.data(),
		_forward.data(),
		_link.data(),
		_download.data()
	};
	const auto slot = inner / int(actions.size());
	auto index = 0;
	for (auto *button : actions) {
		button->moveToLeft(
			padding + slot * index + (slot - button->width()) / 2,
			y + (st::playerSheetActionsHeight - button->height()) / 2);
		++index;
	}
	y += st::playerSheetActionsHeight + padding;

	if (size() != QSize(width, y)) {
		resize(width, y);
	}
	if (const auto parent = parentWidget()) {
		moveToLeft(
			(parent->width() - width) / 2,
			(parent->height() - y) / 2);
	}
}

void PlayerSheet::refreshTrack() {
	const auto song = AudioMsgId::Type::Song;
	const auto current = Media::Player::instance()->current(song);
	_current = current;
	const auto document = current.audio();
	if (!document) {
		_title->setText(QString());
		_performer->setText(QString());
		_coverMedia = nullptr;
		_coverPending = false;
		_elapsed->setText(Ui::FormatDurationText(0));
		_total->setText(Ui::FormatDurationText(0));
		_seek->setValue(0.);
		refreshControls();
		layoutChildren();
		update();
		return;
	}

	auto title = QString();
	auto performer = QString();
	if (const auto data = document->song()) {
		title = data->title;
		performer = data->performer;
	} else {
		const auto composed = Ui::Text::FormatSongNameFor(
			document).composedName();
		title = composed.title;
		performer = composed.performer;
	}
	_title->setText(title);
	_performer->setText(performer);

	_coverMedia = document->createMediaView();
	_coverMedia->thumbnailWanted(Data::FileOrigin(current.contextId()));
	_coverPending = !CoverImage(_coverMedia);

	refreshControls();
	layoutChildren();
	update();
}

void PlayerSheet::updateState(const Media::Player::TrackState &state) {
	const auto document = state.id.audio();
	if (state.id.type() != AudioMsgId::Type::Song || !document) {
		return;
	}
	if (_current.audio() != document
		|| _current.contextId() != state.id.contextId()) {
		refreshTrack();
	}
	const auto frequency = std::max(state.frequency, 1);
	_durationMs = (state.length * 1000LL) / frequency;
	_total->setText(Ui::FormatDurationText(_durationMs / 1000));
	_seek->setDisabled(document->loading());
	if (!_seek->isChanging()) {
		const auto progress = state.length
			? std::clamp(float64(state.position) / state.length, 0., 1.)
			: 0.;
		_seek->setValue(progress);
		updateElapsed((state.position * 1000LL) / frequency);
	}
	const auto pause = Media::Player::ShowPauseIcon(state.state)
		|| Media::Player::instance()->isSeeking(AudioMsgId::Type::Song);
	_playPause->setIconOverride(pause ? &st::playerSheetPauseIcon : nullptr);
}

void PlayerSheet::updateElapsed(crl::time position) {
	_elapsed->setText(Ui::FormatDurationText(position / 1000));
}

void PlayerSheet::refreshControls() {
	const auto song = AudioMsgId::Type::Song;
	_previous->setIconOverride(Media::Player::instance()->previousAvailable(song)
		? nullptr
		: &st::mediaPlayerPreviousDisabledIcon);
	_next->setIconOverride(Media::Player::instance()->nextAvailable(song)
		? nullptr
		: &st::mediaPlayerNextDisabledIcon);

	const auto item = currentItem();
	for (auto *button : {
			_show.data(),
			_forward.data(),
			_link.data(),
			_download.data() }) {
		button->setEnabled(item != nullptr);
	}
	_link->setEnabled(item && item->hasDirectLink());
}

void PlayerSheet::refreshRate() {
	const auto speed = Core::App().settings().audioPlaybackSpeed();
	const auto text = QString::number(base::SafeRound(speed * 10) / 10.)
		+ u"x"_q;
	_rate->setText(rpl::single(text));
	_rate->setAccessibleName(tr::lng_mediaview_playback_speed(
		tr::now,
		lt_speed,
		text));
}

void PlayerSheet::refreshVolume() {
	if (!_volume->isChanging()) {
		_volume->setValue(Core::App().settings().songVolume());
	}
}

HistoryItem *PlayerSheet::currentItem() const {
	const auto id = currentId();
	return id ? _controller->session().data().message(id) : nullptr;
}

FullMsgId PlayerSheet::currentId() const {
	return Media::Player::instance()->current(AudioMsgId::Type::Song).contextId();
}

void PlayerSheet::showInChat() {
	if (const auto item = currentItem()) {
		closeLayer();
		_controller->showMessage(item);
	}
}

void PlayerSheet::forwardTrack() {
	if (const auto id = currentId(); id) {
		Window::ShowForwardMessagesBox(
			_controller,
			MessageIdsList{ id },
			nullptr);
	}
}

void PlayerSheet::copyLink() {
	if (const auto id = currentId(); id) {
		HistoryView::CopyPostLink(
			_controller,
			id,
			HistoryView::Context::History);
	}
}

void PlayerSheet::downloadTrack() {
	const auto current = Media::Player::instance()->current(
		AudioMsgId::Type::Song);
	const auto document = current.audio();
	if (document && current.contextId()) {
		DocumentSaveClickHandler::SaveAndTrack(
			current.contextId(),
			document);
	}
}

void PlayerSheet::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);
	Ui::FillRoundRect(p, rect(), st::playerSheetBg, _corners);

	const auto border = st::playerSheetBorderWidth;
	if (border > 0) {
		auto pen = QPen(st::playerSheetBorderFg->c);
		pen.setWidthF(border);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		const auto half = border / 2.;
		p.drawRoundedRect(
			QRectF(rect()).adjusted(half, half, -half, -half),
			st::playerSheetRadius * 1.,
			st::playerSheetRadius * 1.);
	}

	const auto cover = QRect(
		(width() - st::playerSheetCover) / 2,
		st::playerSheetPadding,
		st::playerSheetCover,
		st::playerSheetCover);
	paintCover(p, cover);
}

void PlayerSheet::paintCover(QPainter &p, QRect rect) {
	auto path = QPainterPath();
	const auto radius = st::playerSheetCoverRadius * 1.;
	path.addRoundedRect(rect, radius, radius);
	p.save();
	p.setClipPath(path);
	if (const auto image = CoverImage(_coverMedia)) {
		const auto scaled = image->size().scaled(
			rect.size(),
			Qt::KeepAspectRatioByExpanding);
		p.drawPixmap(
			rect.x() + (rect.width() - scaled.width()) / 2,
			rect.y() + (rect.height() - scaled.height()) / 2,
			image->pixSingle(scaled));
	} else {
		p.fillRect(rect, st::playerSheetCoverBg);
		st::playerSheetCoverIcon.paintInCenter(p, rect);
	}
	p.restore();
}

} // namespace Exteraless
