#pragma once

#include <QLabel>
#include <QPixmap>

class FrameCanvas : public QLabel
{
	Q_OBJECT
public:
	explicit FrameCanvas(QWidget* parent = nullptr);

	void setFrame(const QImage& image);
	void clearFrame();

protected:
	void resizeEvent(QResizeEvent* event) override;

private:
	void updateScaled();

	QPixmap _source;
};
