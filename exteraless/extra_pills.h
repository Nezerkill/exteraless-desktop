#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

namespace Exteraless {

struct FiatRates {
	bool ok = false;
	double eurToRub = 0.;
	double usdToRub = 0.;
};

struct WeatherCityInfo {
	QString label;
	double lat = 0.;
	double lon = 0.;
};

[[nodiscard]] QStringList WeatherCityIds();
[[nodiscard]] std::optional<WeatherCityInfo> WeatherCity(const QString &id);

[[nodiscard]] QString WeatherApiUrl();
[[nodiscard]] QString CoinbaseSpotUrl(const QString &code);
[[nodiscard]] QString EcbDailyUrl();
[[nodiscard]] QString CbrDailyUrl();
[[nodiscard]] QString GoldApiUrl();

[[nodiscard]] QString ParseWeather(const QByteArray &data);
[[nodiscard]] QString ParseCoinbase(const QByteArray &data);
[[nodiscard]] FiatRates ParseEcbXml(const QByteArray &data);
[[nodiscard]] FiatRates ParseCbrXml(const QByteArray &data);
[[nodiscard]] QString FormatFiatRate(
	const FiatRates &rates,
	const QString &code);
[[nodiscard]] QString ParseGoldApi(const QByteArray &data);

class ExtraPills final {
public:
	ExtraPills();
	~ExtraPills();

	ExtraPills(const ExtraPills &) = delete;
	ExtraPills &operator=(const ExtraPills &) = delete;

	[[nodiscard]] static ExtraPills &Instance();

	void Refresh();

	[[nodiscard]] QString WeatherText() const;
	[[nodiscard]] QString CryptoText(const QString &code) const;
	[[nodiscard]] QString FiatText(const QString &code) const;
	[[nodiscard]] QString GoldText() const;

private:
	struct State;
	std::unique_ptr<State> _state;
};

} // namespace Exteraless
