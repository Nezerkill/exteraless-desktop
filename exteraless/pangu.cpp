#include "exteraless/pangu.h"

namespace Exteraless {
namespace {

[[nodiscard]] bool IsCjk(QChar ch) {
	const auto code = ch.unicode();
	return (code >= 0x2E80 && code <= 0x2EFF)
		|| (code >= 0x3040 && code <= 0x30FF)
		|| (code >= 0x3400 && code <= 0x4DBF)
		|| (code >= 0x4E00 && code <= 0x9FFF)
		|| (code >= 0xAC00 && code <= 0xD7AF)
		|| (code >= 0xF900 && code <= 0xFAFF);
}

[[nodiscard]] bool IsAsciiAlnum(QChar ch) {
	const auto code = ch.unicode();
	return (code >= '0' && code <= '9')
		|| (code >= 'A' && code <= 'Z')
		|| (code >= 'a' && code <= 'z');
}

} // namespace

PanguResult ApplyPanguWithMap(const QString &text) {
	auto result = PanguResult();
	result.text.reserve(text.size() + text.size() / 4);
	result.map.resize(text.size() + 1);
	auto previousCjk = false;
	auto previousAscii = false;
	for (auto i = 0; i != text.size(); ++i) {
		const auto ch = text[i];
		const auto cjk = IsCjk(ch);
		const auto ascii = IsAsciiAlnum(ch);
		if ((previousCjk && ascii) || (previousAscii && cjk)) {
			result.text += u' ';
		}
		result.map[i] = result.text.size();
		result.text += ch;
		previousCjk = cjk;
		previousAscii = ascii;
	}
	result.map[text.size()] = result.text.size();
	return result;
}

QString ApplyPangu(const QString &text) {
	return ApplyPanguWithMap(text).text;
}

} // namespace Exteraless
