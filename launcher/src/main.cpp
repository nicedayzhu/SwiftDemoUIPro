#include "LauncherWindow.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Swift Demo Launcher"));
    application.setApplicationVersion(QStringLiteral("0.1.0"));
    application.setOrganizationName(QStringLiteral("SwiftTools"));
    application.setStyle(QStringLiteral("Fusion"));

    QFont font(QStringLiteral("Segoe UI"));
    font.setStyleHint(QFont::SansSerif);
    application.setFont(font);

    LauncherWindow window;
    window.show();

    const QStringList arguments = application.arguments();
    const int previewIndex = arguments.indexOf(QStringLiteral("--render-preview"));
    if (previewIndex >= 0 && previewIndex + 1 < arguments.size()) {
        const QString outputPath = QDir::cleanPath(arguments[previewIndex + 1]);
        QTimer::singleShot(350, &window, [&window, outputPath, &application]() {
            const bool saved = window.grab().save(outputPath, "PNG");
            application.exit(saved ? 0 : 2);
        });
    }
    return application.exec();
}
