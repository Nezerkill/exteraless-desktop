#pragma once

#include "base/timer.h"
#include "ui/rp_widget.h"

#include <QElapsedTimer>
#include <QPoint>
#include <rpl/event_stream.h>

#include <optional>
#include <vector>

namespace Window {
class SessionController;
} // namespace Window

namespace Exteraless {

class PillStack final : public Ui::RpWidget {
public:
	PillStack(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<> visibilityChanges() const;
	[[nodiscard]] bool enabled() const {
		return _enabled;
	}

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	enum class Kind {
		Cpu,
		Ram,
		Net,
		Dc,
		Proxy,
		Cache,
		Ghost,
		LastSeen,
		Weather,
		Btc,
		Eth,
		Usd,
		Eur,
		Gold,
	};

	struct Pill {
		Kind kind = Kind::Cpu;
		QString text;
		bool active = false;
		QRect rect;
	};

	void tick();
	void updatePills();
	void updateLayout();
	void activate(Kind kind);
	void applySavedOrder();
	void saveOrder();
	[[nodiscard]] static QString KindKey(Kind kind);
	[[nodiscard]] static std::optional<Kind> KindFromKey(const QString &key);
	[[nodiscard]] int indexAt(QPoint position) const;
	[[nodiscard]] bool isPillEnabled(Kind kind) const;

	const not_null<Window::SessionController*> _controller;
	std::vector<Pill> _pills;
	base::Timer _timer;
	QElapsedTimer _netTimer;
	qint64 _cacheBytes = 0;
	qint64 _cacheBigBytes = 0;
	int _pillsMask = 0;
	qint64 _previousCpuIdle = 0;
	qint64 _previousCpuTotal = 0;
	qint64 _previousRx = 0;
	qint64 _previousTx = 0;
	qint64 _previousNetAt = 0;
	int _pressed = -1;
	int _hovered = -1;
	int _dragIndex = -1;
	QPoint _pressPoint;
	QString _order;
	bool _dragging = false;
	bool _cpuValid = false;
	bool _netValid = false;
	bool _enabled = false;
	rpl::event_stream<> _visibilityChanges;
};

} // namespace Exteraless
