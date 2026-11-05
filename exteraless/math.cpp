#include "exteraless/math.h"

#include <QtMath>

#include <cmath>
#include <optional>

namespace Exteraless {
namespace {

constexpr auto kMaxExpressionLength = 256;
constexpr auto kMaxResultLength = 64;
constexpr auto kPi = 3.14159265358979323846;
constexpr auto kE = 2.71828182845904523536;

class Parser final {
public:
	explicit Parser(const QString &text) : _text(text) {
	}

	[[nodiscard]] std::optional<double> parse() {
		skipSpaces();
		const auto value = parseExpression();
		if (!value) {
			return std::nullopt;
		}
		skipSpaces();
		if (_position != _text.size()) {
			return std::nullopt;
		}
		return value;
	}

private:
	void skipSpaces() {
		while (_position != _text.size() && _text[_position].isSpace()) {
			++_position;
		}
	}

	[[nodiscard]] bool consume(QChar ch) {
		skipSpaces();
		if (_position != _text.size() && _text[_position] == ch) {
			++_position;
			return true;
		}
		return false;
	}

	[[nodiscard]] std::optional<double> parseExpression() {
		auto value = parseTerm();
		if (!value) {
			return std::nullopt;
		}
		while (true) {
			skipSpaces();
			if (_position == _text.size()) {
				return value;
			}
			const auto ch = _text[_position];
			if (ch != u'+' && ch != u'-') {
				return value;
			}
			++_position;
			const auto right = parseTerm();
			if (!right) {
				return std::nullopt;
			}
			*value = (ch == u'+') ? (*value + *right) : (*value - *right);
		}
	}

	[[nodiscard]] std::optional<double> parseTerm() {
		auto value = parsePower();
		if (!value) {
			return std::nullopt;
		}
		while (true) {
			skipSpaces();
			if (_position == _text.size()) {
				return value;
			}
			const auto ch = _text[_position];
			if (ch != u'*' && ch != u'/' && ch != u'%') {
				return value;
			}
			++_position;
			const auto right = parsePower();
			if (!right) {
				return std::nullopt;
			}
			if (ch == u'*') {
				*value = *value * *right;
			} else if (ch == u'/') {
				if (*right == 0.) {
					return std::nullopt;
				}
				*value = *value / *right;
			} else {
				if (*right == 0.) {
					return std::nullopt;
				}
				*value = std::fmod(*value, *right);
			}
		}
	}

	[[nodiscard]] std::optional<double> parsePower() {
		const auto value = parseUnary();
		if (!value) {
			return std::nullopt;
		}
		skipSpaces();
		if (_position != _text.size() && _text[_position] == u'^') {
			++_position;
			const auto exponent = parsePower();
			if (!exponent) {
				return std::nullopt;
			}
			return std::pow(*value, *exponent);
		}
		return value;
	}

	[[nodiscard]] std::optional<double> parseUnary() {
		skipSpaces();
		if (_position != _text.size()) {
			const auto ch = _text[_position];
			if (ch == u'+' || ch == u'-') {
				++_position;
				const auto value = parseUnary();
				if (!value) {
					return std::nullopt;
				}
				return (ch == u'-') ? -*value : *value;
			}
		}
		return parsePrimary();
	}

	[[nodiscard]] std::optional<double> parsePrimary() {
		skipSpaces();
		if (_position == _text.size()) {
			return std::nullopt;
		}
		if (consume(u'(')) {
			const auto value = parseExpression();
			if (!value || !consume(u')')) {
				return std::nullopt;
			}
			return value;
		}
		if (_text[_position].isDigit() || _text[_position] == u'.') {
			return parseNumber();
		}
		if (_text[_position].isLetter()) {
			return parseIdentifier();
		}
		return std::nullopt;
	}

	[[nodiscard]] std::optional<double> parseNumber() {
		const auto start = _position;
		auto seenDot = false;
		while (_position != _text.size()) {
			const auto ch = _text[_position];
			if (ch.isDigit()) {
				++_position;
			} else if (ch == u'.' && !seenDot) {
				seenDot = true;
				++_position;
			} else {
				break;
			}
		}
		if (_position == start) {
			return std::nullopt;
		}
		auto ok = false;
		const auto value = _text.mid(start, _position - start).toDouble(&ok);
		return ok ? std::optional<double>(value) : std::nullopt;
	}

	[[nodiscard]] std::optional<double> parseIdentifier() {
		const auto start = _position;
		while (_position != _text.size() && _text[_position].isLetter()) {
			++_position;
		}
		const auto name = _text.mid(start, _position - start).toLower();
		skipSpaces();
		if (_position == _text.size() || _text[_position] != u'(') {
			if (name == QStringLiteral("pi")) {
				return kPi;
			} else if (name == QStringLiteral("e")) {
				return kE;
			}
			return std::nullopt;
		}
		++_position;
		const auto argument = parseExpression();
		if (!argument || !consume(u')')) {
			return std::nullopt;
		}
		const auto x = *argument;
		if (name == QStringLiteral("sqrt")) {
			return (x >= 0.) ? std::optional<double>(std::sqrt(x)) : std::nullopt;
		} else if (name == QStringLiteral("abs")) {
			return std::abs(x);
		} else if (name == QStringLiteral("sin")) {
			return std::sin(x);
		} else if (name == QStringLiteral("cos")) {
			return std::cos(x);
		} else if (name == QStringLiteral("tan")) {
			return std::tan(x);
		} else if (name == QStringLiteral("asin")) {
			return (x >= -1. && x <= 1.)
				? std::optional<double>(std::asin(x))
				: std::nullopt;
		} else if (name == QStringLiteral("acos")) {
			return (x >= -1. && x <= 1.)
				? std::optional<double>(std::acos(x))
				: std::nullopt;
		} else if (name == QStringLiteral("atan")) {
			return std::atan(x);
		} else if (name == QStringLiteral("ln")) {
			return (x > 0.) ? std::optional<double>(std::log(x)) : std::nullopt;
		} else if (name == QStringLiteral("log")) {
			return (x > 0.)
				? std::optional<double>(std::log10(x))
				: std::nullopt;
		} else if (name == QStringLiteral("exp")) {
			return std::exp(x);
		} else if (name == QStringLiteral("floor")) {
			return std::floor(x);
		} else if (name == QStringLiteral("ceil")) {
			return std::ceil(x);
		} else if (name == QStringLiteral("round")) {
			return std::round(x);
		}
		return std::nullopt;
	}

	QString _text;
	int _position = 0;
};

} // namespace

std::optional<QString> EvaluateMathExpression(const QString &expression) {
	const auto trimmed = expression.trimmed();
	if (trimmed.isEmpty()
		|| trimmed.size() > kMaxExpressionLength) {
		return std::nullopt;
	}
	const auto value = Parser(trimmed).parse();
	if (!value || !std::isfinite(*value)) {
		return std::nullopt;
	}
	auto result = *value;
	if (std::abs(result) < 1e-12) {
		result = 0.;
	}
	const auto text = QString::number(result, 'g', 10);
	if (text.isEmpty() || text.size() > kMaxResultLength) {
		return std::nullopt;
	}
	return text;
}

} // namespace Exteraless
