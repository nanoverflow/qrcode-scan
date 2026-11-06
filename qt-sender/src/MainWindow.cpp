#include "MainWindow.h"
#include "FrameCanvas.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
{
	setWindowTitle(QStringLiteral("Cimbar Qt Sender"));
	resize(1180, 760);

	auto* central = new QWidget(this);
	setCentralWidget(central);
	auto* root = new QHBoxLayout(central);

	auto* splitter = new QSplitter(Qt::Horizontal, central);
	root->addWidget(splitter);

	_canvas = new FrameCanvas(splitter);
	splitter->addWidget(_canvas);

	auto* side = new QWidget(splitter);
	auto* sideLayout = new QVBoxLayout(side);
	splitter->addWidget(side);
	splitter->setStretchFactor(0, 4);
	splitter->setStretchFactor(1, 1);

	_fileLabel = new QLabel(QStringLiteral("未选择文件"), side);
	_fileLabel->setWordWrap(true);
	sideLayout->addWidget(_fileLabel);

	auto* params = new QGroupBox(QStringLiteral("发送参数"), side);
	auto* form = new QFormLayout(params);

	_modeCombo = new QComboBox(params);
	_modeCombo->addItems({QStringLiteral("B"), QStringLiteral("Bm"), QStringLiteral("Bu"), QStringLiteral("4C")});
	form->addRow(QStringLiteral("模式"), _modeCombo);

	_fpsSpin = new QSpinBox(params);
	_fpsSpin->setRange(1, 60);
	_fpsSpin->setValue(15);
	_fpsSpin->setSuffix(QStringLiteral(" fps"));
	form->addRow(QStringLiteral("目标帧率"), _fpsSpin);

	_compressionSpin = new QSpinBox(params);
	_compressionSpin->setRange(0, 22);
	_compressionSpin->setValue(16);
	_compressionSpin->setToolTip(QStringLiteral("0 = 不压缩；默认 16（zstd）"));
	form->addRow(QStringLiteral("压缩等级"), _compressionSpin);

	_paddingSpin = new QSpinBox(params);
	_paddingSpin->setRange(0, 128);
	_paddingSpin->setValue(32);
	_paddingSpin->setSuffix(QStringLiteral(" px"));
	form->addRow(QStringLiteral("黑边"), _paddingSpin);

	_encodeIdSpin = new QSpinBox(params);
	_encodeIdSpin->setRange(-1, 127);
	_encodeIdSpin->setValue(-1);
	_encodeIdSpin->setSpecialValueText(QStringLiteral("自动递增"));
	_encodeIdSpin->setToolTip(QStringLiteral("-1 表示每次加载文件自动 +1（与 cimbar_send 一致）"));
	form->addRow(QStringLiteral("Encode ID"), _encodeIdSpin);

	_loopCheck = new QCheckBox(QStringLiteral("循环播放码流"), params);
	_loopCheck->setChecked(true);
	form->addRow(QString(), _loopCheck);

	_fullscreenCheck = new QCheckBox(QStringLiteral("全屏预览 (F11)"), params);
	form->addRow(QString(), _fullscreenCheck);

	sideLayout->addWidget(params);

	auto* buttons = new QHBoxLayout();
	_openBtn = new QPushButton(QStringLiteral("打开文件…"), side);
	_applyBtn = new QPushButton(QStringLiteral("应用参数"), side);
	_startBtn = new QPushButton(QStringLiteral("开始发送"), side);
	_startBtn->setEnabled(false);
	buttons->addWidget(_openBtn);
	buttons->addWidget(_applyBtn);
	buttons->addWidget(_startBtn);
	sideLayout->addLayout(buttons);

	_statsLabel = new QLabel(side);
	_statsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	_statsLabel->setWordWrap(true);
	_statsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	_statsLabel->setStyleSheet(QStringLiteral(
		"QLabel { font-family: Consolas, 'Courier New', monospace; font-size: 12px;"
		" background: #111; color: #d8ffe0; padding: 10px; border-radius: 6px; }"));
	sideLayout->addWidget(_statsLabel, 1);

	_timer = new QTimer(this);
	connect(_timer, &QTimer::timeout, this, &MainWindow::onTick);
	connect(_openBtn, &QPushButton::clicked, this, &MainWindow::onOpenFile);
	connect(_startBtn, &QPushButton::clicked, this, &MainWindow::onStartStop);
	connect(_applyBtn, &QPushButton::clicked, this, &MainWindow::onApplyParams);
	connect(_fullscreenCheck, &QCheckBox::toggled, this, &MainWindow::onFullscreenToggle);

	_session.configure(CimbarEncoderSession::modeFromName(_modeCombo->currentText()),
			   _compressionSpin->value());
	_session.setTargetFps(_fpsSpin->value());
	rebuildStatsText();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_F11) {
		_fullscreenCheck->toggle();
		return;
	}
	if (event->key() == Qt::Key_Escape && isFullScreen()) {
		_fullscreenCheck->setChecked(false);
		return;
	}
	if (event->key() == Qt::Key_Space && _startBtn->isEnabled()) {
		onStartStop();
		return;
	}
	QMainWindow::keyPressEvent(event);
}

void MainWindow::onOpenFile()
{
	const QString path = QFileDialog::getOpenFileName(
		this,
		QStringLiteral("选择要发送的文件"),
		QString(),
		QStringLiteral("所有文件 (*.*)"));
	if (path.isEmpty())
		return;

	onApplyParams();
	const int encodeId = _encodeIdSpin->value();
	if (!_session.loadFile(path, encodeId)) {
		QMessageBox::warning(this, QStringLiteral("加载失败"),
				     QStringLiteral("无法编码文件：\n%1").arg(path));
		return;
	}

	_currentPath = path;
	_fileLabel->setText(QStringLiteral("文件：%1").arg(path));
	_startBtn->setEnabled(true);
	_session.setLooping(_loopCheck->isChecked());
	rebuildStatsText();

	// Show first frame immediately as preview
	if (auto img = _session.nextFrame(_paddingSpin->value())) {
		_canvas->setFrame(*img);
		rebuildStatsText();
	}
}

void MainWindow::onApplyParams()
{
	const int mode = CimbarEncoderSession::modeFromName(_modeCombo->currentText());
	_session.configure(mode, _compressionSpin->value());
	_session.setTargetFps(_fpsSpin->value());

	_session.setLooping(_loopCheck->isChecked());

	if (_playing) {
		_timer->setInterval(std::max(1, 1000 / std::max(1, _fpsSpin->value())));
	}

	if (!_currentPath.isEmpty() && !_playing) {
		// Re-encode with new mode/compression if a file is loaded and idle.
		const int encodeId = _encodeIdSpin->value();
		if (!_session.loadFile(_currentPath, encodeId < 0 ? _session.stats().encodeId : encodeId)) {
			QMessageBox::warning(this, QStringLiteral("应用参数失败"),
					     QStringLiteral("使用新参数重新编码失败。"));
		} else if (auto img = _session.nextFrame(_paddingSpin->value())) {
			_canvas->setFrame(*img);
		}
	}
	rebuildStatsText();
}

void MainWindow::onStartStop()
{
	setPlaying(!_playing);
}

void MainWindow::setPlaying(bool playing)
{
	_playing = playing;
	_startBtn->setText(playing ? QStringLiteral("停止发送") : QStringLiteral("开始发送"));
	syncControlsEnabled(!playing);

	if (playing) {
		_session.setLooping(_loopCheck->isChecked());
		_session.setTargetFps(_fpsSpin->value());
		_timer->start(std::max(1, 1000 / std::max(1, _fpsSpin->value())));
		_frameClock.restart();
	} else {
		_timer->stop();
	}
	rebuildStatsText();
}

void MainWindow::syncControlsEnabled(bool enableParams)
{
	_modeCombo->setEnabled(enableParams);
	_compressionSpin->setEnabled(enableParams);
	_encodeIdSpin->setEnabled(enableParams);
	_applyBtn->setEnabled(enableParams);
	_openBtn->setEnabled(enableParams);
	// fps/padding/loop can change live
}

void MainWindow::onTick()
{
	if (!_session.stats().active)
		return;

	const qint64 elapsedMs = _frameClock.restart();
	const double instFps = elapsedMs > 0 ? 1000.0 / static_cast<double>(elapsedMs) : 0.0;

	// Live fps / padding / loop updates
	_session.setTargetFps(_fpsSpin->value());
	_session.setLooping(_loopCheck->isChecked());
	_timer->setInterval(std::max(1, 1000 / std::max(1, _fpsSpin->value())));

	auto img = _session.nextFrame(_paddingSpin->value());
	if (!img) {
		setPlaying(false);
		QMessageBox::information(this, QStringLiteral("发送结束"),
					 QStringLiteral("码流已播放完毕（未开启循环）。"));
		return;
	}
	_session.noteFramePresented(instFps);
	_canvas->setFrame(*img);
	rebuildStatsText();
}

void MainWindow::onFullscreenToggle(bool on)
{
	if (on)
		showFullScreen();
	else
		showNormal();
}

void MainWindow::rebuildStatsText()
{
	const EncodeStats& s = _session.stats();
	const double progress =
		s.blocksRequired > 0
			? std::min(1.0, static_cast<double>(s.blockCount) / static_cast<double>(s.blocksRequired * 8))
			: 0.0;

	const QString text = QStringLiteral(
				  "状态: %1\n"
				  "模式: %2 (id=%3)\n"
				  "文件: %4\n"
				  "原始大小: %5 字节 (%6 KB)\n"
				  "压缩后约: %7 字节 (%8 KB)\n"
				  "压缩等级: %9\n"
				  "画面尺寸: %10 x %11\n"
				  "每帧容量: %12 字节\n"
				  "喷泉块大小: %13\n"
				  "所需最小块数: %14\n"
				  "当前块序号: %15\n"
				  "已渲染帧: %16 (本轮 %17)\n"
				  "Encode ID: %18\n"
				  "目标帧率: %19 fps\n"
				  "实测帧率: %20 fps\n"
				  "估算载荷吞吐: %21 kbps (~%22 KB/s)\n"
				  "循环进度: %23%\n"
				  "提示: Space 开始/停止 · F11 全屏")
				  .arg(s.active ? (_playing ? QStringLiteral("发送中") : QStringLiteral("已就绪"))
						: QStringLiteral("空闲"))
				  .arg(s.modeName)
				  .arg(s.modeVal)
				  .arg(s.fileName.isEmpty() ? QStringLiteral("-") : s.fileName)
				  .arg(s.fileBytes)
				  .arg(QString::number(s.fileBytes / 1024.0, 'f', 2))
				  .arg(s.compressedBytes)
				  .arg(QString::number(s.compressedBytes / 1024.0, 'f', 2))
				  .arg(s.compressionLevel)
				  .arg(s.imageW)
				  .arg(s.imageH)
				  .arg(s.capacityBytesPerFrame)
				  .arg(s.fountainChunkSize)
				  .arg(s.blocksRequired)
				  .arg(s.blockCount)
				  .arg(s.framesRendered)
				  .arg(s.frameIndex)
				  .arg(s.encodeId)
				  .arg(QString::number(s.targetFps, 'f', 1))
				  .arg(QString::number(s.measuredFps, 'f', 1))
				  .arg(QString::number(s.estimatedPayloadKbps, 'f', 1))
				  .arg(QString::number(s.estimatedPayloadKbps / 8.0, 'f', 1))
				  .arg(QString::number(progress * 100.0, 'f', 1));

	_statsLabel->setText(text);
}
