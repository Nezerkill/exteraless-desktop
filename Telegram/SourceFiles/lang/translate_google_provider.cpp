/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "lang/translate_google_provider.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QJsonValue>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Ui {
namespace {

[[nodiscard]] QString UnescapeHtmlEntities(QString text) {
	text.replace(u"&quot;"_q, u"\""_q);
	text.replace(u"&#39;"_q, u"'"_q);
	text.replace(u"&lt;"_q, u"<"_q);
	text.replace(u"&gt;"_q, u">"_q);
	text.replace(u"&amp;"_q, u"&"_q);
	return text;
}

// WHY: the v2 API escapes entities in translatedText even with format=text,
// so the text is unescaped here and not where it would be re-escaped again.
[[nodiscard]] std::optional<QString> ParseTranslatedText(
		const QByteArray &body) {
	auto error = QJsonParseError();
	const auto parsed = QJsonDocument::fromJson(body, &error);
	if (error.error != QJsonParseError::NoError || !parsed.isObject()) {
		return std::nullopt;
	}
	const auto data = parsed.object().value(u"data"_q).toObject();
	const auto translations = data.value(u"translations"_q).toArray();
	if (translations.isEmpty()) {
		return std::nullopt;
	}
	auto result = QString();
	for (const auto &value : translations) {
		const auto translated = value.toObject()
			.value(u"translatedText"_q)
			.toString();
		if (translated.isEmpty()) {
			return std::nullopt;
		}
		result += UnescapeHtmlEntities(translated);
	}
	return result;
}

class GoogleTranslateProvider final : public TranslateProvider {
public:
	explicit GoogleTranslateProvider(QString key)
	: _key(std::move(key)) {
	}

	[[nodiscard]] bool supportsMessageId() const override {
		return false;
	}

	void request(
			TranslateProviderRequest request,
			LanguageId to,
			Fn<void(TranslateProviderResult)> done) override {
		const auto target = to.twoLetterCode();
		if (_key.isEmpty()
			|| request.text.text.isEmpty()
			|| target.isEmpty()) {
			done(TranslateProviderResult{
				.error = TranslateProviderError::Unknown,
			});
			return;
		}
		auto query = QByteArray("q=");
		query += QUrl::toPercentEncoding(request.text.text);
		query += "&target=" + QUrl::toPercentEncoding(target);
		query += "&format=text&key=" + QUrl::toPercentEncoding(_key);
		auto url = QUrl(
			u"https://translation.googleapis.com/language/translate/v2"_q);
		url.setQuery(QString::fromLatin1(query), QUrl::StrictMode);
		if (!url.isValid()) {
			done(TranslateProviderResult{
				.error = TranslateProviderError::Unknown,
			});
			return;
		}
		const auto reply = _network.get(QNetworkRequest(url));
		QObject::connect(reply, &QNetworkReply::finished, [=] {
			auto result = TranslateProviderResult();
			if (reply->error() != QNetworkReply::NoError) {
				result.error = TranslateProviderError::Unknown;
			} else if (const auto translated
					= ParseTranslatedText(reply->readAll())) {
				result.text = TextWithEntities{ *translated };
			} else {
				result.error = TranslateProviderError::Unknown;
			}
			done(std::move(result));
			reply->deleteLater();
		});
	}

private:
	const QString _key;
	QNetworkAccessManager _network;

};

} // namespace

std::unique_ptr<TranslateProvider> CreateGoogleTranslateProvider(
		QString key) {
	return std::make_unique<GoogleTranslateProvider>(std::move(key));
}

} // namespace Ui
