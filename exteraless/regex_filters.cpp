#include "exteraless/regex_filters.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <utility>

namespace Exteraless {
namespace {

constexpr auto kMaxCacheEntries = 4096;

[[nodiscard]] QRegularExpression BuildExpression(const Filter &filter) {
	auto options = QRegularExpression::PatternOptions(
		QRegularExpression::NoPatternOption);
	if (filter.caseInsensitive) {
		options |= QRegularExpression::CaseInsensitiveOption;
	}
	auto result = QRegularExpression(filter.regex, options);
	result.optimize();
	return result;
}

[[nodiscard]] Filter ParseFilter(const QJsonObject &object) {
	auto result = Filter();
	result.regex = object.value(QStringLiteral("regex")).toString();
	result.caseInsensitive = object.value(
		QStringLiteral("caseInsensitive")).toBool(false);
	result.reversed = object.value(
		QStringLiteral("reversed")).toBool(false);
	result.enabled = object.value(QStringLiteral("enabled")).toBool(true);
	return result;
}

} // namespace

QList<Filter> ParseFilters(const QString &json) {
	auto result = QList<Filter>();
	auto error = QJsonParseError{ 0, QJsonParseError::NoError };
	const auto document = QJsonDocument::fromJson(json.toUtf8(), &error);
	if (error.error != QJsonParseError::NoError || !document.isArray()) {
		return result;
	}
	const auto array = document.array();
	result.reserve(array.size());
	for (const auto &item : array) {
		if (!item.isObject()) {
			continue;
		}
		const auto object = item.toObject();
		if (!object.value(QStringLiteral("regex")).isString()) {
			continue;
		}
		auto filter = ParseFilter(object);
		if (!BuildExpression(filter).isValid()) {
			continue;
		}
		result.push_back(std::move(filter));
	}
	return result;
}

QList<Compiled> CompileFilters(const QList<Filter> &filters) {
	auto result = QList<Compiled>();
	result.reserve(filters.size());
	for (const auto &filter : filters) {
		auto expression = BuildExpression(filter);
		if (!expression.isValid()) {
			continue;
		}
		auto compiled = Compiled();
		compiled.re = std::move(expression);
		compiled.reversed = filter.reversed;
		compiled.enabled = filter.enabled;
		result.push_back(std::move(compiled));
	}
	return result;
}

bool MatchesText(
		const QList<Compiled> &filters,
		const QString &text) {
	for (const auto &filter : filters) {
		if (!filter.enabled) {
			continue;
		}
		if (filter.re.match(text).hasMatch() != filter.reversed) {
			return true;
		}
	}
	return false;
}

void Matcher::setFilters(QList<Compiled> filters) {
	_filters = std::move(filters);
	++_generation;
	_cache.clear();
}

bool Matcher::match(qint64 msgId, const QString &text) const {
	if (_filters.isEmpty()) {
		return false;
	}
	if (const auto i = _cache.constFind(msgId); i != _cache.cend()) {
		return i.value();
	}
	const auto result = MatchesText(_filters, text);
	if (_cache.size() >= kMaxCacheEntries) {
		_cache.clear();
	}
	_cache.insert(msgId, result);
	return result;
}

void Matcher::invalidate() {
	_cache.clear();
}

quint64 Matcher::generation() const {
	return _generation;
}

} // namespace Exteraless
