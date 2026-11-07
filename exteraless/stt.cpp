#include "exteraless/stt.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QStringList>

#include <sherpa-onnx/c-api/c-api.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>
#include <vector>

namespace Exteraless::Speech {
namespace {

constexpr int kSampleRate = 16000;
constexpr int kFeatureDim = 80;
constexpr int kPcmRate = 48000;

[[nodiscard]] Result Failure(const QString &error) {
	return { false, QString(), error };
}

[[nodiscard]] QString PickModel(
		const QStringList &names,
		const QString &needle,
		bool prefix = false) {
	auto fallback = QString();
	for (const auto &name : names) {
		const auto lower = name.toLower();
		const auto matches = prefix
			? lower.startsWith(needle)
			: lower.contains(needle);
		if (!matches) {
			continue;
		} else if (!lower.contains(QStringLiteral(".int8."))) {
			return name;
		} else if (fallback.isEmpty()) {
			fallback = name;
		}
	}
	return fallback;
}

struct Model {
	QString tokens;
	QString encoder;
	QString decoder;
	QString joiner;
	QString single;

	[[nodiscard]] bool valid() const {
		return !tokens.isEmpty()
			&& (!encoder.isEmpty() || !single.isEmpty());
	}
	[[nodiscard]] bool transducer() const {
		return !joiner.isEmpty();
	}
};

[[nodiscard]] Model ResolveModel() {
	const auto path = ModelDir();
	if (path.isEmpty()) {
		return Model();
	}
	const auto dir = QDir(path);
	if (!dir.exists()) {
		return Model();
	}
	const auto tokens = dir.filePath(QStringLiteral("tokens.txt"));
	if (!QFileInfo::exists(tokens)) {
		return Model();
	}
	const auto names = dir.entryList(
		{ QStringLiteral("*.onnx") },
		QDir::Files,
		QDir::Name);
	if (names.isEmpty()) {
		return Model();
	}
	auto result = Model();
	const auto encoder = PickModel(names, QStringLiteral("encoder"));
	const auto decoder = PickModel(names, QStringLiteral("decoder"));
	const auto joiner = PickModel(names, QStringLiteral("joiner"));
	if (!encoder.isEmpty() && !decoder.isEmpty()) {
		result.encoder = dir.filePath(encoder);
		result.decoder = dir.filePath(decoder);
		result.joiner = joiner.isEmpty() ? QString() : dir.filePath(joiner);
	} else {
		const auto single = PickModel(
			names,
			QStringLiteral("model"),
			true);
		if (single.isEmpty()) {
			return Model();
		}
		result.single = dir.filePath(single);
	}
	result.tokens = tokens;
	return result;
}

[[nodiscard]] bool ReadAudioBytes(
		const QString &audioPath,
		const QByteArray &audioBytes,
		QByteArray *bytes) {
	if (!audioBytes.isEmpty()) {
		*bytes = audioBytes;
		return true;
	}
	auto file = QFile(audioPath);
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}
	*bytes = file.readAll();
	return !bytes->isEmpty();
}

[[nodiscard]] std::vector<float> ToMono16k(
		const float *samples,
		int count,
		int rate) {
	auto result = std::vector<float>();
	if (!samples || count <= 0 || rate <= 0) {
		return result;
	} else if (rate == kSampleRate) {
		return std::vector<float>(samples, samples + count);
	}
	const auto size = int(std::llround(
		double(count) * kSampleRate / rate));
	if (size <= 0) {
		return result;
	}
	result.resize(size);
	for (auto i = 0; i != size; ++i) {
		const auto position = double(i) * rate / kSampleRate;
		const auto index = int(position);
		const auto fraction = float(position - index);
		const auto from = samples[std::min(index, count - 1)];
		const auto to = samples[std::min(index + 1, count - 1)];
		result[i] = from + (to - from) * fraction;
	}
	return result;
}

[[nodiscard]] std::vector<float> MonoFromStereo(
		const int16_t *stereo,
		size_t frames) {
	auto result = std::vector<float>();
	if (!stereo || !frames) {
		return result;
	}
	result.resize(frames);
	for (auto i = size_t(0); i != frames; ++i) {
		result[i] = (float(stereo[2 * i])
			+ float(stereo[2 * i + 1])) / 65536.f;
	}
	return result;
}

[[nodiscard]] Result Recognize(
		const Model &model,
		const std::vector<float> &samples,
		const QString &langCode) {
	auto config = SherpaOnnxOfflineRecognizerConfig();
	std::memset(&config, 0, sizeof(config));
	config.feat_config.sample_rate = kSampleRate;
	config.feat_config.feature_dim = kFeatureDim;
	config.decoding_method = "greedy_search";
	config.max_active_paths = 4;

	const auto tokens = model.tokens.toUtf8();
	const auto provider = QByteArrayLiteral("cpu");
	const auto language = langCode.toUtf8();
	const auto task = QByteArrayLiteral("transcribe");
	const auto encoder = model.encoder.toUtf8();
	const auto decoder = model.decoder.toUtf8();
	const auto joiner = model.joiner.toUtf8();
	const auto single = model.single.toUtf8();

	config.model_config.tokens = tokens.constData();
	config.model_config.provider = provider.constData();
	config.model_config.num_threads = 2;
	if (model.transducer()) {
		config.model_config.transducer.encoder = encoder.constData();
		config.model_config.transducer.decoder = decoder.constData();
		config.model_config.transducer.joiner = joiner.constData();
	} else if (!model.encoder.isEmpty()) {
		config.model_config.whisper.encoder = encoder.constData();
		config.model_config.whisper.decoder = decoder.constData();
		config.model_config.whisper.language = language.constData();
		config.model_config.whisper.task = task.constData();
	} else {
		config.model_config.sense_voice.model = single.constData();
		config.model_config.sense_voice.language = langCode.isEmpty()
			? "auto"
			: language.constData();
		config.model_config.sense_voice.use_itn = 1;
	}

	const auto recognizer = SherpaOnnxCreateOfflineRecognizer(&config);
	if (!recognizer) {
		return Failure(QStringLiteral("model-invalid"));
	}
	auto result = Failure(QStringLiteral("decode-failed"));
	if (const auto stream = SherpaOnnxCreateOfflineStream(recognizer)) {
		SherpaOnnxAcceptWaveformOffline(
			stream,
			kSampleRate,
			samples.data(),
			int(samples.size()));
		SherpaOnnxDecodeOfflineStream(recognizer, stream);
		if (const auto value = SherpaOnnxGetOfflineStreamResult(stream)) {
			result = {
				true,
				QString::fromUtf8(value->text).trimmed(),
				QString(),
			};
			SherpaOnnxDestroyOfflineRecognizerResult(value);
		}
		SherpaOnnxDestroyOfflineStream(stream);
	}
	SherpaOnnxDestroyOfflineRecognizer(recognizer);
	return result;
}

[[nodiscard]] Result TranscribeMono(
		const Model &model,
		const float *samples,
		int count,
		int rate,
		const QString &langCode) {
	const auto mono = ToMono16k(samples, count, rate);
	if (mono.empty()) {
		return Failure(QStringLiteral("empty-audio"));
	}
	return Recognize(model, mono, langCode);
}

[[nodiscard]] Result TranscribePcmBlocking(
		const int16_t *stereo48k,
		size_t frames,
		const QString &langCode) {
	const auto model = ResolveModel();
	if (!model.valid()) {
		return Failure(QStringLiteral("model-missing"));
	}
	const auto mono = MonoFromStereo(stereo48k, frames);
	return TranscribeMono(
		model,
		mono.data(),
		int(mono.size()),
		kPcmRate,
		langCode);
}

void PostToMain(Done done, Result result) {
	const auto app = QCoreApplication::instance();
	if (!app) {
		done(std::move(result));
		return;
	}
	QMetaObject::invokeMethod(app, [done = std::move(done), result] {
		done(result);
	}, Qt::QueuedConnection);
}

} // namespace

Result TranscribeBlocking(
		const QString &audioPath,
		const QByteArray &audioBytes,
		const QString &langCode) {
	const auto model = ResolveModel();
	if (!model.valid()) {
		return Failure(QStringLiteral("model-missing"));
	}
	auto bytes = QByteArray();
	if (!ReadAudioBytes(audioPath, audioBytes, &bytes)) {
		return Failure(QStringLiteral("invalid-file"));
	}
	const auto wave = SherpaOnnxReadWaveFromBinaryData(
		bytes.constData(),
		int(bytes.size()));
	if (!wave) {
		return Failure(QStringLiteral("invalid-file"));
	}
	const auto result = TranscribeMono(
		model,
		wave->samples,
		wave->num_samples,
		wave->sample_rate,
		langCode);
	SherpaOnnxFreeWave(wave);
	return result;
}

void Transcribe(
		const QString &audioPath,
		const QByteArray &audioBytes,
		const QString &langCode,
		Done done) {
	if (!done) {
		return;
	}
	std::thread(
		[path = audioPath,
			bytes = audioBytes,
			lang = langCode,
			done = std::move(done)]() mutable {
		auto result = TranscribeBlocking(path, bytes, lang);
		PostToMain(std::move(done), std::move(result));
	}).detach();
}

void TranscribePcm(
		const int16_t *stereo48k,
		size_t frames,
		const QString &langCode,
		Done done) {
	if (!done) {
		return;
	}
	auto samples = std::vector<int16_t>();
	if (stereo48k && frames) {
		samples.assign(stereo48k, stereo48k + frames * 2);
	}
	std::thread(
		[samples = std::move(samples),
			lang = langCode,
			done = std::move(done)]() mutable {
		auto result = TranscribePcmBlocking(
			samples.data(),
			samples.size() / 2,
			lang);
		PostToMain(std::move(done), std::move(result));
	}).detach();
}

QString ModelDir() {
	return qEnvironmentVariable("EXTERALESS_STT_MODEL");
}

QString EngineVersion() {
	return QString::fromUtf8(SherpaOnnxGetVersionStr());
}

} // namespace Exteraless::Speech
