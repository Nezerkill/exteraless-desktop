#pragma once

#include <QHash>
#include <QList>
#include <QRegularExpression>
#include <QString>

namespace Exteraless {

struct Filter {
	QString regex;
	bool caseInsensitive = false;
	bool reversed = false;
	bool enabled = true;
};

struct Compiled {
	QRegularExpression re;
	bool reversed = false;
	bool enabled = false;
};

[[nodiscard]] QList<Filter> ParseFilters(const QString &json);
[[nodiscard]] QList<Compiled> CompileFilters(const QList<Filter> &filters);
[[nodiscard]] bool MatchesText(
	const QList<Compiled> &filters,
	const QString &text);

class Matcher final {
public:
	Matcher() = default;

	void setFilters(QList<Compiled> filters);
	[[nodiscard]] bool match(qint64 msgId, const QString &text) const;
	void invalidate();
	[[nodiscard]] quint64 generation() const;

private:
	QList<Compiled> _filters;
	mutable QHash<qint64, bool> _cache;
	mutable quint64 _generation = 0;

};

} // namespace Exteraless
