#include <QApplication>
#include <QIcon>

#include "MainWindow.h"

// Application entry point. Kept intentionally minimal: all setup happens
// inside MainWindow's constructor.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Codex"));
    QApplication::setApplicationName(QStringLiteral("FFmpeg Audio Track Editor"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app.svg")));

    MainWindow window;
    window.show();

    return QApplication::exec();
}
