/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_audio_msg_id.h"
#include "ui/cached_round_corners.h"
#include "ui/layers/layer_widget.h"
#include "base/object_ptr.h"

#include <memory>

class HistoryItem;
class QPainter;

namespace Data {
class DocumentMedia;
} // namespace Data

namespace Ui {
class FlatLabel;
class LabelSimple;
class IconButton;
class RoundButton;
class MediaSlider;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Media::Player {
struct TrackState;
} // namespace Media::Player

namespace Exteraless {

class PlayerSheet final : public Ui::LayerWidget {
public:
	PlayerSheet(
		QWidget *parent,
		not_null<Window::SessionController*> controller);
	~PlayerSheet();

	static void Show(not_null<Window::SessionController*> controller);

	void parentResized() override;

private:
	void setupControls();
	void setupSubscriptions();
	void layoutChildren();

	void refreshTrack();
	void updateState(const Media::Player::TrackState &state);
	void updateElapsed(crl::time position);
	void refreshControls();
	void refreshRate();
	void refreshVolume();

	[[nodiscard]] HistoryItem *currentItem() const;
	[[nodiscard]] FullMsgId currentId() const;

	void showInChat();
	void forwardTrack();
	void copyLink();
	void downloadTrack();

	void paintEvent(QPaintEvent *e) override;
	void paintCover(QPainter &p, QRect rect);

	const not_null<Window::SessionController*> _controller;

	object_ptr<Ui::FlatLabel> _title = { nullptr };
	object_ptr<Ui::FlatLabel> _performer = { nullptr };
	object_ptr<Ui::LabelSimple> _elapsed = { nullptr };
	object_ptr<Ui::LabelSimple> _total = { nullptr };
	object_ptr<Ui::IconButton> _previous = { nullptr };
	object_ptr<Ui::IconButton> _playPause = { nullptr };
	object_ptr<Ui::IconButton> _next = { nullptr };
	object_ptr<Ui::MediaSlider> _seek = { nullptr };
	object_ptr<Ui::MediaSlider> _volume = { nullptr };
	object_ptr<Ui::RoundButton> _rate = { nullptr };
	object_ptr<Ui::IconButton> _show = { nullptr };
	object_ptr<Ui::IconButton> _forward = { nullptr };
	object_ptr<Ui::IconButton> _link = { nullptr };
	object_ptr<Ui::IconButton> _download = { nullptr };

	AudioMsgId _current;
	std::shared_ptr<Data::DocumentMedia> _coverMedia;
	Ui::CornersPixmaps _corners;

	crl::time _durationMs = 0;
	bool _coverPending = false;

};

} // namespace Exteraless
