/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "media/player/player_queue_sheet.h"

#include "base/algorithm.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "media/media_video_canvas.h"
#include "media/player/media_player_instance.h"
#include "ui/image/image.h"
#include "ui/painter.h"
#include "ui/style/style_core.h"
#include "ui/text/format_song_document_name.h"
#include "ui/text/format_values.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"
#include "styles/style_media_player.h"

#include <QtGui/QLinearGradient>
#include <QtGui/QPen>

#include <algorithm>

namespace Exteraless {
namespace {

constexpr auto kActiveFillAlpha = 64;
constexpr auto kAccentBarAlpha = 255;

[[nodiscard]] QColor WithAlpha(QColor color, int alpha) {
	color.setAlpha(alpha);
	return color;
}

[[nodiscard]] DocumentData *RowDocument(HistoryItem *item) {
	return (item && item->media()) ? item->media()->document() : nullptr;
}

} // namespace

QueueSheet::QueueSheet(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: LayerWidget(parent)
, _controller(controller)
, _title(this, st::queueSheetTitle)
, _corners(
	Ui::PrepareCornerPixmaps(
		st::queueSheetRadius,
		st::queueSheetBg)) {
	_title->setText(tr::lng_sr_player_queue(tr::now));
	_title->setAttribute(Qt::WA_TransparentForMouseEvents);

	style::PaletteChanged(
	) | rpl::on_next([=] {
		_corners = Ui::PrepareCornerPixmaps(
			st::queueSheetRadius,
			st::queueSheetBg);
		update();
	}, lifetime());

	setupSubscriptions();
	setupQueue();
	refreshRows();
	layoutChildren();
}

QueueSheet::~QueueSheet() = default;

void QueueSheet::Show(not_null<Window::SessionController*> controller) {
	controller->showLayer(
		std::make_unique<QueueSheet>(nullptr, controller),
		Ui::LayerOption::CloseOther);
}

void QueueSheet::setupSubscriptions() {
	_queue.changed(
	) | rpl::on_next([=] {
		refreshRows();
	}, lifetime());

	Media::Player::instance()->trackChanged(
	) | rpl::filter([=](AudioMsgId::Type type) {
		return (type == AudioMsgId::Type::Song);
	}) | rpl::on_next([=] {
		setupQueue();
		refreshRows();
	}, lifetime());

	_controller->session().downloaderTaskFinished(
	) | rpl::on_next([=] {
		refreshAccents();
	}, lifetime());
}

void QueueSheet::setupQueue() {
	const auto type = AudioMsgId::Type::Song;
	_queue.attach(
		Media::Player::instance(),
		type,
		[=] {
			return Media::Player::instance()->playlistTrackIds(type);
		});
}

void QueueSheet::refreshRows() {
	const auto &order = _queue.order();
	const auto &data = _controller->session().data();

	auto rows = std::vector<Row>();
	rows.reserve(order.size());
	for (const auto &id : order) {
		const auto item = data.message(id);
		const auto document = RowDocument(item);
		if (!document) {
			rows.push_back({ id });
			continue;
		}
		auto row = Row();
		row.id = id;
		row.documentId = document->id;
		if (const auto song = document->song()) {
			row.title = song->title;
			row.performer = song->performer;
		} else {
			const auto composed = Ui::Text::FormatSongNameFor(
				document).composedName();
			row.title = composed.title;
			row.performer = composed.performer;
		}
		row.time = Ui::FormatDurationText(document->duration());
		rows.push_back(std::move(row));
		loadAccent(document, id);
	}
	_rows = std::move(rows);

	layoutChildren();
	_scroll = std::clamp(_scroll, 0, maxScroll());
	update();
}

void QueueSheet::refreshAccents() {
	const auto &data = _controller->session().data();
	const auto &order = _queue.order();
	for (const auto &id : order) {
		if (const auto document = RowDocument(data.message(id))) {
			loadAccent(document, id);
		}
	}
	update();
}

void QueueSheet::loadAccent(
		not_null<DocumentData*> document,
		FullMsgId id) {
	const auto documentId = document->id;
	if (_accents.contains(documentId)) {
		return;
	}
	const auto media = document->createMediaView();
	const auto image = media->thumbnail()
		? media->thumbnail()
		: media->thumbnailInline();
	if (!image) {
		media->thumbnailWanted(Data::FileOrigin(id));
		return;
	}
	_accents.insert(
		documentId,
		Media::Encode::DominantCanvasBackground(image->original()).top);
}

void QueueSheet::layoutChildren() {
	const auto width = st::queueSheetWidth;
	const auto count = int(_rows.size());
	const auto listMax = st::queueSheetMaxHeight
		- st::queueSheetHeaderHeight
		- st::queueSheetPadding;
	const auto list = (count > 0)
		? std::min(count * st::queueSheetRowHeight, listMax)
		: st::queueSheetEmptyHeight;
	const auto height = st::queueSheetHeaderHeight + list + st::queueSheetPadding;

	_title->resizeToNaturalWidth(width - 2 * st::queueSheetPadding);
	_title->moveToLeft(
		st::queueSheetPadding,
		(st::queueSheetHeaderHeight - _title->height()) / 2);

	if (size() != QSize(width, height)) {
		resize(width, height);
	}
	if (const auto parent = parentWidget()) {
		moveToLeft(
			(parent->width() - width) / 2,
			(parent->height() - height) / 2);
	}
}

void QueueSheet::parentResized() {
	layoutChildren();
}

int QueueSheet::listTop() const {
	return st::queueSheetHeaderHeight;
}

int QueueSheet::listHeight() const {
	return std::max(height() - listTop() - st::queueSheetPadding, 0);
}

int QueueSheet::maxScroll() const {
	return std::max(
		int(_rows.size()) * st::queueSheetRowHeight - listHeight(),
		0);
}

int QueueSheet::rowAt(QPoint position) const {
	if (_rows.empty()) {
		return -1;
	}
	const auto relative = position.y() - (listTop() - _scroll);
	if (relative < 0) {
		return -1;
	}
	const auto index = relative / st::queueSheetRowHeight;
	if (index < 0 || index >= int(_rows.size())) {
		return -1;
	}
	return index;
}

void QueueSheet::paintEvent(QPaintEvent *) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);

	Ui::FillRoundRect(p, rect(), st::queueSheetBg, _corners);

	const auto border = st::queueSheetBorderWidth;
	if (border > 0) {
		auto pen = QPen(st::queueSheetBorderFg->c);
		pen.setWidthF(border);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		const auto half = border / 2.;
		p.drawRoundedRect(
			QRectF(rect()).adjusted(half, half, -half, -half),
			st::queueSheetRadius * 1.,
			st::queueSheetRadius * 1.);
	}

	paintHeader(p);

	const auto top = listTop();
	const auto clip = QRect(0, top, width(), listHeight());
	p.save();
	p.setClipRect(clip);
	if (_rows.empty()) {
		paintEmpty(p, clip);
	} else {
		for (auto i = 0; i != int(_rows.size()); ++i) {
			const auto rowTop = top + i * st::queueSheetRowHeight - _scroll;
			const auto row = QRect(
				st::queueSheetPadding,
				rowTop,
				width() - 2 * st::queueSheetPadding,
				st::queueSheetRowHeight);
			if (row.bottom() < clip.top() || row.top() > clip.bottom()) {
				continue;
			}
			paintRow(p, i, row);
		}
	}
	p.restore();
}

void QueueSheet::paintHeader(QPainter &p) {
	const auto inset = st::queueSheetPadding;
	p.fillRect(
		QRect(
			inset,
			st::queueSheetHeaderHeight - st::lineWidth,
			width() - 2 * inset,
			st::lineWidth),
		st::queueSheetDivider);
}

void QueueSheet::paintRow(QPainter &p, int index, QRect rect) {
	const auto &row = _rows[index];
	const auto current = Media::Player::instance()->current(
		AudioMsgId::Type::Song);
	const auto active = (row.id == current.contextId());
	const auto dragged = _dragging && (index == _pressedRow);
	const auto target = _dragging && (index == _targetRow) && (index != _pressedRow);

	const auto accent = _accents.value(row.documentId);
	const auto color = accent.isValid()
		? accent
		: st::queueSheetActiveBg->c;
	if (active || dragged || target) {
		auto gradient = QLinearGradient(rect.topLeft(), rect.bottomLeft());
		gradient.setColorAt(0., WithAlpha(color, kActiveFillAlpha));
		gradient.setColorAt(1., WithAlpha(color, kActiveFillAlpha / 4));
		p.setPen(Qt::NoPen);
		p.setBrush(gradient);
		p.drawRoundedRect(rect, st::queueSheetRowRadius, st::queueSheetRowRadius);
		if (active || dragged) {
			p.fillRect(
				QRect(
					rect.x(),
					rect.y() + st::queueSheetRowBarSkip,
					st::queueSheetAccentWidth,
					rect.height() - 2 * st::queueSheetRowBarSkip),
				WithAlpha(color, kAccentBarAlpha));
		}
	}

	const auto padding = st::queueSheetRowPadding;
	const auto left = rect.x() + padding.left();
	const auto right = rect.x() + rect.width() - padding.right();
	const auto timeWidth = st::queueSheetTimeWidth;
	const auto textWidth = std::max(
		right - left - timeWidth - st::queueSheetTimeSkip,
		0);

	const auto &titleFont = st::queueSheetTitle.style.font;
	const auto titleTop = rect.y() + padding.top();
	p.setFont(titleFont);
	p.setPen(st::queueSheetTitle.textFg->c);
	p.drawText(
		left,
		titleTop + titleFont->ascent,
		titleFont->elided(row.title, textWidth));

	const auto &performerFont = st::queueSheetPerformer.style.font;
	p.setFont(performerFont);
	p.setPen(st::queueSheetPerformer.textFg->c);
	p.drawText(
		left,
		titleTop + titleFont->height + performerFont->ascent,
		performerFont->elided(row.performer, textWidth));

	p.setFont(st::queueSheetTime.font);
	p.setPen(st::queueSheetTime.textFg->c);
	p.drawText(
		QRect(
			right - timeWidth,
			rect.y(),
			timeWidth,
			rect.height()),
		Qt::AlignRight | Qt::AlignVCenter,
		row.time);
}

void QueueSheet::paintEmpty(QPainter &p, QRect rect) {
	p.setFont(st::queueSheetEmpty.font);
	p.setPen(st::queueSheetEmpty.textFg->c);
	p.drawText(
		rect,
		Qt::AlignCenter,
		tr::lng_sr_player_queue_empty(tr::now));
}

void QueueSheet::mousePressEvent(QMouseEvent *e) {
	LayerWidget::mousePressEvent(e);
	if (e->button() != Qt::LeftButton) {
		return;
	}
	const auto index = rowAt(e->pos());
	if (index < 0) {
		return;
	}
	_pressedRow = index;
	_targetRow = index;
	_pressPosition = e->pos();
	_dragging = false;
}

void QueueSheet::mouseMoveEvent(QMouseEvent *e) {
	if (_pressedRow < 0) {
		return;
	}
	if (!_dragging
		&& (e->pos() - _pressPosition).manhattanLength()
			>= st::queueSheetDragThreshold) {
		_dragging = true;
		setCursor(style::cur_sizever);
	}
	if (!_dragging) {
		return;
	}
	const auto index = rowAt(e->pos());
	if (index >= 0 && index != _targetRow) {
		_targetRow = index;
		update();
	}
}

void QueueSheet::mouseReleaseEvent(QMouseEvent *) {
	const auto pressed = base::take(_pressedRow);
	const auto target = base::take(_targetRow);
	const auto dragging = base::take(_dragging);
	setCursor(style::cur_default);

	if (pressed < 0) {
		return;
	}
	if (dragging) {
		if (target >= 0 && target != pressed) {
			_queue.reorder(pressed, target);
		}
	} else {
		activate(pressed);
	}
	update();
}

void QueueSheet::wheelEvent(QWheelEvent *e) {
	const auto delta = e->angleDelta().y();
	if (!delta) {
		return;
	}
	const auto step = (delta > 0)
		? -st::queueSheetScrollStep
		: st::queueSheetScrollStep;
	_scroll = std::clamp(_scroll + step, 0, maxScroll());
	e->accept();
	update();
}

void QueueSheet::activate(int index) {
	if (index < 0 || index >= int(_rows.size())) {
		return;
	}
	const auto &row = _rows[index];
	const auto document = RowDocument(
		_controller->session().data().message(row.id));
	if (!document) {
		return;
	}
	Media::Player::instance()->play(AudioMsgId(document, row.id));
}

} // namespace Exteraless
