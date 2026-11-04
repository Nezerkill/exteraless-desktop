#include "exteraless/links.h"

#include <QStringList>

namespace Exteraless {
namespace {

[[nodiscard]] bool IsTrackingKey(const QString &key) {
	const auto lower = key.toLower();
	if (lower.startsWith(QStringLiteral("utm_"))) {
		return true;
	}
	static const auto keys = QStringList{
		QStringLiteral("fbclid"),
		QStringLiteral("gclid"),
		QStringLiteral("dclid"),
		QStringLiteral("gbraid"),
		QStringLiteral("wbraid"),
		QStringLiteral("msclkid"),
		QStringLiteral("yclid"),
		QStringLiteral("twclid"),
		QStringLiteral("ttclid"),
		QStringLiteral("igshid"),
		QStringLiteral("li_fat_id"),
		QStringLiteral("mc_cid"),
		QStringLiteral("mc_eid"),
		QStringLiteral("_hsenc"),
		QStringLiteral("_hsmi"),
		QStringLiteral("mkt_tok"),
		QStringLiteral("rb_clickid"),
		QStringLiteral("s_cid"),
		QStringLiteral("vero_id"),
		QStringLiteral("wickedid"),
		QStringLiteral("oly_anon_id"),
		QStringLiteral("oly_enc_id"),
	};
	return keys.contains(lower);
}

} // namespace

QString StripTracking(const QString &url) {
	const auto colon = url.indexOf(u':');
	if (colon <= 0) {
		return url;
	}
	const auto scheme = url.mid(0, colon);
	if (scheme.compare(QStringLiteral("http"), Qt::CaseInsensitive) != 0
		&& scheme.compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0) {
		return url;
	}
	const auto start = colon + 1;
	if (url.mid(start, 2) != QStringLiteral("//")) {
		return url;
	}
	const auto authorityBegin = start + 2;
	auto authorityEnd = authorityBegin;
	while (authorityEnd != url.size()
		&& url[authorityEnd] != u'/'
		&& url[authorityEnd] != u'?'
		&& url[authorityEnd] != u'#') {
		++authorityEnd;
	}
	if (authorityEnd == authorityBegin) {
		return url;
	}
	const auto query = url.indexOf(u'?', authorityEnd);
	if (query < 0) {
		return url;
	}
	const auto fragment = url.indexOf(u'#', authorityEnd);
	if (fragment >= 0 && fragment < query) {
		return url;
	}
	const auto end = (fragment >= 0) ? fragment : url.size();
	const auto parts = url.mid(query + 1, end - query - 1)
		.split(u'&', Qt::KeepEmptyParts);
	auto kept = QStringList();
	auto keptNonEmpty = false;
	for (const auto &part : parts) {
		const auto equal = part.indexOf(u'=');
		const auto key = (equal >= 0) ? part.left(equal) : part;
		if (!IsTrackingKey(key)) {
			kept.push_back(part);
			keptNonEmpty = keptNonEmpty || !part.isEmpty();
		}
	}
	if (kept.size() == parts.size()) {
		return url;
	}
	auto result = url.left(query);
	if (keptNonEmpty) {
		result += u'?';
		result += kept.join(QStringLiteral("&"));
	}
	result += url.mid(end);
	return result;
}

} // namespace Exteraless
