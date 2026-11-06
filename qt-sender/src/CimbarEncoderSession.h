#pragma once

#include <QImage>
#include <QString>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

struct EncodeStats {
	qint64 fileBytes = 0;
	qint64 compressedBytes = 0;
	unsigned blocksRequired = 0;
	unsigned blockCount = 0;
	unsigned frameIndex = 0;
	unsigned framesRendered = 0;
	unsigned capacityBytesPerFrame = 0;
	unsigned fountainChunkSize = 0;
	unsigned imageW = 0;
	unsigned imageH = 0;
	int modeVal = 68;
	int compressionLevel = 16;
	int encodeId = 0;
	double targetFps = 15.0;
	double measuredFps = 0.0;
	double estimatedPayloadKbps = 0.0;
	QString fileName;
	QString modeName;
	bool looping = true;
	bool active = false;
};

class CimbarEncoderSession
{
public:
	CimbarEncoderSession();
	~CimbarEncoderSession();

	static int modeFromName(const QString& name);
	static QString nameFromMode(int modeVal);

	bool configure(int modeVal, int compressionLevel);
	bool loadFile(const QString& path, int encodeId = -1);
	void clear();

	std::optional<QImage> nextFrame(int canvasPadding = 32);
	void restartFountainLoop();

	const EncodeStats& stats() const { return _stats; }
	void setTargetFps(double fps);
	void setLooping(bool looping) { _stats.looping = looping; }
	void noteFramePresented(double instantaneousFps);

private:
	struct Impl;
	std::unique_ptr<Impl> _impl;
	EncodeStats _stats;
};
