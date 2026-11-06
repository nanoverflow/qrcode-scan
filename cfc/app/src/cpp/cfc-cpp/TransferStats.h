#pragma once

#include <chrono>
#include <ctime>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

// Snapshot of receive-side metrics for the Android UI.
struct TransferStatsSnapshot {
	unsigned cameraFrames = 0;
	unsigned scanned = 0;
	unsigned decoded = 0;
	unsigned perfect = 0;
	unsigned bytes = 0;
	unsigned backlog = 0;
	unsigned filesDecoded = 0;
	unsigned filesInFlight = 0;
	unsigned threads = 0;
	int mode = 0;
	int detectedMode = 0;
	double progressPrimary = 0.0;
	std::string progressCsv; // comma-separated 0..1 values
	double scanMsAvg = 0.0;
	double extractMsAvg = 0.0;
	double decodeMsAvg = 0.0;
	double cameraFps = 0.0;
	double decodeFps = 0.0;
	double perfectFps = 0.0;
	double bytesPerSec = 0.0;
	double elapsedSec = 0.0;
};

class TransferStats
{
public:
	void reset()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_start = Clock::now();
		_last = _start;
		_lastCamera = 0;
		_lastDecoded = 0;
		_lastPerfect = 0;
		_lastBytes = 0;
		_cameraFps = _decodeFps = _perfectFps = _bytesPerSec = 0.0;
		_cameraFrames = 0;
		_curDecoded = _curPerfect = _curBytes = 0;
	}

	void onCameraFrame()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		++_cameraFrames;
	}

	TransferStatsSnapshot snapshot(
		unsigned scanned,
		unsigned decoded,
		unsigned perfect,
		unsigned bytes,
		unsigned backlog,
		unsigned filesDecoded,
		unsigned filesInFlight,
		unsigned threads,
		int mode,
		int detectedMode,
		const std::vector<double>& progress,
		unsigned scanTicks,
		unsigned extractTicks,
		unsigned decodeTicks)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_curDecoded = decoded;
		_curPerfect = perfect;
		_curBytes = bytes;
		updateRatesLocked();

		TransferStatsSnapshot s;
		s.cameraFrames = _cameraFrames;
		s.scanned = scanned;
		s.decoded = decoded;
		s.perfect = perfect;
		s.bytes = bytes;
		s.backlog = backlog;
		s.filesDecoded = filesDecoded;
		s.filesInFlight = filesInFlight;
		s.threads = threads;
		s.mode = mode;
		s.detectedMode = detectedMode;
		s.cameraFps = _cameraFps;
		s.decodeFps = _decodeFps;
		s.perfectFps = _perfectFps;
		s.bytesPerSec = _bytesPerSec;
		s.elapsedSec = secondsSince(_start);

		if (!progress.empty())
			s.progressPrimary = progress.front();
		std::ostringstream csv;
		for (size_t i = 0; i < progress.size(); ++i) {
			if (i)
				csv << ',';
			csv << progress[i];
		}
		s.progressCsv = csv.str();

		auto avgMs = [](unsigned ticks, unsigned denom) -> double {
			if (!denom)
				return 0.0;
			return (static_cast<double>(ticks) / static_cast<double>(denom)) * 1000.0 / CLOCKS_PER_SEC;
		};
		s.scanMsAvg = avgMs(scanTicks, scanned);
		s.extractMsAvg = avgMs(extractTicks, decoded);
		s.decodeMsAvg = avgMs(decodeTicks, decoded);
		return s;
	}

	static std::string toPipeString(const TransferStatsSnapshot& s)
	{
		// Keep field order stable for Java parsing.
		std::ostringstream o;
		o << s.cameraFrames << '|'
		  << s.scanned << '|'
		  << s.decoded << '|'
		  << s.perfect << '|'
		  << s.bytes << '|'
		  << s.backlog << '|'
		  << s.filesDecoded << '|'
		  << s.filesInFlight << '|'
		  << s.threads << '|'
		  << s.mode << '|'
		  << s.detectedMode << '|'
		  << s.progressPrimary << '|'
		  << s.cameraFps << '|'
		  << s.decodeFps << '|'
		  << s.perfectFps << '|'
		  << s.bytesPerSec << '|'
		  << s.elapsedSec << '|'
		  << s.scanMsAvg << '|'
		  << s.extractMsAvg << '|'
		  << s.decodeMsAvg << '|'
		  << s.progressCsv;
		return o.str();
	}

private:
	using Clock = std::chrono::steady_clock;

	void updateRatesLocked()
	{
		const auto now = Clock::now();
		const double dt = std::chrono::duration<double>(now - _last).count();
		if (dt < 0.25)
			return;

		_cameraFps = (_cameraFrames - _lastCamera) / dt;
		_decodeFps = (_curDecoded - _lastDecoded) / dt;
		_perfectFps = (_curPerfect - _lastPerfect) / dt;
		_bytesPerSec = (_curBytes - _lastBytes) / dt;

		_lastCamera = _cameraFrames;
		_lastDecoded = _curDecoded;
		_lastPerfect = _curPerfect;
		_lastBytes = _curBytes;
		_last = now;
	}

	double secondsSince(Clock::time_point t) const
	{
		return std::chrono::duration<double>(Clock::now() - t).count();
	}

	std::mutex _mutex;
	Clock::time_point _start = Clock::now();
	Clock::time_point _last = _start;
	unsigned _cameraFrames = 0;
	unsigned _lastCamera = 0;
	unsigned _lastDecoded = 0;
	unsigned _lastPerfect = 0;
	unsigned _lastBytes = 0;
	unsigned _curDecoded = 0;
	unsigned _curPerfect = 0;
	unsigned _curBytes = 0;
	double _cameraFps = 0.0;
	double _decodeFps = 0.0;
	double _perfectFps = 0.0;
	double _bytesPerSec = 0.0;
};
