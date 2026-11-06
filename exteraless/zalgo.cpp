#include "exteraless/zalgo.h"

#include <QChar>

namespace Exteraless {
namespace {

constexpr auto kZalgoRunLength = 4;
constexpr auto kMinTextLength = 4;
constexpr auto kMaxTextLength = 2048;

[[nodiscard]] bool IsZalgoMark(QChar ch) {
	switch (QChar::category(ch.unicode())) {
	case QChar::Mark_NonSpacing:
	case QChar::Mark_SpacingCombining:
	case QChar::Mark_Enclosing:
		return true;
	default:
		return false;
	}
}

[[nodiscard]] bool IsSurrogatePairAt(const QString &text, int index) {
	return text[index].isHighSurrogate()
		&& (index + 1 < text.size())
		&& text[index + 1].isLowSurrogate();
}

[[nodiscard]] ZalgoResult CopyWithIdentityMap(const QString &text) {
	auto result = ZalgoResult();
	result.text = text;
	result.map.resize(text.size() + 1);
	for (auto i = 0; i != text.size(); ++i) {
		result.map[i] = i;
	}
	result.map[text.size()] = text.size();
	return result;
}

} // namespace

bool HasZalgoRun(const QString &text) {
	auto run = 0;
	for (const auto ch : text) {
		if (IsZalgoMark(ch)) {
			if (++run >= kZalgoRunLength) {
				return true;
			}
		} else {
			run = 0;
		}
	}
	return false;
}

ZalgoResult ApplyZalgoFilterWithMap(const QString &text) {
	if (text.size() < kMinTextLength
		|| text.size() > kMaxTextLength
		|| !HasZalgoRun(text)) {
		return CopyWithIdentityMap(text);
	}
	auto result = ZalgoResult();
	result.text.reserve(text.size());
	result.map.resize(text.size() + 1);
	for (auto i = 0; i != text.size(); ++i) {
		result.map[i] = result.text.size();
		if (IsSurrogatePairAt(text, i)) {
			result.text += text[i];
			result.text += text[i + 1];
			result.map[i + 1] = result.text.size() - 1;
			++i;
			continue;
		}
		if (IsZalgoMark(text[i])) {
			continue;
		}
		result.text += text[i];
	}
	result.map[text.size()] = result.text.size();
	return result;
}

QString ApplyZalgoFilter(const QString &text) {
	return ApplyZalgoFilterWithMap(text).text;
}

} // namespace Exteraless
