#include "LauncherWindow.h"
#include "AppVersion.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Swift DemoUI Pro"));
    application.setApplicationVersion(QString::fromLatin1(SwiftDemoUIBuild::kVersion));
    application.setProperty("menuVersion", QString::fromLatin1(SwiftDemoUIBuild::kMenuVersion));
    application.setProperty("gitCommit", QString::fromLatin1(SwiftDemoUIBuild::kGitCommit));
    application.setOrganizationName(QStringLiteral("SwiftTools"));
    application.setStyle(QStringLiteral("Fusion"));

    const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/NotoSansSC-VF.ttf"));
    const QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
    QFont font(fontFamilies.isEmpty() ? QStringLiteral("Microsoft YaHei UI") : fontFamilies.constFirst());
    font.setStyleHint(QFont::SansSerif);
    application.setFont(font);

    const QStringList arguments = application.arguments();
    const bool liveUpdatePreview = arguments.contains(QStringLiteral("--live-update-preview"));
    application.setProperty(
        "disableAutoUpdateCheck",
        arguments.contains(QStringLiteral("--no-update-check"))
            || (arguments.contains(QStringLiteral("--render-preview")) && !liveUpdatePreview)
            || arguments.contains(QStringLiteral("--preview-update-bubble")));
    application.setProperty(
        "previewUpdateBubble",
        arguments.contains(QStringLiteral("--preview-update-bubble")));
    const int languageIndex = arguments.indexOf(QStringLiteral("--ui-language"));
    if (languageIndex >= 0 && languageIndex + 1 < arguments.size())
        application.setProperty("uiLanguageOverride", arguments[languageIndex + 1]);

    LauncherWindow window;
    window.show();

    const int pageIndex = arguments.indexOf(QStringLiteral("--preview-page"));
    if (pageIndex >= 0 && pageIndex + 1 < arguments.size()) {
        bool validPage = false;
        const int requestedPage = arguments[pageIndex + 1].toInt(&validPage);
        if (validPage) {
            if (auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("PageStack")))
                stack->setCurrentIndex(requestedPage);
            for (QPushButton *button : window.findChildren<QPushButton *>(QStringLiteral("NavButton")))
                button->setChecked(button->property("pageIndex").toInt() == requestedPage);
        }
    }
    const int previewIndex = arguments.indexOf(QStringLiteral("--render-preview"));
    if (previewIndex >= 0 && previewIndex + 1 < arguments.size()) {
        const QString outputPath = QDir::cleanPath(arguments[previewIndex + 1]);
        const int previewDelay = liveUpdatePreview ? 5000 : 350;
        QTimer::singleShot(previewDelay, &window, [&window, outputPath, &application]() {
            const bool saved = window.grab().save(outputPath, "PNG");
            application.exit(saved ? 0 : 2);
        });
    }
    return application.exec();
}
