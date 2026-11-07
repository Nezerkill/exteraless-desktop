#include "exteraless/lyrics.h"

#include <QStringList>

#include <algorithm>

namespace Exteraless {
namespace {

[[nodiscard]] QString DecodeUtf16(const QByteArray &data, bool littleEndian) {
	const auto raw = reinterpret_cast<const uchar*>(data.constData());
	const auto size = data.size();
	auto result = QString();
	result.reserve((size - 2) / 2);
	for (auto i = 2; i + 1 < size; i += 2) {
		const auto first = littleEndian ? raw[i] : raw[i + 1];
		const auto second = littleEndian ? raw[i + 1] : raw[i];
		result += QChar(ushort(first | (second << 8)));
	}
	return result;
}

[[nodiscard]] QString Decode(const QByteArray &data) {
	if (data.size() >= 3
		&& uchar(data[0]) == 0xEF
		&& uchar(data[1]) == 0xBB
		&& uchar(data[2]) == 0xBF) {
		return QString::fromUtf8(data.constData() + 3, data.size() - 3);
	} else if (data.size() >= 2
		&& uchar(data[0]) == 0xFF
		&& uchar(data[1]) == 0xFE) {
		return DecodeUtf16(data, true);
	} else if (data.size() >= 2
		&& uchar(data[0]) == 0xFE
		&& uchar(data[1]) == 0xFF) {
		return DecodeUtf16(data, false);
	}
	return QString::fromUtf8(data);
}

[[nodiscard]] bool ParseDigits(QStringView text, qint64 limit, qint64 &value) {
	if (text.isEmpty() || text.size() > 9) {
		return false;
	}
	auto result = qint64(0);
	for (const auto ch : text) {
		const auto digit = int(ch.unicode()) - u'0';
		if (digit < 0 || digit > 9) {
			return false;
		}
		result = result * 10 + digit;
	}
	if (result > limit) {
		return false;
	}
	value = result;
	return true;
}

[[nodiscard]] bool ParseTimestamp(QStringView text, qint64 &milliseconds) {
	text = text.trimmed();
	const auto parts = text.split(u':');
	if (parts.size() != 2) {
		return false;
	}
	auto seconds = parts[1];
	auto fraction = QStringView();
	const auto dot = seconds.indexOf(u'.');
	if (dot >= 0) {
		fraction = seconds.mid(dot + 1);
		seconds = seconds.left(dot);
	}
	if (fraction.size() > 3) {
		return false;
	}
	auto minutesValue = qint64(0);
	auto secondsValue = qint64(0);
	auto fractionValue = qint64(0);
	if (!ParseDigits(parts[0], 100000, minutesValue)
		|| !ParseDigits(seconds, 59, secondsValue)) {
		return false;
	}
	if (!fraction.isEmpty()) {
		if (!ParseDigits(fraction, 999, fractionValue)) {
			return false;
		}
		for (auto i = fraction.size(); i != 3; ++i) {
			fractionValue *= 10;
		}
	}
	milliseconds = (minutesValue * 60 + secondsValue) * 1000 + fractionValue;
	return true;
}

[[nodiscard]] bool ParseOffset(QStringView text, qint64 &value) {
	text = text.trimmed();
	auto sign = qint64(1);
	if (text.startsWith(u'+')) {
		text = text.mid(1);
	} else if (text.startsWith(u'-')) {
		sign = -1;
		text = text.mid(1);
	}
	auto magnitude = qint64(0);
	if (!ParseDigits(text, 86400000, magnitude)) {
		return false;
	}
	value = sign * magnitude;
	return true;
}

[[nodiscard]] QString StripWordTimes(const QString &text) {
	auto result = text;
	auto i = qsizetype(0);
	while (i < result.size()) {
		if (result[i] != u'<') {
			++i;
			continue;
		}
		const auto close = result.indexOf(u'>', i + 1);
		if (close < 0) {
			break;
		}
		auto ignored = qint64(0);
		const auto inner = QStringView(result).mid(i + 1, close - i - 1);
		if (!ParseTimestamp(inner, ignored)) {
			++i;
			continue;
		}
		result.remove(i, close - i + 1);
	}
	return result;
}

} // namespace

Lyrics ParseLyrics(const QByteArray &data) {
	auto result = Lyrics();
	if (data.isEmpty()) {
		result.instrumental = true;
		return result;
	}
	auto text = Decode(data);
	text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
	text.replace(u'\r', u'\n');

	const auto rows = text.split(u'\n');
	auto timed = false;
	auto untimed = QStringList();
	auto offset = qint64(0);
	for (const auto &row : rows) {
		const auto line = row.trimmed();
		if (line.isEmpty()) {
			continue;
		}
		auto times = QList<qint64>();
		auto tagged = false;
		auto rest = QStringView(line);
		while (rest.startsWith(u'[')) {
			const auto close = rest.indexOf(u']');
			if (close < 0) {
				break;
			}
			const auto inner = rest.mid(1, close - 1).trimmed();
			auto value = qint64(0);
			if (inner.startsWith(u"offset:")) {
				if (ParseOffset(inner.mid(7), value)) {
					offset = value;
				}
			} else if (ParseTimestamp(inner, value)) {
				times.append(value);
			}
			tagged = true;
			rest = rest.mid(close + 1);
		}
		if (rest.startsWith(u'[')) {
			continue; // unterminated tag marks a broken line
		}
		const auto content = StripWordTimes(rest.toString()).trimmed();
		if (content.isEmpty()) {
			continue;
		}
		if (!times.isEmpty()) {
			timed = true;
			for (const auto time : times) {
				result.lines.append(Line{ time, content });
			}
		} else if (!tagged) {
			untimed.append(content);
		}
	}
	if (timed) {
		for (auto &line : result.lines) {
			line.ms += offset;
		}
		std::stable_sort(
			result.lines.begin(),
			result.lines.end(),
			[](const Line &a, const Line &b) {
				return a.ms < b.ms;
			});
		result.synced = true;
	} else if (!untimed.isEmpty()) {
		result.lines.append(Line{ 0, untimed.join(u'\n') });
	}
	result.instrumental = result.lines.isEmpty();
	return result;
}

int ActiveIndex(const Lyrics &lyrics, qint64 positionMs) {
	const auto &lines = lyrics.lines;
	auto left = qsizetype(0);
	auto right = lines.size() - 1;
	auto found = -1;
	while (left <= right) {
		const auto middle = left + (right - left) / 2;
		if (lines[middle].ms <= positionMs) {
			found = int(middle);
			left = middle + 1;
		} else {
			right = middle - 1;
		}
	}
	return found;
}

} // namespace Exteraless
