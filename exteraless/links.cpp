#include "exteraless/links.h"

#include <QRegularExpression>
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

QString StripTrackingInText(const QString &text) {
	static const auto expression = QRegularExpression(
		QStringLiteral("https?://[^\\s<>\"']+"),
		QRegularExpression::CaseInsensitiveOption);
	const auto normalized = QString(text).replace(
		QStringLiteral("\r\n"),
		QStringLiteral("\n"));
	auto last = 0;
	auto output = QString();
	auto match = expression.globalMatch(normalized);
	while (match.hasNext()) {
		const auto found = match.next();
		auto url = found.captured();
		auto suffix = QString();
		static const auto trailing = QString::fromUtf8(
			".,;:!?)]}\x27\x22`*»”’。…—");
		while (!url.isEmpty() && trailing.contains(url.back())) {
			suffix.prepend(url.back());
			url.chop(1);
		}
		const auto cleaned = StripTracking(url);
		if (cleaned == url) {
			continue;
		}
		output += normalized.mid(
			last,
			found.capturedStart() - last);
		output += cleaned;
		output += suffix;
		last = found.capturedEnd();
	}
	if (!last) {
		return text;
	}
	output += normalized.mid(last);
	return output;
}

bool IsWin32Executable(const QString &filepath) {
	const auto dot = filepath.lastIndexOf(u'.');
	if (dot < 0) {
		return false;
	}
	const auto extension = filepath.mid(dot + 1).toLower();
	static const auto kWin32Executables = QStringList{
		QStringLiteral("exe"),
		QStringLiteral("msi"),
		QStringLiteral("bat"),
		QStringLiteral("cmd"),
		QStringLiteral("com"),
		QStringLiteral("scr"),
		QStringLiteral("ps1"),
		QStringLiteral("vbs"),
		QStringLiteral("js"),
		QStringLiteral("jar"),
		QStringLiteral("lnk"),
		QStringLiteral("reg"),
		QStringLiteral("msc"),
		QStringLiteral("cpl"),
		QStringLiteral("gadget"),
		QStringLiteral("hta"),
		QStringLiteral("wsf"),
		QStringLiteral("wsh"),
	};
	return kWin32Executables.contains(extension);
}

} // namespace Exteraless
