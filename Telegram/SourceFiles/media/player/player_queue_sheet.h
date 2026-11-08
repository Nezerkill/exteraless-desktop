/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/object_ptr.h"
#include "data/data_types.h"
#include "media/player/player_queue.h"
#include "ui/cached_round_corners.h"
#include "ui/layers/layer_widget.h"

#include <QColor>
#include <QHash>
#include <vector>

class DocumentData;
class QPainter;

namespace Ui {
class FlatLabel;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Exteraless {

class QueueSheet final : public Ui::LayerWidget {
public:
	QueueSheet(
		QWidget *parent,
		not_null<Window::SessionController*> controller);
	~QueueSheet();

	static void Show(not_null<Window::SessionController*> controller);

	void parentResized() override;

private:
	struct Row {
		FullMsgId id;
		DocumentId documentId = 0;
		QString title;
		QString performer;
		QString time;
	};

	void setupSubscriptions();
	void setupQueue();
	void refreshRows();
	void refreshAccents();
	void loadAccent(not_null<DocumentData*> document, FullMsgId id);
	void layoutChildren();

	[[nodiscard]] int listTop() const;
	[[nodiscard]] int listHeight() const;
	[[nodiscard]] int maxScroll() const;
	[[nodiscard]] int rowAt(QPoint position) const;

	void paintEvent(QPaintEvent *e) override;
	void paintHeader(QPainter &p);
	void paintRow(QPainter &p, int index, QRect rect);
	void paintEmpty(QPainter &p, QRect rect);

	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;

	void activate(int index);

	const not_null<Window::SessionController*> _controller;
	PlayerQueue _queue;

	object_ptr<Ui::FlatLabel> _title = { nullptr };
	Ui::CornersPixmaps _corners;

	std::vector<Row> _rows;
	QHash<DocumentId, QColor> _accents;

	int _scroll = 0;
	int _pressedRow = -1;
	int _targetRow = -1;
	QPoint _pressPosition;
	bool _dragging = false;

};

} // namespace Exteraless
