#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QApplication::setApplicationName(QStringLiteral("Cimbar Qt Sender"));
	QApplication::setOrganizationName(QStringLiteral("bar-code"));

	MainWindow w;
	w.show();
	return app.exec();
}
