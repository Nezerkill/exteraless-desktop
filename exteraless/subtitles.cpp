#include "exteraless/subtitles.h"

#include <QStringList>

#include <algorithm>
#include <utility>

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

[[nodiscard]] bool StartsWithWord(const QString &line, QStringView word) {
	if (word.isEmpty() || !QStringView(line).startsWith(word)) {
		return false;
	}
	if (line.size() == word.size()) {
		return true;
	}
	const auto next = line[word.size()];
	return next.isSpace() || next == u'#' || next == u'-' || next == u':';
}

[[nodiscard]] bool IsDirective(const QString &line) {
	return StartsWithWord(line, u"NOTE")
		|| StartsWithWord(line, u"STYLE")
		|| StartsWithWord(line, u"REGION");
}

[[nodiscard]] bool ParseInteger(QStringView text, qint64 limit, qint64 &value) {
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
	if (parts.size() != 2 && parts.size() != 3) {
		return false;
	}
	auto seconds = parts.back();
	auto fraction = QStringView();
	const auto dot = seconds.indexOf(u'.');
	const auto comma = seconds.indexOf(u',');
	const auto separator = (dot >= 0) ? dot : comma;
	if (separator >= 0) {
		fraction = seconds.mid(separator + 1);
		seconds = seconds.first(separator);
	}
	if (fraction.size() > 3) {
		fraction = fraction.first(3);
	}
	auto secondsValue = qint64(0);
	auto fractionValue = qint64(0);
	if (!ParseInteger(seconds, 10000, secondsValue)) {
		return false;
	}
	if (!fraction.isEmpty()) {
		if (!ParseInteger(fraction, 999, fractionValue)) {
			return false;
		}
		for (auto i = fraction.size(); i != 3; ++i) {
			fractionValue *= 10;
		}
	}
	auto minutesValue = qint64(0);
	auto hoursValue = qint64(0);
	if (parts.size() == 2) {
		if (!ParseInteger(parts[0], 10000, minutesValue)) {
			return false;
		}
	} else {
		if (!ParseInteger(parts[0], 1000, hoursValue)
			|| !ParseInteger(parts[1], 10000, minutesValue)) {
			return false;
		}
	}
	milliseconds = ((hoursValue * 60 + minutesValue) * 60 + secondsValue) * 1000
		+ fractionValue;
	return true;
}

[[nodiscard]] bool ParseTiming(const QString &line, qint64 &start, qint64 &end) {
	const auto arrow = line.indexOf(QStringLiteral("-->"));
	if (arrow < 0) {
		return false;
	}
	const auto before = QStringView(line).left(arrow).trimmed();
	auto after = QStringView(line).mid(arrow + 3).trimmed();
	for (auto i = qsizetype(0); i != after.size(); ++i) {
		if (after[i].isSpace()) {
			after = after.left(i);
			break;
		}
	}
	return ParseTimestamp(before, start) && ParseTimestamp(after, end);
}

[[nodiscard]] QString StripTags(QString text) {
	for (auto i = qsizetype(0); i != text.size();) {
		const auto ch = text[i];
		if (ch != u'<' && ch != u'{') {
			++i;
			continue;
		}
		const auto close = (ch == u'<') ? u'>' : u'}';
		const auto position = text.indexOf(close, i + 1);
		if (position <= 0) {
			++i;
			continue;
		}
		text.remove(i, position - i + 1);
	}
	return text;
}

[[nodiscard]] bool HasTiming(const QString &line) {
	return line.contains(QStringLiteral("-->"));
}

[[nodiscard]] QString TextFromLines(const QStringList &lines) {
	auto result = QString();
	for (const auto &line : lines) {
		const auto cleaned = StripTags(line).trimmed();
		if (cleaned.isEmpty()) {
			continue;
		}
		if (!result.isEmpty()) {
			result += u'\n';
		}
		result += cleaned;
	}
	return result;
}

} // namespace

QList<Cue> ParseSubtitles(const QByteArray &data) {
	auto cues = QList<Cue>();
	if (data.isEmpty()) {
		return cues;
	}
	auto text = Decode(data);
	text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
	text.replace(u'\r', u'\n');
	const auto lines = text.split(u'\n');
	const auto count = lines.size();
	auto index = qsizetype(0);
	while (index < count) {
		const auto line = lines[index].trimmed();
		if (line.isEmpty()) {
			++index;
			continue;
		}
		if (line.startsWith(QStringLiteral("WEBVTT"))) {
			++index;
			continue;
		}
		if (IsDirective(line)) {
			while (index < count) {
				const auto current = lines[index].trimmed();
				if (current.isEmpty() || HasTiming(current)) {
					break;
				}
				++index;
			}
			continue;
		}
		auto start = qint64(0);
		auto end = qint64(0);
		if (!ParseTiming(line, start, end)) {
			++index;
			continue;
		}
		auto textLines = QStringList();
		auto next = index + 1;
		while (next < count) {
			const auto current = lines[next];
			const auto trimmed = current.trimmed();
			if (trimmed.isEmpty() || HasTiming(trimmed)) {
				break;
			}
			textLines.append(current);
			++next;
		}
		auto cueText = TextFromLines(textLines);
		if (!cueText.isEmpty()) {
			cues.append(Cue{ start, end, std::move(cueText) });
		}
		index = next;
	}
	std::sort(cues.begin(), cues.end(), [](const Cue &a, const Cue &b) {
		return a.start < b.start;
	});
	return cues;
}

QString ActiveCueText(const QList<Cue> &cues, qint64 position) {
	auto result = QString();
	for (const auto &cue : cues) {
		if (cue.text.isEmpty()
			|| position < cue.start
			|| position >= cue.end) {
			continue;
		}
		if (!result.isEmpty()) {
			result += u'\n';
		}
		result += cue.text;
	}
	return result;
}

QString VideoKey(qint64 docId) {
	return QStringLiteral("doc_%1").arg(docId);
}

} // namespace Exteraless
