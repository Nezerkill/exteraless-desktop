/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/ai_request_box.h"

#include "exteraless/ai_client.h"
#include "exteraless/config.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"

#include <QtCore/QUrl>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <rpl/rpl.h>

namespace Ui {

void AiRequestBox(not_null<GenericBox*> box, QString text) {
	box->setWidth(st::boxWideWidth);
	box->setTitle(u"Ask AI"_q);

	struct State {
		QNetworkAccessManager network;
		rpl::variable<QString> response;
		bool busy = false;
	};
	const auto state = box->lifetime().make_state<State>();

	const auto layout = box->verticalLayout();
	Ui::AddSkip(layout);

	const auto field = box->addRow(object_ptr<InputField>(
		box,
		st::defaultInputField,
		InputField::Mode::MultiLine,
		rpl::single(u"Ask about this message"_q),
		std::move(text)));
	const auto lineHeight = st::defaultInputField.style.font->height;
	field->setMinHeight(lineHeight * 3);
	field->setMaxHeight(lineHeight * 8);

	Ui::AddSkip(layout);
	layout->add(
		object_ptr<FlatLabel>(box, state->response.value(), st::boxLabel),
		st::boxRowPadding);

	const auto send = [=] {
		if (state->busy) {
			return;
		}
		// WHY: the key is read on every send, so a key saved from Settings
		// while this box is open is used without reopening the box.
		const auto &config = Exteraless::Config::Instance();
		const auto key = config.aiApiKey().trimmed();
		if (key.isEmpty()) {
			box->showToast(u"Set API key in Exteraless settings"_q);
			return;
		}
		const auto prompt = field->getLastText().trimmed();
		if (prompt.isEmpty()) {
			box->showToast(u"Enter your request"_q);
			return;
		}
		state->busy = true;
		state->response = u"Thinking..."_q;

		auto networkRequest = QNetworkRequest(
			QUrl(Exteraless::ChatCompletionsUrl(config.aiEndpoint())));
		networkRequest.setHeader(
			QNetworkRequest::ContentTypeHeader,
			u"application/json"_q);
		networkRequest.setRawHeader(
			"Authorization",
			(u"Bearer " + key).toUtf8());
		const auto reply = state->network.post(
			networkRequest,
			Exteraless::BuildRequestJson(
				config.aiEndpoint(),
				config.aiModel(),
				Exteraless::AiRequest{ .prompt = prompt }));
		QObject::connect(reply, &QNetworkReply::finished, [=] {
			const auto body = reply->readAll();
			const auto failure = reply->errorString();
			const auto networkError
				= (reply->error() != QNetworkReply::NoError);
			reply->deleteLater();
			state->busy = false;
			auto result = Exteraless::ParseResponse(body);
			if (result.ok) {
				state->response = result.text;
			} else {
				state->response = result.error.isEmpty()
					? (networkError
						? failure
						: u"Request failed"_q)
					: result.error;
			}
		});
	};

	box->addButton(rpl::single(u"Send"_q), send);
	box->addLeftButton(rpl::single(u"Copy"_q), [=] {
		const auto answer = state->response.current();
		if (!answer.isEmpty()) {
			QGuiApplication::clipboard()->setText(answer);
		}
	});
	box->addLeftButton(rpl::single(u"Close"_q), [=] {
		box->closeBox();
	});
	box->setShowFinishedCallback([=] {
		field->setFocusFast();
	});
}

} // namespace Ui
