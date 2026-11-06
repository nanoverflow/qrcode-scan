#pragma once

#include "CimbarEncoderSession.h"

#include <QMainWindow>
#include <QElapsedTimer>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;
class FrameCanvas;

class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	explicit MainWindow(QWidget* parent = nullptr);

protected:
	void keyPressEvent(QKeyEvent* event) override;

private slots:
	void onOpenFile();
	void onStartStop();
	void onApplyParams();
	void onTick();
	void onFullscreenToggle(bool on);

private:
	void rebuildStatsText();
	void setPlaying(bool playing);
	void syncControlsEnabled(bool enableParams);

	CimbarEncoderSession _session;
	FrameCanvas* _canvas = nullptr;
	QLabel* _statsLabel = nullptr;
	QLabel* _fileLabel = nullptr;

	QComboBox* _modeCombo = nullptr;
	QSpinBox* _fpsSpin = nullptr;
	QSpinBox* _compressionSpin = nullptr;
	QSpinBox* _paddingSpin = nullptr;
	QSpinBox* _encodeIdSpin = nullptr;
	QCheckBox* _loopCheck = nullptr;
	QCheckBox* _fullscreenCheck = nullptr;

	QPushButton* _openBtn = nullptr;
	QPushButton* _startBtn = nullptr;
	QPushButton* _applyBtn = nullptr;

	QTimer* _timer = nullptr;
	QElapsedTimer _frameClock;
	QString _currentPath;
	bool _playing = false;
};
