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
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QPointer>
#include <QtCore/QUrl>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <rpl/rpl.h>

#include <memory>
#include <vector>

namespace {

constexpr auto kMaxTurns = 20;

[[nodiscard]] std::vector<Exteraless::AiRole> AvailableRoles() {
	return Exteraless::ParseAiRoles(
		Exteraless::Config::Instance().aiRoles());
}

[[nodiscard]] QString RoleTitle(const Exteraless::AiRole &role) {
	return role.title;
}

[[nodiscard]] QString RoleSystem(const Exteraless::AiRole &role) {
	return role.system;
}

[[nodiscard]] QString ActiveRole() {
	return Exteraless::Config::Instance().aiActiveRole();
}

void SetActiveRole(const QString &value) {
	Exteraless::Config::Instance().setAiActiveRole(value);
}

[[nodiscard]] bool HistoryEnabled() {
	return Exteraless::Config::Instance().aiHistory();
}

[[nodiscard]] QString ActiveRoleSystem(
		const std::vector<Exteraless::AiRole> &roles,
		const QString &active) {
	for (const auto &role : roles) {
		if (RoleTitle(role) == active) {
			return RoleSystem(role);
		}
	}
	return QString();
}

} // namespace

namespace Ui {

void AiRequestBox(not_null<GenericBox*> box, QString text) {
	box->setWidth(st::boxWideWidth);
	box->setTitle(u"Ask AI"_q);

	struct State {
		QNetworkAccessManager network;
		Exteraless::SseParser parser;
		QPointer<RoundButton> sendButton;
		QString answer;
		QByteArray raw;
		QList<QPair<QString, QString>> turns;
		rpl::variable<QString> response;
		QNetworkReply *reply = nullptr;
		bool busy = false;
		bool closing = false;

		~State() {
			cancel();
		}

		void cancel() {
			closing = true;
			parser.reset();
			const auto pending = reply;
			reply = nullptr;
			if (pending) {
				pending->abort();
				pending->deleteLater();
			}
		}
	};
	const auto state = box->lifetime().make_state<State>();

	const auto layout = box->verticalLayout();
	Ui::AddSkip(layout);

	const auto roles = AvailableRoles();
	const auto activeRole = ActiveRole();
	if (!roles.empty()) {
		auto initial = int(roles.size());
		for (auto i = 0; i != int(roles.size()); ++i) {
			if (RoleTitle(roles[i]) == activeRole) {
				initial = i;
				break;
			}
		}
		const auto group = std::make_shared<Ui::RadiobuttonGroup>(initial);
		for (auto i = 0; i != int(roles.size()); ++i) {
			box->addRow(object_ptr<Ui::Radiobutton>(
				box,
				group,
				i,
				RoleTitle(roles[i]),
				st::defaultBoxCheckbox), st::boxRowPadding);
		}
		box->addRow(object_ptr<Ui::Radiobutton>(
			box,
			group,
			int(roles.size()),
			u"No role"_q,
			st::defaultBoxCheckbox), st::boxRowPadding);
		group->setChangedCallback([=](int value) {
			SetActiveRole((value < int(roles.size()))
				? RoleTitle(roles[value])
				: QString());
		});
		Ui::AddSkip(layout);
	}

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
		state->answer.clear();
		state->raw.clear();
		state->parser.reset();
		state->response = u"Thinking..."_q;
		if (const auto button = state->sendButton) {
			button->setEnabled(false);
		}

		auto messages = QJsonArray();
		const auto system = ActiveRoleSystem(AvailableRoles(), ActiveRole());
		if (!system.isEmpty()) {
			messages.append(QJsonObject{
				{ u"role"_q, u"system"_q },
				{ u"content"_q, system },
			});
		}
		if (HistoryEnabled()) {
			for (const auto &turn : state->turns) {
				messages.append(QJsonObject{
					{ u"role"_q, u"user"_q },
					{ u"content"_q, turn.first },
				});
				messages.append(QJsonObject{
					{ u"role"_q, u"assistant"_q },
					{ u"content"_q, turn.second },
				});
			}
		}
		messages.append(QJsonObject{
			{ u"role"_q, u"user"_q },
			{ u"content"_q, prompt },
		});
		const auto payload = QJsonDocument(QJsonObject{
			{ u"model"_q, config.aiModel() },
			{ u"messages"_q, messages },
			{ u"stream"_q, true },
		}).toJson(QJsonDocument::Compact);

		auto networkRequest = QNetworkRequest(
			QUrl(Exteraless::ChatCompletionsUrl(config.aiEndpoint())));
		networkRequest.setHeader(
			QNetworkRequest::ContentTypeHeader,
			u"application/json"_q);
		networkRequest.setRawHeader(
			"Authorization",
			(u"Bearer " + key).toUtf8());
		const auto reply = state->network.post(networkRequest, payload);
		state->reply = reply;
		QObject::connect(reply, &QNetworkReply::readyRead, box, [=] {
			if (state->closing) {
				return;
			}
			const auto chunk = reply->readAll();
			if (state->answer.isEmpty()) {
				state->raw += chunk;
			}
			const auto delta = state->parser.feed(chunk);
			if (!delta.isEmpty()) {
				state->answer += delta;
				state->response = state->answer;
			}
			if (state->parser.isDone()) {
				reply->abort();
			}
		});
		QObject::connect(reply, &QNetworkReply::finished, box, [=] {
			if (state->closing) {
				return;
			}
			const auto failure = reply->errorString();
			const auto networkError
				= (reply->error() != QNetworkReply::NoError);
			const auto done = state->parser.isDone();
			reply->deleteLater();
			state->reply = nullptr;
			state->busy = false;
			state->parser.reset();
			if (const auto button = state->sendButton) {
				button->setEnabled(true);
			}
			if (!state->answer.isEmpty() && (done || !networkError)) {
				state->response = state->answer;
				if (HistoryEnabled()) {
					state->turns.append({ prompt, state->answer });
					while (state->turns.size() > kMaxTurns) {
						state->turns.removeFirst();
					}
				}
			} else if (!state->answer.isEmpty()) {
				state->response = failure;
			} else if (done) {
				state->response = u"Empty response"_q;
			} else {
				const auto result = Exteraless::ParseResponse(state->raw);
				state->response = result.ok
					? result.text
					: (result.error.isEmpty()
						? (networkError
							? failure
							: u"Request failed"_q)
						: result.error);
			}
		});
	};

	state->sendButton = box->addButton(rpl::single(u"Send"_q), send);
	box->addLeftButton(rpl::single(u"Copy"_q), [=] {
		const auto answer = state->response.current();
		if (!answer.isEmpty()) {
			QGuiApplication::clipboard()->setText(answer);
		}
	});
	box->addLeftButton(rpl::single(u"Clear"_q), [=] {
		state->turns.clear();
	});
	box->addLeftButton(rpl::single(u"Close"_q), [=] {
		state->cancel();
		box->closeBox();
	});
	box->setShowFinishedCallback([=] {
		field->setFocusFast();
	});
}

} // namespace Ui
