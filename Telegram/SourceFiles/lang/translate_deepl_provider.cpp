/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "lang/translate_deepl_provider.h"

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

[[nodiscard]] QString FormalityValue(int formality) {
	switch (formality) {
	case 1: return u"more"_q;
	case 2: return u"less"_q;
	}
	return QString();
}

[[nodiscard]] std::optional<QString> ParseTranslatedText(
		const QByteArray &body) {
	auto error = QJsonParseError();
	const auto parsed = QJsonDocument::fromJson(body, &error);
	if (error.error != QJsonParseError::NoError || !parsed.isObject()) {
		return std::nullopt;
	}
	const auto translations = parsed.object()
		.value(u"translations"_q)
		.toArray();
	if (translations.isEmpty()) {
		return std::nullopt;
	}
	auto result = QString();
	for (const auto &value : translations) {
		const auto translated = value.toObject().value(u"text"_q).toString();
		if (translated.isEmpty()) {
			return std::nullopt;
		}
		result += translated;
	}
	return result;
}

class DeepLTranslateProvider final : public TranslateProvider {
public:
	DeepLTranslateProvider(QString key, int formality)
	: _key(std::move(key))
	, _formality(FormalityValue(formality)) {
	}

	[[nodiscard]] bool supportsMessageId() const override {
		return false;
	}

	void request(
			TranslateProviderRequest request,
			LanguageId to,
			Fn<void(TranslateProviderResult)> done) override {
		const auto target = to.twoLetterCode().toUpper();
		if (_key.isEmpty()
			|| request.text.text.isEmpty()
			|| target.isEmpty()) {
			done(TranslateProviderResult{
				.error = TranslateProviderError::Unknown,
			});
			return;
		}
		auto body = QByteArray("text=");
		body += QUrl::toPercentEncoding(request.text.text);
		body += "&target_lang=" + QUrl::toPercentEncoding(target);
		if (!_formality.isEmpty()) {
			body += "&formality=" + QUrl::toPercentEncoding(_formality);
		}
		auto networkRequest = QNetworkRequest(
			QUrl(u"https://api-free.deepl.com/v2/translate"_q));
		networkRequest.setHeader(
			QNetworkRequest::ContentTypeHeader,
			u"application/x-www-form-urlencoded"_q);
		networkRequest.setRawHeader(
			"Authorization",
			QByteArray("DeepL-Auth-Key ") + _key.toUtf8());
		const auto reply = _network.post(networkRequest, body);
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
	const QString _formality;
	QNetworkAccessManager _network;

};

} // namespace

std::unique_ptr<TranslateProvider> CreateDeepLTranslateProvider(
		QString key,
		int formality) {
	return std::make_unique<DeepLTranslateProvider>(
		std::move(key),
		formality);
}

} // namespace Ui
