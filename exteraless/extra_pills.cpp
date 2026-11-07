#include "exteraless/extra_pills.h"

#include "exteraless/config.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QXmlStreamReader>

#include <array>
#include <cmath>
#include <functional>
#include <utility>

namespace Exteraless {
namespace {

struct CityRow {
	const char *id;
	const char *label;
	double lat;
	double lon;
};

constexpr auto kWeatherCities = std::array<CityRow, 12>{ {
	{ "moscow", "Moscow", 55.75, 37.61 },
	{ "london", "London", 51.51, -0.13 },
	{ "new-york", "New York", 40.71, -74.01 },
	{ "tokyo", "Tokyo", 35.68, 139.69 },
	{ "berlin", "Berlin", 52.52, 13.41 },
	{ "paris", "Paris", 48.86, 2.35 },
	{ "beijing", "Beijing", 39.90, 116.41 },
	{ "delhi", "Delhi", 28.61, 77.21 },
	{ "istanbul", "Istanbul", 41.01, 28.98 },
	{ "kyiv", "Kyiv", 50.45, 30.52 },
	{ "warsaw", "Warsaw", 52.23, 21.01 },
	{ "astana", "Astana", 51.17, 71.45 },
} };

constexpr qint64 kRefreshIntervalMs = 30 * 60 * 1000;

enum class Slot {
	Weather,
	Btc,
	Eth,
	Usd,
	Eur,
	Gold,
	Count,
};

constexpr auto kSlotCount = int(Slot::Count);

constexpr auto kCacheSlots = std::array<Slot, kSlotCount>{ {
	Slot::Weather,
	Slot::Btc,
	Slot::Eth,
	Slot::Usd,
	Slot::Eur,
	Slot::Gold,
} };

QString &CachePathOverride() {
	static auto value = QString();
	return value;
}

[[nodiscard]] const char *SlotKey(Slot slot) {
	switch (slot) {
	case Slot::Weather: return "weather";
	case Slot::Btc: return "btc";
	case Slot::Eth: return "eth";
	case Slot::Usd: return "usd";
	case Slot::Eur: return "eur";
	case Slot::Gold: return "gold";
	case Slot::Count: break;
	}
	return "";
}

[[nodiscard]] QString TrimZeros(QString text) {
	while (text.endsWith(u'0')) {
		text.chop(1);
	}
	if (text.endsWith(u'.')) {
		text.chop(1);
	}
	return text;
}

[[nodiscard]] QString FormatAmount(double value) {
	if (!std::isfinite(value) || value <= 0.) {
		return QString();
	}
	if (value >= 1000.) {
		return QString::number(std::llround(value));
	}
	return TrimZeros(QString::number(value, 'f', 2));
}

[[nodiscard]] bool ParseJsonObject(
		const QByteArray &data,
		QJsonObject *object) {
	auto error = QJsonParseError{ 0, QJsonParseError::NoError };
	const auto document = QJsonDocument::fromJson(data, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return false;
	}
	*object = document.object();
	return true;
}

[[nodiscard]] QString DashText() {
	return QString(QChar(0x2014));
}

[[nodiscard]] QString WithDash(const QString &text) {
	return text.isEmpty() ? DashText() : text;
}

// CBR writes decimal fractions with a comma, unlike the JSON sources.
[[nodiscard]] bool ParseCommaDecimal(const QString &text, double *value) {
	auto normalized = text.trimmed();
	normalized.replace(u',', u'.');
	auto ok = false;
	const auto parsed = normalized.toDouble(&ok);
	if (!ok || !std::isfinite(parsed)) {
		return false;
	}
	*value = parsed;
	return true;
}

[[nodiscard]] const CityRow *FindCity(const QString &id) {
	const auto key = id.trimmed().toLower();
	for (const auto &row : kWeatherCities) {
		if (key == QLatin1String(row.id)) {
			return &row;
		}
	}
	return nullptr;
}

[[nodiscard]] QString BuildWeatherUrl(const CityRow &city) {
	return QStringLiteral(
		"https://api.open-meteo.com/v1/forecast"
		"?latitude=%1&longitude=%2&current=temperature_2m")
		.arg(QString::number(city.lat, 'f', 2))
		.arg(QString::number(city.lon, 'f', 2));
}

} // namespace

QString ExtraPillsCachePath() {
	if (!CachePathOverride().isEmpty()) {
		return CachePathOverride();
	}
	// Same tdata directory as exteraless.json in Config::ensureLoaded().
	return QStandardPaths::writableLocation(
		QStandardPaths::AppLocalDataLocation)
		+ QStringLiteral("/tdata/extra-pills-cache.json");
}

void SetExtraPillsCachePath(const QString &path) {
	CachePathOverride() = path;
}

QStringList WeatherCityIds() {
	auto result = QStringList();
	for (const auto &row : kWeatherCities) {
		result.push_back(QString::fromLatin1(row.id));
	}
	return result;
}

std::optional<WeatherCityInfo> WeatherCity(const QString &id) {
	const auto row = FindCity(id);
	if (!row) {
		return std::nullopt;
	}
	return WeatherCityInfo{
		QString::fromLatin1(row->label),
		row->lat,
		row->lon,
	};
}

QString WeatherApiUrl() {
	const auto city = FindCity(Config::Instance().networkWeatherCity());
	return BuildWeatherUrl(city ? *city : kWeatherCities.front());
}

QString CoinbaseSpotUrl(const QString &code) {
	const auto upper = code.trimmed().toUpper();
	if (upper.isEmpty()) {
		return QString();
	}
	for (const auto ch : upper) {
		const auto value = ch.unicode();
		if ((value < u'A' || value > u'Z')
			&& (value < u'0' || value > u'9')) {
			return QString();
		}
	}
	return QStringLiteral(
		"https://api.coinbase.com/v2/prices/%1-USD/spot").arg(upper);
}

QString EcbDailyUrl() {
	return QStringLiteral(
		"https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml");
}

QString CbrDailyUrl() {
	return QStringLiteral("https://www.cbr.ru/scripts/XML_daily.asp");
}

QString GoldApiUrl() {
	return QStringLiteral("https://api.gold-api.com/price/XAU");
}

QString ParseWeather(const QByteArray &data) {
	auto object = QJsonObject();
	if (!ParseJsonObject(data, &object)) {
		return QString();
	}
	const auto current = object.value(QStringLiteral("current"));
	if (!current.isObject()) {
		return QString();
	}
	const auto temperature = current.toObject().value(
		QStringLiteral("temperature_2m"));
	if (!temperature.isDouble()) {
		return QString();
	}
	const auto value = temperature.toDouble();
	if (!std::isfinite(value) || value < -99. || value > 99.) {
		return QString();
	}
	const auto rounded = int(std::lround(value));
	if (rounded > 0) {
		return QStringLiteral("+") + QString::number(rounded) + QChar(0x00B0);
	}
	return QString::number(rounded) + QChar(0x00B0);
}

QString ParseCoinbase(const QByteArray &data) {
	auto object = QJsonObject();
	if (!ParseJsonObject(data, &object)) {
		return QString();
	}
	const auto payload = object.value(QStringLiteral("data"));
	if (!payload.isObject()) {
		return QString();
	}
	const auto amount = payload.toObject().value(QStringLiteral("amount"));
	if (!amount.isString()) {
		return QString();
	}
	auto ok = false;
	const auto value = amount.toString().toDouble(&ok);
	return ok ? FormatAmount(value) : QString();
}

FiatRates ParseEcbXml(const QByteArray &data) {
	auto rates = FiatRates();
	auto reader = QXmlStreamReader(data);
	auto usdPerEur = 0.;
	auto rubPerEur = 0.;
	auto hasUsd = false;
	auto hasRub = false;
	while (!reader.atEnd()) {
		if (reader.readNext() != QXmlStreamReader::StartElement) {
			continue;
		}
		if (reader.name() != QLatin1String("Cube")) {
			continue;
		}
		const auto attributes = reader.attributes();
		const auto currency = attributes.value(QLatin1String("currency"));
		const auto rate = attributes.value(QLatin1String("rate"));
		if (currency.isEmpty() || rate.isEmpty()) {
			continue;
		}
		auto ok = false;
		const auto value = rate.toDouble(&ok);
		if (!ok || !std::isfinite(value) || value <= 0.) {
			continue;
		}
		const auto code = currency.toString().toUpper();
		if (code == QStringLiteral("USD")) {
			usdPerEur = value;
			hasUsd = true;
		} else if (code == QStringLiteral("RUB")) {
			rubPerEur = value;
			hasRub = true;
		}
	}
	if (reader.hasError() || !hasUsd || !hasRub || usdPerEur <= 0.) {
		return rates;
	}
	rates.ok = true;
	rates.eurToRub = rubPerEur;
	rates.usdToRub = rubPerEur / usdPerEur;
	return rates;
}

FiatRates ParseCbrXml(const QByteArray &data) {
	auto rates = FiatRates();
	// Only these ASCII fields matter, so skip the charset CBR declares.
	auto reader = QXmlStreamReader(QString::fromLatin1(data));
	auto usdToRub = 0.;
	auto eurToRub = 0.;
	auto hasUsd = false;
	auto hasEur = false;
	while (!reader.atEnd()) {
		if (reader.readNext() != QXmlStreamReader::StartElement) {
			continue;
		}
		if (reader.name() != QLatin1String("Valute")) {
			continue;
		}
		auto code = QString();
		auto nominal = 0.;
		auto value = 0.;
		auto hasNominal = false;
		auto hasValue = false;
		while (reader.readNextStartElement()) {
			const auto name = reader.name();
			const auto text = reader.readElementText();
			if (name == QLatin1String("CharCode")) {
				code = text.trimmed().toUpper();
			} else if (name == QLatin1String("Nominal")) {
				hasNominal = ParseCommaDecimal(text, &nominal);
			} else if (name == QLatin1String("Value")) {
				hasValue = ParseCommaDecimal(text, &value);
			}
		}
		if (!hasNominal || !hasValue || nominal <= 0. || value <= 0.) {
			continue;
		}
		const auto perUnit = value / nominal;
		if (code == QStringLiteral("USD")) {
			usdToRub = perUnit;
			hasUsd = true;
		} else if (code == QStringLiteral("EUR")) {
			eurToRub = perUnit;
			hasEur = true;
		}
	}
	if (reader.hasError() || !hasUsd || !hasEur) {
		return rates;
	}
	rates.ok = true;
	rates.usdToRub = usdToRub;
	rates.eurToRub = eurToRub;
	return rates;
}

QString FormatFiatRate(const FiatRates &rates, const QString &code) {
	if (!rates.ok) {
		return QString();
	}
	const auto upper = code.trimmed().toUpper();
	auto value = 0.;
	if (upper == QStringLiteral("USD")) {
		value = rates.usdToRub;
	} else if (upper == QStringLiteral("EUR")) {
		value = rates.eurToRub;
	} else {
		return QString();
	}
	if (!std::isfinite(value) || value <= 0.) {
		return QString();
	}
	return TrimZeros(QString::number(value, 'f', 2));
}

QString ParseGoldApi(const QByteArray &data) {
	auto object = QJsonObject();
	if (!ParseJsonObject(data, &object)) {
		return QString();
	}
	const auto price = object.value(QStringLiteral("price"));
	if (!price.isDouble()) {
		return QString();
	}
	return FormatAmount(price.toDouble());
}

struct ExtraPills::State final {
	struct Entry {
		QString text;
		QElapsedTimer since;
		qint64 at = 0;
		bool pending = false;
	};

	struct RatesEntry {
		QElapsedTimer since;
		bool pending = false;
		FiatRates rates;
	};

	QString cachePath;
	QNetworkAccessManager manager;
	std::array<Entry, kSlotCount> entries = {};
	RatesEntry cbr;
	RatesEntry ecb;

	void refreshAll();
	void refreshFiat();
	void refreshRates(
		RatesEntry &entry,
		const QString &url,
		FiatRates (*parse)(const QByteArray &));
	void updateFiatTexts();
	void fetch(
		Entry &entry,
		const QString &url,
		std::function<void(const QByteArray &)> apply);
	bool saveCache(const QString &path) const;
	bool loadCache(const QString &path);

	[[nodiscard]] bool fresh(const QElapsedTimer &since) const;
	[[nodiscard]] bool cachedFresh(const Entry &entry) const;
	[[nodiscard]] int cachedEntries() const;
	[[nodiscard]] QString text(Slot slot) const;
};

bool ExtraPills::State::fresh(const QElapsedTimer &since) const {
	return since.isValid() && since.elapsed() < kRefreshIntervalMs;
}

bool ExtraPills::State::cachedFresh(const Entry &entry) const {
	return entry.at > 0
		&& (QDateTime::currentMSecsSinceEpoch() - entry.at
			< kRefreshIntervalMs);
}

int ExtraPills::State::cachedEntries() const {
	auto count = 0;
	for (const auto &entry : entries) {
		count += entry.text.isEmpty() ? 0 : 1;
	}
	return count;
}

bool ExtraPills::State::loadCache(const QString &path) {
	if (path.isEmpty()) {
		return false;
	}
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}
	auto error = QJsonParseError{ 0, QJsonParseError::NoError };
	const auto document = QJsonDocument::fromJson(file.readAll(), &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return false;
	}
	const auto root = document.object();
	auto loaded = 0;
	for (const auto slot : kCacheSlots) {
		const auto value = root.value(QLatin1String(SlotKey(slot)));
		if (!value.isObject()) {
			continue;
		}
		const auto object = value.toObject();
		const auto text = object.value(QStringLiteral("text")).toString();
		const auto at = qint64(object.value(QStringLiteral("at")).toDouble());
		if (text.isEmpty() || at <= 0) {
			continue;
		}
		// WHY: a stale value still beats a dash, so age is deliberately
		// not part of this filter; only the refetch gate checks it.
		auto &target = entries[int(slot)];
		target.text = text;
		target.at = at;
		++loaded;
	}
	return loaded > 0;
}

bool ExtraPills::State::saveCache(const QString &path) const {
	if (path.isEmpty()) {
		return false;
	}
	auto root = QJsonObject();
	for (const auto slot : kCacheSlots) {
		const auto &entry = entries[int(slot)];
		if (entry.text.isEmpty() || entry.at <= 0) {
			continue;
		}
		root.insert(QLatin1String(SlotKey(slot)), QJsonObject{
			{ QStringLiteral("text"), entry.text },
			{ QStringLiteral("at"), entry.at },
		});
	}
	if (root.isEmpty()) {
		return false;
	}
	if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
		return false;
	}
	auto file = QSaveFile(path);
	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}
	file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
	return file.commit();
}

QString ExtraPills::State::text(Slot slot) const {
	return entries[int(slot)].text;
}

void ExtraPills::State::fetch(
		Entry &entry,
		const QString &url,
		std::function<void(const QByteArray &)> apply) {
	if (entry.pending || url.isEmpty()) {
		return;
	}
	entry.pending = true;
	entry.since.start();
	auto request = QNetworkRequest(QUrl(url));
	request.setRawHeader("User-Agent", "ExteralessDesktop");
	const auto reply = manager.get(request);
	QObject::connect(
		reply,
		&QNetworkReply::finished,
		&manager,
		[this, reply, &entry, apply = std::move(apply)] {
			reply->deleteLater();
			entry.pending = false;
			const auto bytes = reply->readAll();
			if (reply->error() != QNetworkReply::NoError) {
				return;
			}
			apply(bytes);
			if (!entry.text.isEmpty()) {
				entry.at = QDateTime::currentMSecsSinceEpoch();
				saveCache(cachePath);
			}
		});
}

void ExtraPills::State::refreshRates(
		RatesEntry &entry,
		const QString &url,
		FiatRates (*parse)(const QByteArray &)) {
	if (entry.pending || url.isEmpty() || fresh(entry.since)) {
		return;
	}
	entry.pending = true;
	entry.since.start();
	auto request = QNetworkRequest(QUrl(url));
	request.setRawHeader("User-Agent", "ExteralessDesktop");
	const auto reply = manager.get(request);
	QObject::connect(
		reply,
		&QNetworkReply::finished,
		&manager,
		[this, reply, &entry, parse] {
			reply->deleteLater();
			entry.pending = false;
			if (reply->error() != QNetworkReply::NoError) {
				return;
			}
			entry.rates = parse(reply->readAll());
			updateFiatTexts();
			saveCache(cachePath);
		});
}

void ExtraPills::State::updateFiatTexts() {
	// CBR quotes the rouble directly; the ECB cross is the fallback.
	const auto &rates = cbr.rates.ok ? cbr.rates : ecb.rates;
	auto &usd = entries[int(Slot::Usd)];
	auto &eur = entries[int(Slot::Eur)];
	usd.text = FormatFiatRate(rates, QStringLiteral("USD"));
	eur.text = FormatFiatRate(rates, QStringLiteral("EUR"));
	if (!usd.text.isEmpty() || !eur.text.isEmpty()) {
		const auto at = QDateTime::currentMSecsSinceEpoch();
		usd.at = at;
		eur.at = at;
	}
}

void ExtraPills::State::refreshFiat() {
	if (cachedFresh(entries[int(Slot::Usd)])
		|| cachedFresh(entries[int(Slot::Eur)])) {
		return;
	}
	refreshRates(cbr, CbrDailyUrl(), ParseCbrXml);
	refreshRates(ecb, EcbDailyUrl(), ParseEcbXml);
}

void ExtraPills::State::refreshAll() {
	auto &weather = entries[int(Slot::Weather)];
	if (!fresh(weather.since) && !cachedFresh(weather)) {
		fetch(weather, WeatherApiUrl(), [this](const QByteArray &data) {
			entries[int(Slot::Weather)].text = ParseWeather(data);
		});
	}
	auto &btc = entries[int(Slot::Btc)];
	if (!fresh(btc.since) && !cachedFresh(btc)) {
		fetch(
			btc,
			CoinbaseSpotUrl(QStringLiteral("BTC")),
			[this](const QByteArray &data) {
				entries[int(Slot::Btc)].text = ParseCoinbase(data);
			});
	}
	auto &eth = entries[int(Slot::Eth)];
	if (!fresh(eth.since) && !cachedFresh(eth)) {
		fetch(
			eth,
			CoinbaseSpotUrl(QStringLiteral("ETH")),
			[this](const QByteArray &data) {
				entries[int(Slot::Eth)].text = ParseCoinbase(data);
			});
	}
	refreshFiat();
	auto &gold = entries[int(Slot::Gold)];
	if (!fresh(gold.since) && !cachedFresh(gold)) {
		fetch(gold, GoldApiUrl(), [this](const QByteArray &data) {
			entries[int(Slot::Gold)].text = ParseGoldApi(data);
		});
	}
}

ExtraPills::ExtraPills()
: _state(std::make_unique<State>()) {
	_state->cachePath = ExtraPillsCachePath();
	_state->loadCache(_state->cachePath);
}

ExtraPills::~ExtraPills() = default;

ExtraPills &ExtraPills::Instance() {
	static auto instance = ExtraPills();
	return instance;
}

void ExtraPills::Refresh() {
	_state->refreshAll();
}

QString ExtraPills::CachePath() const {
	return _state->cachePath;
}

int ExtraPills::CachedEntries() const {
	return _state->cachedEntries();
}

bool ExtraPills::LoadCache(const QString &path) {
	return _state->loadCache(path);
}

bool ExtraPills::SaveCache(const QString &path) const {
	return _state->saveCache(path);
}

QString ExtraPills::WeatherText() const {
	return WithDash(_state->text(Slot::Weather));
}

QString ExtraPills::CryptoText(const QString &code) const {
	const auto upper = code.trimmed().toUpper();
	auto slot = Slot::Count;
	if (upper == QStringLiteral("BTC")) {
		slot = Slot::Btc;
	} else if (upper == QStringLiteral("ETH")) {
		slot = Slot::Eth;
	} else {
		return QString();
	}
	const auto text = _state->text(slot);
	return text.isEmpty()
		? DashText()
		: (QStringLiteral("$") + text);
}

QString ExtraPills::FiatText(const QString &code) const {
	const auto upper = code.trimmed().toUpper();
	if (upper == QStringLiteral("USD")) {
		return WithDash(_state->text(Slot::Usd));
	} else if (upper == QStringLiteral("EUR")) {
		return WithDash(_state->text(Slot::Eur));
	}
	return QString();
}

QString ExtraPills::GoldText() const {
	const auto text = _state->text(Slot::Gold);
	return text.isEmpty()
		? DashText()
		: (QStringLiteral("$") + text);
}

} // namespace Exteraless
