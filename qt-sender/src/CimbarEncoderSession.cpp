#include "CimbarEncoderSession.h"

#include "cimb_translator/Config.h"
#include "compression/zstd_compressor.h"
#include "encoder/Encoder.h"
#include "fountain/FountainInit.h"
#include "fountain/fountain_encoder_stream.h"
#include "util/File.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>

#include <algorithm>
#include <sstream>

namespace {
QImage matToQImage(const cv::Mat& mat)
{
	if (mat.empty())
		return {};

	cv::Mat rgb;
	if (mat.channels() == 4)
		cv::cvtColor(mat, rgb, cv::COLOR_BGRA2RGBA);
	else if (mat.channels() == 3)
		cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
	else if (mat.channels() == 1)
		cv::cvtColor(mat, rgb, cv::COLOR_GRAY2RGB);
	else
		return {};

	QImage img(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
	return img.copy();
}

cv::Mat withPadding(const cv::Mat& src, int padding)
{
	if (padding <= 0)
		return src;
	cv::Mat dst;
	cv::copyMakeBorder(src, dst, padding, padding, padding, padding, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
	return dst;
}
} // namespace

struct CimbarEncoderSession::Impl {
	fountain_encoder_stream::ptr fes;
	std::string compressedPayload;
	uint8_t encodeId = 109;
};

CimbarEncoderSession::CimbarEncoderSession()
	: _impl(std::make_unique<Impl>())
{
	FountainInit::init();
	_stats.modeName = nameFromMode(_stats.modeVal);
}

CimbarEncoderSession::~CimbarEncoderSession() = default;

int CimbarEncoderSession::modeFromName(const QString& name)
{
	const QString m = name.trimmed();
	if (m.compare(QStringLiteral("4C"), Qt::CaseInsensitive) == 0)
		return 4;
	if (m.compare(QStringLiteral("Bu"), Qt::CaseInsensitive) == 0)
		return 66;
	if (m.compare(QStringLiteral("Bm"), Qt::CaseInsensitive) == 0)
		return 67;
	return 68; // B
}

QString CimbarEncoderSession::nameFromMode(int modeVal)
{
	switch (modeVal) {
	case 4: return QStringLiteral("4C");
	case 66: return QStringLiteral("Bu");
	case 67: return QStringLiteral("Bm");
	default: return QStringLiteral("B");
	}
}

bool CimbarEncoderSession::configure(int modeVal, int compressionLevel)
{
	if (compressionLevel < 0 || compressionLevel > 22)
		compressionLevel = cimbar::Config::compression_level();

	_stats.modeVal = modeVal;
	_stats.compressionLevel = compressionLevel;
	_stats.modeName = nameFromMode(modeVal);
	cimbar::Config::update(modeVal);

	_stats.capacityBytesPerFrame = cimbar::Config::capacity();
	_stats.fountainChunkSize = cimbar::Config::fountain_chunk_size();
	_stats.imageW = cimbar::Config::image_size_x();
	_stats.imageH = cimbar::Config::image_size_y();
	return true;
}

bool CimbarEncoderSession::loadFile(const QString& path, int encodeId)
{
	configure(_stats.modeVal, _stats.compressionLevel);

	if (!FountainInit::init())
		return false;

	QFileInfo info(path);
	if (!info.exists() || !info.isFile())
		return false;

	// QFile handles Unicode paths on Windows; std::ifstream(path.toStdString()) does not.
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	const QByteArray rawBytes = file.readAll();
	file.close();
	const std::string rawStr(rawBytes.constData(), static_cast<size_t>(rawBytes.size()));
	_stats.fileBytes = static_cast<qint64>(rawStr.size());
	_stats.fileName = info.fileName();

	if (encodeId < 0)
		++_impl->encodeId;
	else
		_impl->encodeId = static_cast<uint8_t>(encodeId & 0x7F);
	_stats.encodeId = _impl->encodeId;

	{
		std::istringstream input(rawStr);
		const unsigned chunk = cimbar::Config::fountain_chunk_size();
		// Wire format expects UTF-8 filename bytes (not Windows local ANSI).
		const QByteArray fnameUtf8 = info.fileName().toUtf8();

		if (_stats.compressionLevel <= 0) {
			std::stringstream ss;
			ss << input.rdbuf();
			if (static_cast<size_t>(ss.tellp()) < chunk) {
				// pad plain payload so fountain has enough material
				std::string pad(chunk - static_cast<size_t>(ss.tellp()) + 1, '\0');
				ss.write(pad.data(), static_cast<std::streamsize>(pad.size()));
			}
			_stats.compressedBytes = static_cast<qint64>(ss.tellp());
			_impl->fes = fountain_encoder_stream::create(ss, chunk, _impl->encodeId);
		} else {
			cimbar::zstd_compressor<std::stringstream> comp;
			comp.set_compression_level(_stats.compressionLevel);
			comp.write_header(fnameUtf8.constData(), static_cast<unsigned>(fnameUtf8.size()));
			if (!comp.compress(input))
				return false;

			size_t compressedSize = comp.size();
			_stats.compressedBytes = static_cast<qint64>(compressedSize);
			if (compressedSize < chunk)
				comp.pad(chunk - compressedSize + 1);

			_impl->fes = fountain_encoder_stream::create(comp, chunk, _impl->encodeId);
		}
	}

	if (!_impl->fes)
		return false;

	_stats.blocksRequired = _impl->fes->blocks_required();
	_stats.blockCount = 0;
	_stats.frameIndex = 0;
	_stats.framesRendered = 0;
	_stats.active = true;
	_stats.capacityBytesPerFrame = cimbar::Config::capacity();
	_stats.fountainChunkSize = cimbar::Config::fountain_chunk_size();
	_stats.imageW = cimbar::Config::image_size_x();
	_stats.imageH = cimbar::Config::image_size_y();
	setTargetFps(_stats.targetFps);
	return true;
}

void CimbarEncoderSession::clear()
{
	_impl->fes.reset();
	_stats.active = false;
	_stats.blockCount = 0;
	_stats.frameIndex = 0;
}

void CimbarEncoderSession::setTargetFps(double fps)
{
	if (fps < 1.0)
		fps = 1.0;
	if (fps > 60.0)
		fps = 60.0;
	_stats.targetFps = fps;
	// Fountain payload estimate: capacity * ECC efficiency * FPS
	const double eccRatio =
		static_cast<double>(cimbar::Config::ecc_block_size() - cimbar::Config::ecc_bytes()) /
		std::max(1u, cimbar::Config::ecc_block_size());
	_stats.estimatedPayloadKbps =
		(_stats.capacityBytesPerFrame * eccRatio * _stats.targetFps * 8.0) / 1000.0;
}

void CimbarEncoderSession::noteFramePresented(double instantaneousFps)
{
	if (instantaneousFps > 0.0)
		_stats.measuredFps = instantaneousFps;
}

void CimbarEncoderSession::restartFountainLoop()
{
	if (_impl->fes) {
		_impl->fes->restart();
		_stats.frameIndex = 0;
		_stats.blockCount = 0;
	}
}

std::optional<QImage> CimbarEncoderSession::nextFrame(int canvasPadding)
{
	if (!_impl->fes)
		return std::nullopt;

	// Match cimbar_js: emit ~8x the minimum fountain blocks, then loop.
	const unsigned required = _impl->fes->blocks_required() * 8;
	if (_impl->fes->block_count() > required) {
		if (_stats.looping)
			_impl->fes->restart();
		else
			return std::nullopt;
		_stats.frameIndex = 0;
	}

	Encoder enc;
	enc.set_encode_id(_impl->encodeId);
	auto mat = enc.encode_next(*_impl->fes);
	if (!mat)
		return std::nullopt;

	++_stats.frameIndex;
	++_stats.framesRendered;
	_stats.blockCount = _impl->fes->block_count();
	_stats.blocksRequired = _impl->fes->blocks_required();

	cv::Mat padded = withPadding(*mat, canvasPadding);
	return matToQImage(padded);
}
