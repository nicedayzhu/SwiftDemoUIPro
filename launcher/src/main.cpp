#include "LauncherWindow.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Swift DemoUI Pro"));
    application.setApplicationVersion(QStringLiteral("0.1.0"));
    application.setOrganizationName(QStringLiteral("SwiftTools"));
    application.setStyle(QStringLiteral("Fusion"));

    QFont font(QStringLiteral("Segoe UI"));
    font.setStyleHint(QFont::SansSerif);
    application.setFont(font);

    LauncherWindow window;
    window.show();

    const QStringList arguments = application.arguments();
    const int pageIndex = arguments.indexOf(QStringLiteral("--preview-page"));
    if (pageIndex >= 0 && pageIndex + 1 < arguments.size()) {
        bool validPage = false;
        const int requestedPage = arguments[pageIndex + 1].toInt(&validPage);
        if (validPage) {
            if (auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("PageStack")))
                stack->setCurrentIndex(requestedPage);
            const QStringList pageNames = {QStringLiteral("Demo 回放"), QStringLiteral("DemoUI 管理"), QStringLiteral("关于")};
            for (QPushButton *button : window.findChildren<QPushButton *>(QStringLiteral("NavButton")))
                button->setChecked(requestedPage >= 0 && requestedPage < pageNames.size() && button->text() == pageNames[requestedPage]);
        }
    }
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
