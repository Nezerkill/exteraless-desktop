#include "exteraless/pill_stack.h"

#include "base/unixtime.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "exteraless/config.h"
#include "exteraless/ghost_mode.h"
#include "main/main_session.h"
#include "mtproto/facade.h"
#include "mtproto/mtp_instance.h"
#include "settings/sections/settings_advanced.h"
#include "settings/sections/settings_local_storage.h"
#include "storage/cache/storage_cache_database.h"
#include "styles/style_basic.h"
#include "styles/style_dialogs.h"
#include "ui/painter.h"
#include "ui/text/format_values.h"
#include "window/window_session_controller.h"

#include <QtCore/QFile>
#include <QtCore/QtGlobal>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <optional>

namespace Exteraless {
namespace {

constexpr auto kTimerInterval = crl::time(2000);
constexpr auto kPillPadding = 4;
constexpr auto kPillGap = 4;
constexpr auto kMaxPillTextWidth = 120;

struct CpuSample {
	qint64 idle = 0;
	qint64 total = 0;
};

struct NetSample {
	qint64 rx = 0;
	qint64 tx = 0;
};

[[nodiscard]] std::optional<CpuSample> ReadCpuSample() {
	auto file = QFile(u"/proc/stat"_q);
	if (!file.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	const auto line = file.readLine().simplified();
	if (!line.startsWith("cpu ")) {
		return std::nullopt;
	}
	const auto parts = QString::fromLatin1(line).split(
		' ',
		Qt::SkipEmptyParts);
	if (parts.size() < 8) {
		return std::nullopt;
	}
	auto total = qint64(0);
	for (auto i = 1; i != parts.size() && i <= 8; ++i) {
		total += parts[i].toLongLong();
	}
	const auto idle = parts[4].toLongLong() + parts[5].toLongLong();
	return CpuSample{ idle, total };
}

[[nodiscard]] std::optional<int> ReadMemoryUsage() {
	auto file = QFile(u"/proc/meminfo"_q);
	if (!file.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	auto total = qint64(0);
	auto available = qint64(0);
	while (true) {
		const auto line = file.readLine();
		if (line.isEmpty()) {
			break;
		}
		if (line.startsWith("MemTotal:")) {
			total = line.mid(9).trimmed().split(' ').front().toLongLong();
		} else if (line.startsWith("MemAvailable:")) {
			available = line.mid(13).trimmed().split(' ').front().toLongLong();
		}
	}
	if (total <= 0) {
		return std::nullopt;
	}
	return int(qBound(
		qint64(0),
		(total - available) * 100 / total,
		qint64(100)));
}

[[nodiscard]] std::optional<NetSample> ReadNetSample() {
	auto file = QFile(u"/proc/net/dev"_q);
	if (!file.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	auto rx = qint64(0);
	auto tx = qint64(0);
	auto lineIndex = 0;
	while (true) {
		const auto line = file.readLine();
		if (line.isEmpty()) {
			break;
		}
		if (++lineIndex <= 2) {
			continue;
		}
		const auto colon = line.indexOf(':');
		if (colon < 0) {
			continue;
		}
		const auto name = line.left(colon).trimmed();
		if (name == "lo") {
			continue;
		}
		const auto fields = line.mid(colon + 1).simplified().split(' ');
		if (fields.size() < 9) {
			continue;
		}
		rx += fields[0].toLongLong();
		tx += fields[8].toLongLong();
	}
	return NetSample{ rx, tx };
}

[[nodiscard]] QString DcStateText(int32 state) {
	if (state == MTP::ConnectedState) {
		return u"online"_q;
	} else if (state == MTP::ConnectingState) {
		return u"connecting"_q;
	}
	return u"offline"_q;
}

[[nodiscard]] QString LastSeenText(not_null<UserData*> user) {
	const auto now = base::unixtime::now();
	const auto status = user->lastseen();
	if (status.isOnline(now)) {
		return u"online"_q;
	} else if (const auto till = status.onlineTill()) {
		const auto seconds = std::max(0, now - till);
		if (seconds < 60) {
			return u"now"_q;
		} else if (seconds < 3600) {
			return u"%1m"_q.arg(seconds / 60);
		} else if (seconds < 86400) {
			return u"%1h"_q.arg(seconds / 3600);
		}
		return u"%1d"_q.arg(seconds / 86400);
	} else if (status.isRecently()) {
		return u"recently"_q;
	} else if (status.isWithinWeek()) {
		return u"week"_q;
	} else if (status.isWithinMonth()) {
		return u"month"_q;
	} else if (status.isLongAgo()) {
		return u"long ago"_q;
	}
	return u"—"_q;
}

} // namespace

PillStack::PillStack(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: RpWidget(parent)
, _controller(controller)
, _timer([=] { tick(); }) {
	_pills = {
		{ Kind::Cpu, u"CPU …"_q },
		{ Kind::Ram, u"RAM …"_q },
		{ Kind::Net, u"Net …"_q },
		{ Kind::Dc, u"DC …"_q },
		{ Kind::Proxy, u"Proxy …"_q },
		{ Kind::Cache, u"Cache …"_q },
		{ Kind::Ghost, u"Ghost …"_q },
		{ Kind::LastSeen, u"Seen …"_q },
	};
	setMouseTracking(true);
	setCursor(Qt::PointingHandCursor);
	setFixedHeight(st::dialogsFilter.heightMin);

	_netTimer.start();
	_enabled = Config::Instance().pillStackEnabled();
	_pillsMask = Config::Instance().pillStackPills();
	setVisible(_enabled);

	_controller->session().data().cache().statsOnMain(
	) | rpl::on_next([=](Storage::Cache::Database::Stats stats) {
		_cacheBytes = stats.full.totalSize;
		updatePills();
	}, lifetime());
	_controller->session().data().cacheBigFile().statsOnMain(
	) | rpl::on_next([=](Storage::Cache::Database::Stats stats) {
		_cacheBigBytes = stats.full.totalSize;
		updatePills();
	}, lifetime());

	_timer.callEach(kTimerInterval);
	updatePills();
	applySavedOrder();
}

rpl::producer<> PillStack::visibilityChanges() const {
	return _visibilityChanges.events();
}

void PillStack::tick() {
	const auto enabled = Config::Instance().pillStackEnabled();
	if (enabled != _enabled) {
		_enabled = enabled;
		setVisible(enabled);
		_visibilityChanges.fire({});
	}
	if (enabled) {
		updatePills();
	}
}

void PillStack::updatePills() {
	auto changed = false;
	if (Config::Instance().pillStackOrder() != _order) {
		applySavedOrder();
		changed = true;
	}
	const auto setText = [&](Kind kind, QString text, bool active = false) {
		const auto i = ranges::find(_pills, kind, &Pill::kind);
		if (i == _pills.end()) {
			return;
		}
		if (i->text != text || i->active != active) {
			i->text = std::move(text);
			i->active = active;
			changed = true;
		}
	};

	if (const auto sample = ReadCpuSample()) {
		if (_cpuValid) {
			const auto total = sample->total - _previousCpuTotal;
			const auto idle = sample->idle - _previousCpuIdle;
			if (total > 0) {
				const auto percent = qBound(
					qint64(0),
					(total - idle) * 100 / total,
					qint64(100));
				setText(Kind::Cpu, u"CPU %1%"_q.arg(percent));
			}
		}
		_previousCpuIdle = sample->idle;
		_previousCpuTotal = sample->total;
		_cpuValid = true;
	} else {
		_cpuValid = false;
	}

	if (const auto memory = ReadMemoryUsage()) {
		setText(Kind::Ram, u"RAM %1%"_q.arg(*memory));
	}

	if (const auto sample = ReadNetSample()) {
		const auto now = _netTimer.elapsed();
		if (_netValid) {
			const auto elapsed = now - _previousNetAt;
			if (elapsed > 0) {
				const auto rx = std::max(
					qint64(0),
					(sample->rx - _previousRx) * 1000 / elapsed);
				const auto tx = std::max(
					qint64(0),
					(sample->tx - _previousTx) * 1000 / elapsed);
				const auto speed = std::max(rx, tx);
				const auto arrow = (rx >= tx) ? u"↓"_q : u"↑"_q;
				setText(
					Kind::Net,
					arrow + Ui::FormatSizeText(speed) + u"/s"_q);
			}
		}
		_previousRx = sample->rx;
		_previousTx = sample->tx;
		_previousNetAt = now;
		_netValid = true;
	} else {
		_netValid = false;
	}

	auto &mtp = _controller->session().mtp();
	const auto dc = u"DC%1"_q.arg(mtp.mainDcId());
	const auto ping = mtp.pingTime();
	if (ping > 0) {
		setText(Kind::Dc, dc + u" · %1 ms"_q.arg(ping));
	} else {
		setText(Kind::Dc, dc + u" · "_q + DcStateText(mtp.dcstate()));
	}

	const auto &proxy = Core::App().settings().proxy();
	const auto proxyEnabled = proxy.isEnabled();
	setText(
		Kind::Proxy,
		proxyEnabled
			? u"Proxy on"_q
			: (proxy.isSystem() ? u"Proxy system"_q : u"Proxy off"_q),
		proxyEnabled);

	setText(Kind::Cache, u"Cache %1"_q.arg(Ui::FormatSizeText(
		_cacheBytes + _cacheBigBytes)));

	const auto ghost = IsGhostActive();
	setText(
		Kind::Ghost,
		ghost ? u"Ghost on"_q : u"Ghost off"_q,
		ghost);

	const auto user = _controller->session().user();
	setText(Kind::LastSeen, u"Seen %1"_q.arg(LastSeenText(user)));

	const auto mask = Config::Instance().pillStackPills();
	if (mask != _pillsMask) {
		_pillsMask = mask;
		changed = true;
	}

	if (changed) {
		updateLayout();
		update();
	}
}

void PillStack::applySavedOrder() {
	_order = Config::Instance().pillStackOrder();
	auto ordered = std::vector<Pill>();
	ordered.reserve(_pills.size());
	for (const auto &key : _order.split(',')) {
		if (const auto kind = KindFromKey(key)) {
			const auto i = ranges::find(_pills, *kind, &Pill::kind);
			if (i != _pills.end()) {
				ordered.push_back(*i);
			}
		}
	}
	if (ordered.size() == _pills.size()) {
		_pills = std::move(ordered);
	}
	updateLayout();
}

void PillStack::saveOrder() {
	auto keys = QStringList();
	keys.reserve(int(_pills.size()));
	for (const auto &pill : _pills) {
		keys.push_back(KindKey(pill.kind));
	}
	Config::Instance().setPillStackOrder(keys.join(','));
	_order = Config::Instance().pillStackOrder();
}

QString PillStack::KindKey(Kind kind) {
	switch (kind) {
	case Kind::Cpu: return u"cpu"_q;
	case Kind::Ram: return u"ram"_q;
	case Kind::Net: return u"net"_q;
	case Kind::Dc: return u"dc"_q;
	case Kind::Proxy: return u"proxy"_q;
	case Kind::Cache: return u"cache"_q;
	case Kind::Ghost: return u"ghost"_q;
	case Kind::LastSeen: return u"lastseen"_q;
	}
	return QString();
}

std::optional<PillStack::Kind> PillStack::KindFromKey(const QString &key) {
	if (key == u"cpu"_q) return Kind::Cpu;
	if (key == u"ram"_q) return Kind::Ram;
	if (key == u"net"_q) return Kind::Net;
	if (key == u"dc"_q) return Kind::Dc;
	if (key == u"proxy"_q) return Kind::Proxy;
	if (key == u"cache"_q) return Kind::Cache;
	if (key == u"ghost"_q) return Kind::Ghost;
	if (key == u"lastseen"_q) return Kind::LastSeen;
	return std::nullopt;
}

void PillStack::updateLayout() {
	const auto pillHeight = st::dialogsFilter.heightMin - 14;
	const auto rowHeight = pillHeight + 6;
	const auto leftPadding = st::dialogsFilterPadding.x();
	const auto right = width() - leftPadding;
	auto left = leftPadding;
	auto top = 4;
	auto rowBottom = top + pillHeight;
	auto anyPlaced = false;
	for (auto &pill : _pills) {
		if (!isPillEnabled(pill.kind)) {
			pill.rect = QRect();
			continue;
		}
		const auto textWidth = std::min(
			st::semiboldFont->width(pill.text),
			kMaxPillTextWidth);
		const auto width = textWidth + 2 * kPillPadding;
		if (left + width > right && left > leftPadding) {
			left = leftPadding;
			top += rowHeight;
			rowBottom = top + pillHeight;
		}
		if (left + width > right) {
			pill.rect = QRect();
			continue;
		}
		pill.rect = QRect(left, top, width, pillHeight);
		anyPlaced = true;
		left += width + kPillGap;
	}
	const auto desiredHeight = anyPlaced
		? std::max(st::dialogsFilter.heightMin, rowBottom + 4)
		: 0;
	if (height() != desiredHeight) {
		setFixedHeight(desiredHeight);
	}
}

void PillStack::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);
	p.setFont(st::semiboldFont);

	for (auto i = 0; i != int(_pills.size()); ++i) {
		const auto &pill = _pills[i];
		if (pill.rect.isEmpty()) {
			continue;
		}
		const auto radius = pill.rect.height() / 2.;
		const auto over = (_hovered == i) || (_pressed == i);
		p.setPen(Qt::NoPen);
		p.setBrush(pill.active
			? st::windowActiveTextFg->c
			: (over ? st::windowBgActive->c : st::windowBgOver->c));
		p.drawRoundedRect(pill.rect, radius, radius);
		p.setPen(pill.active ? st::windowBg->c : st::windowFg->c);
		p.drawText(
			pill.rect,
			Qt::AlignCenter,
			st::semiboldFont->elided(
				pill.text,
				pill.rect.width() - 2 * kPillPadding));
	}
}

void PillStack::resizeEvent(QResizeEvent *e) {
	RpWidget::resizeEvent(e);
	updateLayout();
}

void PillStack::mousePressEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) {
		return;
	}
	_pressed = indexAt(e->pos());
	_dragIndex = _pressed;
	_pressPoint = e->pos();
	_dragging = false;
	update();
}

void PillStack::mouseReleaseEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton && _dragging) {
		saveOrder();
		_dragging = false;
		_dragIndex = -1;
		_pressed = -1;
		setCursor(Qt::PointingHandCursor);
		update();
		return;
	}
	const auto index = indexAt(e->pos());
	if (e->button() == Qt::LeftButton
		&& index >= 0
		&& index == _pressed) {
		activate(_pills[index].kind);
	}
	_pressed = -1;
	_dragIndex = -1;
	update();
}

void PillStack::mouseMoveEvent(QMouseEvent *e) {
	const auto index = indexAt(e->pos());
	if (index != _hovered) {
		_hovered = index;
		update();
	}
	if (_pressed < 0
		|| _dragIndex < 0
		|| !(e->buttons() & Qt::LeftButton)) {
		return;
	}
	if (!_dragging) {
		if ((e->pos() - _pressPoint).manhattanLength()
			< QApplication::startDragDistance()) {
			return;
		}
		_dragging = true;
		setCursor(Qt::ClosedHandCursor);
	}
	const auto target = indexAt(e->pos());
	if (target < 0 || target == _dragIndex) {
		return;
	}
	auto moved = _pills[_dragIndex];
	_pills.erase(_pills.begin() + _dragIndex);
	_pills.insert(_pills.begin() + target, std::move(moved));
	_dragIndex = target;
	_pressed = target;
	updateLayout();
	update();
}

void PillStack::leaveEventHook(QEvent *e) {
	_hovered = -1;
	if (!_dragging) {
		_pressed = -1;
		_dragIndex = -1;
	}
	update();
}

void PillStack::activate(Kind kind) {
	switch (kind) {
	case Kind::Ghost:
		SetGhostActive(!IsGhostActive());
		updatePills();
		break;
	case Kind::Proxy:
		_controller->showSettings(Settings::AdvancedId());
		break;
	case Kind::Cache:
		_controller->showSettings(Settings::LocalStorageId());
		break;
	case Kind::LastSeen:
		updatePills();
		break;
	default:
		break;
	}
}

int PillStack::indexAt(QPoint position) const {
	for (auto i = 0; i != int(_pills.size()); ++i) {
		if (_pills[i].rect.contains(position)) {
			return i;
		}
	}
	return -1;
}

bool PillStack::isPillEnabled(Kind kind) const {
	const auto pill = [&] {
		switch (kind) {
		case Kind::Cpu: return PillStackPill::Cpu;
		case Kind::Ram: return PillStackPill::Ram;
		case Kind::Net: return PillStackPill::Net;
		case Kind::Dc: return PillStackPill::Dc;
		case Kind::Proxy: return PillStackPill::Proxy;
		case Kind::Cache: return PillStackPill::Cache;
		case Kind::Ghost: return PillStackPill::Ghost;
		case Kind::LastSeen: return PillStackPill::LastSeen;
		}
		return PillStackPill::Cpu;
	}();
	return HasPillStackPill(Config::Instance().pillStackPills(), pill);
}

} // namespace Exteraless
