#include "FrameCanvas.h"

#include <QResizeEvent>

FrameCanvas::FrameCanvas(QWidget* parent)
	: QLabel(parent)
{
	setAlignment(Qt::AlignCenter);
	setMinimumSize(320, 320);
	setStyleSheet(QStringLiteral("background-color: #050505; color: #888;"));
	setText(QStringLiteral("打开文件后开始发送"));
	setScaledContents(false);
}

void FrameCanvas::setFrame(const QImage& image)
{
	if (image.isNull())
		return;
	_source = QPixmap::fromImage(image);
	setText(QString());
	updateScaled();
}

void FrameCanvas::clearFrame()
{
	_source = QPixmap();
	clear();
	setText(QStringLiteral("打开文件后开始发送"));
}

void FrameCanvas::resizeEvent(QResizeEvent* event)
{
	QLabel::resizeEvent(event);
	updateScaled();
}

void FrameCanvas::updateScaled()
{
	if (_source.isNull())
		return;
	setPixmap(_source.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
