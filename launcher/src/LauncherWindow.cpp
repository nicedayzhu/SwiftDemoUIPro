#include "LauncherWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFrame>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QTranslator>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <functional>
#include <utility>

namespace
{
bool isDemoSource(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix();
    return suffix.compare(QStringLiteral("dem"), Qt::CaseInsensitive) == 0
        || suffix.compare(QStringLiteral("zip"), Qt::CaseInsensitive) == 0
        || suffix.compare(QStringLiteral("zst"), Qt::CaseInsensitive) == 0;
}

QPixmap makeLogo()
{
    QPixmap pixmap(48, 48);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#5aa9ff")));
    painter.drawRoundedRect(QRectF(1, 1, 46, 46), 13, 13);
    painter.setBrush(QColor(QStringLiteral("#ffffff")));
    QPolygonF triangle;
    triangle << QPointF(20, 15) << QPointF(20, 33) << QPointF(35, 24);
    painter.drawPolygon(triangle);
    return pixmap;
}

QPixmap makeCircularPixmap(const QPixmap &source, int side)
{
    QPixmap result(side, side);
    result.fill(Qt::transparent);

    const int inset = 2;
    const QRectF avatarRect(inset, inset, side - inset * 2, side - inset * 2);
    const QPixmap scaled = source.scaled(
        avatarRect.size().toSize(),
        Qt::KeepAspectRatioByExpanding,
        Qt::SmoothTransformation);
    const QRect sourceRect(
        qMax(0, (scaled.width() - static_cast<int>(avatarRect.width())) / 2),
        qMax(0, (scaled.height() - static_cast<int>(avatarRect.height())) / 2),
        static_cast<int>(avatarRect.width()),
        static_cast<int>(avatarRect.height()));

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addEllipse(avatarRect);
    painter.setClipPath(clip);
    painter.drawPixmap(avatarRect, scaled, sourceRect);
    painter.setClipping(false);
    painter.setPen(QPen(QColor(QStringLiteral("#ffffff")), 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(avatarRect);
    painter.setPen(QPen(QColor(QStringLiteral("#d9e2ee")), 1));
    painter.drawEllipse(QRectF(0.5, 0.5, side - 1.0, side - 1.0));
    return result;
}

QPixmap makeAuthorFallback(int side)
{
    QPixmap result(side, side);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient gradient(0, 0, side, side);
    gradient.setColorAt(0.0, QColor(QStringLiteral("#56b4ff")));
    gradient.setColorAt(1.0, QColor(QStringLiteral("#2563d9")));
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawEllipse(QRectF(1, 1, side - 2, side - 2));
    painter.setPen(QColor(QStringLiteral("#ffffff")));
    QFont font = painter.font();
    font.setPixelSize(qRound(side * 0.31));
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    painter.drawText(QRectF(0, 0, side, side), Qt::AlignCenter, QStringLiteral("NZ"));
    return result;
}

enum class Glyph
{
    Demo,
    Folder,
    Play,
    Stop,
    Menu,
    About,
    Website,
    Code,
    Video,
    External,
    Coffee
};

QPixmap makeGlyph(Glyph glyph, int side, const QColor &color)
{
    QPixmap pixmap(side, side);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, qMax(1.5, side / 12.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    if (glyph == Glyph::Demo) {
        painter.drawRoundedRect(QRectF(side * 0.16, side * 0.12, side * 0.68, side * 0.76), side * 0.12, side * 0.12);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        QPolygonF triangle;
        triangle << QPointF(side * 0.43, side * 0.35)
                 << QPointF(side * 0.43, side * 0.65)
                 << QPointF(side * 0.68, side * 0.50);
        painter.drawPolygon(triangle);
    } else if (glyph == Glyph::Folder) {
        painter.drawLine(QPointF(side * 0.15, side * 0.35), QPointF(side * 0.37, side * 0.35));
        painter.drawLine(QPointF(side * 0.37, side * 0.35), QPointF(side * 0.45, side * 0.24));
        painter.drawLine(QPointF(side * 0.45, side * 0.24), QPointF(side * 0.70, side * 0.24));
        painter.drawRoundedRect(QRectF(side * 0.12, side * 0.34, side * 0.76, side * 0.48), side * 0.08, side * 0.08);
    } else if (glyph == Glyph::Play) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        QPolygonF triangle;
        triangle << QPointF(side * 0.30, side * 0.19)
                 << QPointF(side * 0.30, side * 0.81)
                 << QPointF(side * 0.79, side * 0.50);
        painter.drawPolygon(triangle);
    } else if (glyph == Glyph::Stop) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(side * 0.25, side * 0.25, side * 0.50, side * 0.50), side * 0.08, side * 0.08);
    } else if (glyph == Glyph::Menu) {
        painter.drawRoundedRect(QRectF(side * 0.18, side * 0.18, side * 0.24, side * 0.24), side * 0.04, side * 0.04);
        painter.drawRoundedRect(QRectF(side * 0.58, side * 0.18, side * 0.24, side * 0.24), side * 0.04, side * 0.04);
        painter.drawRoundedRect(QRectF(side * 0.18, side * 0.58, side * 0.24, side * 0.24), side * 0.04, side * 0.04);
        painter.drawRoundedRect(QRectF(side * 0.58, side * 0.58, side * 0.24, side * 0.24), side * 0.04, side * 0.04);
    } else if (glyph == Glyph::About) {
        painter.drawEllipse(QRectF(side * 0.16, side * 0.16, side * 0.68, side * 0.68));
        painter.drawPoint(QPointF(side * 0.50, side * 0.34));
        painter.drawLine(QPointF(side * 0.50, side * 0.47), QPointF(side * 0.50, side * 0.68));
    } else if (glyph == Glyph::Website) {
        painter.drawEllipse(QRectF(side * 0.14, side * 0.14, side * 0.72, side * 0.72));
        painter.drawEllipse(QRectF(side * 0.34, side * 0.14, side * 0.32, side * 0.72));
        painter.drawLine(QPointF(side * 0.16, side * 0.50), QPointF(side * 0.84, side * 0.50));
    } else if (glyph == Glyph::Code) {
        painter.drawLine(QPointF(side * 0.38, side * 0.25), QPointF(side * 0.17, side * 0.50));
        painter.drawLine(QPointF(side * 0.17, side * 0.50), QPointF(side * 0.38, side * 0.75));
        painter.drawLine(QPointF(side * 0.62, side * 0.25), QPointF(side * 0.83, side * 0.50));
        painter.drawLine(QPointF(side * 0.83, side * 0.50), QPointF(side * 0.62, side * 0.75));
    } else if (glyph == Glyph::Video) {
        painter.drawRoundedRect(QRectF(side * 0.12, side * 0.22, side * 0.76, side * 0.56), side * 0.10, side * 0.10);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        QPolygonF triangle;
        triangle << QPointF(side * 0.43, side * 0.36)
                 << QPointF(side * 0.43, side * 0.64)
                 << QPointF(side * 0.65, side * 0.50);
        painter.drawPolygon(triangle);
    } else if (glyph == Glyph::External) {
        painter.drawRoundedRect(QRectF(side * 0.14, side * 0.30, side * 0.56, side * 0.56), side * 0.08, side * 0.08);
        painter.drawLine(QPointF(side * 0.46, side * 0.54), QPointF(side * 0.84, side * 0.16));
        painter.drawLine(QPointF(side * 0.59, side * 0.16), QPointF(side * 0.84, side * 0.16));
        painter.drawLine(QPointF(side * 0.84, side * 0.16), QPointF(side * 0.84, side * 0.41));
    } else {
        painter.drawRoundedRect(QRectF(side * 0.18, side * 0.28, side * 0.52, side * 0.42), side * 0.08, side * 0.08);
        painter.drawArc(QRectF(side * 0.62, side * 0.35, side * 0.24, side * 0.25), -80 * 16, 170 * 16);
        painter.drawLine(QPointF(side * 0.24, side * 0.80), QPointF(side * 0.72, side * 0.80));
    }
    return pixmap;
}

QIcon makeGlyphIcon(Glyph glyph, const QColor &color)
{
    return QIcon(makeGlyph(glyph, 24, color));
}

QLabel *sectionEyebrow(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("SectionEyebrow"));
    return label;
}

class LinkButton final : public QPushButton
{
public:
    explicit LinkButton(QWidget *parent = nullptr)
        : QPushButton(parent)
    {
        setProperty("keyboardFocus", false);
    }

protected:
    void focusInEvent(QFocusEvent *event) override
    {
        QPushButton::focusInEvent(event);
        const bool keyboardFocus = event->reason() == Qt::TabFocusReason
            || event->reason() == Qt::BacktabFocusReason
            || event->reason() == Qt::ShortcutFocusReason;
        setKeyboardFocus(keyboardFocus);
    }

    void focusOutEvent(QFocusEvent *event) override
    {
        QPushButton::focusOutEvent(event);
        setKeyboardFocus(false);
    }

private:
    void setKeyboardFocus(bool enabled)
    {
        if (property("keyboardFocus").toBool() == enabled)
            return;
        setProperty("keyboardFocus", enabled);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }
};

class ClickableDisclosureRow final : public QFrame
{
public:
    explicit ClickableDisclosureRow(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setProperty("keyboardFocus", false);
    }

    void setActivatedHandler(std::function<void()> handler)
    {
        activatedHandler_ = std::move(handler);
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            event->accept();
            clearFocus();
            if (activatedHandler_)
                activatedHandler_();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return
            || event->key() == Qt::Key_Enter
            || event->key() == Qt::Key_Space) {
            event->accept();
            if (activatedHandler_)
                activatedHandler_();
            return;
        }
        QFrame::keyPressEvent(event);
    }

    void focusInEvent(QFocusEvent *event) override
    {
        QFrame::focusInEvent(event);
        const bool keyboardFocus = event->reason() == Qt::TabFocusReason
            || event->reason() == Qt::BacktabFocusReason
            || event->reason() == Qt::ShortcutFocusReason;
        setKeyboardFocus(keyboardFocus);
    }

    void focusOutEvent(QFocusEvent *event) override
    {
        QFrame::focusOutEvent(event);
        setKeyboardFocus(false);
    }

private:
    void setKeyboardFocus(bool enabled)
    {
        if (property("keyboardFocus").toBool() == enabled)
            return;
        setProperty("keyboardFocus", enabled);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }

    std::function<void()> activatedHandler_;
};

}

LauncherWindow::LauncherWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Swift DemoUI Pro"));
    setWindowIcon(QIcon(makeLogo()));
    setMinimumSize(920, 640);
    resize(1000, 720);
    setAcceptDrops(true);

    translator_ = new QTranslator(this);
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    const QString languageOverride = qApp->property("uiLanguageOverride").toString();
    currentLanguage_ = languageOverride.isEmpty()
        ? settings.value(QStringLiteral("uiLanguage"), QStringLiteral("system")).toString()
        : languageOverride;
    trueViewEnabled_ = settings.value(QStringLiteral("trueViewEnabled"), false).toBool();
    advancedLaunchOptionsText_ = settings.value(QStringLiteral("advancedLaunchOptions")).toString();
    if (!loadLanguage(currentLanguage_)) {
        currentLanguage_ = QStringLiteral("system");
        loadLanguage(currentLanguage_);
    }

    updateService_ = new UpdateService(this);
    connect(updateService_, &UpdateService::checkFinished, this, &LauncherWindow::handleUpdateCheck);
    connect(updateService_, &UpdateService::menuDownloadFinished, this, &LauncherWindow::handleMenuDownload);
    connect(updateService_, &UpdateService::menuDownloadProgress, this, [this](qint64 received, qint64 total) {
        if (!updateStatus_ || total <= 0)
            return;
        const int percent = static_cast<int>((received * 100) / total);
        updateStatus_->setText(tr("Downloading DemoUI update... %1%").arg(percent));
    });

    buildInterface();
    applyStyle();
    detectEnvironment();

    const QString rememberedDemo = settings.value(QStringLiteral("lastDemo")).toString();
    if (QFileInfo::exists(rememberedDemo))
        setDemoPath(rememberedDemo, settings.value(QStringLiteral("lastDemoArchiveEntry")).toString());

    stateTimer_ = new QTimer(this);
    stateTimer_->setInterval(1000);
    connect(stateTimer_, &QTimer::timeout, this, &LauncherWindow::refreshState);
    stateTimer_->start();
    refreshState();
    if (qApp->property("previewAdvancedOptionsDialog").toBool())
        QTimer::singleShot(0, this, &LauncherWindow::openAdvancedLaunchOptionsDialog);
    if (qApp->property("previewUpdateBubble").toBool()) {
        QTimer::singleShot(0, this, [this]() {
            UpdateInfo preview;
            preview.valid = true;
            preview.launcher.version = QStringLiteral("0.2.0");
            preview.launcher.url = QStringLiteral("https://github.com/nicedayzhu/SwiftDemoUIPro/releases/latest");
            preview.menu.version = QStringLiteral("0.1.1");
            preview.menu.url = QStringLiteral("https://github.com/nicedayzhu/SwiftDemoUIPro/releases/latest");
            handleUpdateCheck(preview);
        });
    } else if (!qApp->property("disableAutoUpdateCheck").toBool()) {
        QTimer::singleShot(1500, this, &LauncherWindow::checkForUpdates);
    }
}

void LauncherWindow::buildInterface()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("Root"));
    auto *shell = new QHBoxLayout(central);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    auto *sidebar = new QFrame(central);
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(252);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(18, 22, 18, 18);
    sideLayout->setSpacing(8);

    auto *brand = new QHBoxLayout;
    brand->setSpacing(11);
    auto *brandLogo = new QLabel(sidebar);
    brandLogo->setPixmap(makeLogo().scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    brandLogo->setFixedSize(40, 40);
    brand->addWidget(brandLogo);
    auto *brandCopy = new QVBoxLayout;
    brandCopy->setSpacing(1);
    auto *brandTitle = new QLabel(QStringLiteral("Swift DemoUI Pro"), sidebar);
    brandTitle->setObjectName(QStringLiteral("BrandTitle"));
    auto *brandSubtitle = new QLabel(tr("CS2 Demo Tool"), sidebar);
    brandSubtitle->setObjectName(QStringLiteral("BrandSubtitle"));
    brandCopy->addWidget(brandTitle);
    brandCopy->addWidget(brandSubtitle);
    brand->addLayout(brandCopy, 1);
    sideLayout->addLayout(brand);
    sideLayout->addSpacing(22);

    auto *workspaceCaption = new QLabel(tr("Workspace"), sidebar);
    workspaceCaption->setObjectName(QStringLiteral("NavCaption"));
    sideLayout->addWidget(workspaceCaption);

    const auto makeNavButton = [sidebar](const QString &text, Glyph glyph) {
        auto *button = new QPushButton(text, sidebar);
        button->setObjectName(QStringLiteral("NavButton"));
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setIcon(makeGlyphIcon(glyph, QColor(QStringLiteral("#657080"))));
        button->setIconSize(QSize(19, 19));
        button->setMinimumHeight(44);
        return button;
    };

    navReplayButton_ = makeNavButton(tr("Demo Playback"), Glyph::Demo);
    navMenuButton_ = makeNavButton(tr("DemoUI Management"), Glyph::Menu);
    navReplayButton_->setProperty("pageIndex", 0);
    navMenuButton_->setProperty("pageIndex", 1);
    sideLayout->addWidget(navReplayButton_);
    sideLayout->addWidget(navMenuButton_);
    sideLayout->addStretch(1);

    auto *moreCaption = new QLabel(tr("More"), sidebar);
    moreCaption->setObjectName(QStringLiteral("NavCaption"));
    sideLayout->addWidget(moreCaption);
    navAboutButton_ = makeNavButton(tr("About"), Glyph::About);
    navAboutButton_->setProperty("pageIndex", 2);
    sideLayout->addWidget(navAboutButton_);

    auto *languageCaption = new QLabel(tr("Interface language"), sidebar);
    languageCaption->setObjectName(QStringLiteral("NavCaption"));
    sideLayout->addSpacing(10);
    sideLayout->addWidget(languageCaption);
    languageCombo_ = new QComboBox(sidebar);
    languageCombo_->setObjectName(QStringLiteral("LanguageCombo"));
    languageCombo_->addItem(tr("System default"), QStringLiteral("system"));
    languageCombo_->addItem(QStringLiteral("English"), QStringLiteral("en"));

    const QDir translationsDir(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations")));
    const QStringList translationFiles = translationsDir.entryList({QStringLiteral("swift_demoui_pro_*.qm")}, QDir::Files);
    for (const QString &fileName : translationFiles) {
        QString localeName = fileName;
        localeName.remove(QStringLiteral("swift_demoui_pro_"));
        localeName.chop(3);
        if (languageCombo_->findData(localeName) >= 0)
            continue;
        QLocale locale(localeName);
        QString displayName = locale.nativeLanguageName();
        if (displayName.isEmpty())
            displayName = localeName;
        else
            displayName[0] = displayName[0].toUpper();
        languageCombo_->addItem(displayName, localeName);
    }
    const int languageIndex = languageCombo_->findData(currentLanguage_);
    languageCombo_->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);
    connect(languageCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        changeLanguage(languageCombo_->itemData(index).toString());
    });
    sideLayout->addWidget(languageCombo_);

    auto *version = new QLabel(tr("Swift DemoUI Pro · %1").arg(qApp->applicationVersion()), sidebar);
    version->setObjectName(QStringLiteral("SidebarFooter"));
    version->setAlignment(Qt::AlignCenter);
    sideLayout->addSpacing(8);
    sideLayout->addWidget(version);
    shell->addWidget(sidebar);

    pages_ = new QStackedWidget(central);
    pages_->setObjectName(QStringLiteral("PageStack"));
    shell->addWidget(pages_, 1);

    auto *replayPage = new QWidget(pages_);
    replayPage->setObjectName(QStringLiteral("Page"));
    auto *replay = new QVBoxLayout(replayPage);
    replay->setContentsMargins(36, 30, 36, 24);
    replay->setSpacing(18);

    auto *replayHeader = new QHBoxLayout;
    auto *replayTitleCopy = new QVBoxLayout;
    replayTitleCopy->setSpacing(3);
    auto *replayTitle = new QLabel(tr("Demo Playback"), replayPage);
    replayTitle->setObjectName(QStringLiteral("PageTitle"));
    auto *replaySubtitle = new QLabel(tr("Choose a Demo, ZIP, or Zstandard file and start CS2 with a safe, reversible setup"), replayPage);
    replaySubtitle->setObjectName(QStringLiteral("PageSubtitle"));
    replaySubtitle->setWordWrap(true);
    replaySubtitle->setMaximumWidth(480);
    replaySubtitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    replayTitleCopy->addWidget(replayTitle);
    replayTitleCopy->addWidget(replaySubtitle);
    replayHeader->addLayout(replayTitleCopy, 1);

    securityBadge_ = new QLabel(tr("Checking"), replayPage);
    securityBadge_->setObjectName(QStringLiteral("SecurityBadge"));
    securityBadge_->setProperty("state", QStringLiteral("neutral"));
    securityBadge_->setAlignment(Qt::AlignCenter);
    securityBadge_->setMinimumWidth(116);
    securityBadge_->setFixedHeight(32);
    replayHeader->addWidget(securityBadge_, 0, Qt::AlignVCenter);
    replay->addLayout(replayHeader);

    warningCard_ = new QFrame(replayPage);
    warningCard_->setObjectName(QStringLiteral("WarningCard"));
    warningCard_->setProperty("state", QStringLiteral("ready"));
    auto *warningLayout = new QHBoxLayout(warningCard_);
    warningLayout->setContentsMargins(16, 12, 18, 12);
    warningLayout->setSpacing(12);
    warningIcon_ = new QLabel(tr("✓"), warningCard_);
    warningIcon_->setObjectName(QStringLiteral("WarningIcon"));
    warningIcon_->setAlignment(Qt::AlignCenter);
    warningIcon_->setFixedSize(28, 28);
    warningLayout->addWidget(warningIcon_);
    auto *warningCopy = new QVBoxLayout;
    warningCopy->setSpacing(3);
    warningTitle_ = new QLabel(warningCard_);
    warningTitle_->setObjectName(QStringLiteral("WarningTitle"));
    warningDetail_ = new QLabel(warningCard_);
    warningDetail_->setObjectName(QStringLiteral("WarningDetail"));
    warningDetail_->setWordWrap(true);
    warningCopy->addWidget(warningTitle_);
    warningCopy->addWidget(warningDetail_);
    warningLayout->addLayout(warningCopy, 1);
    replay->addWidget(warningCard_);

    dropCard_ = new QFrame(replayPage);
    dropCard_->setObjectName(QStringLiteral("DropCard"));
    auto *dropLayout = new QHBoxLayout(dropCard_);
    dropLayout->setContentsMargins(18, 17, 18, 17);
    dropLayout->setSpacing(16);
    auto *fileIcon = new QLabel(dropCard_);
    fileIcon->setObjectName(QStringLiteral("DemoIcon"));
    fileIcon->setPixmap(makeGlyph(Glyph::Demo, 30, QColor(QStringLiteral("#438ee6"))));
    fileIcon->setAlignment(Qt::AlignCenter);
    fileIcon->setFixedSize(50, 50);
    dropLayout->addWidget(fileIcon);
    auto *fileCopy = new QVBoxLayout;
    fileCopy->setSpacing(3);
    fileCopy->addWidget(sectionEyebrow(tr("Choose Demo"), dropCard_));
    demoName_ = new QLabel(tr("Drop a Demo, ZIP, or Zstandard file here"), dropCard_);
    demoName_->setObjectName(QStringLiteral("PrimaryText"));
    demoMeta_ = new QLabel(tr("Supports CS2 .dem, .zip, and .dem.zst files"), dropCard_);
    demoMeta_->setObjectName(QStringLiteral("SecondaryText"));
    fileCopy->addWidget(demoName_);
    fileCopy->addWidget(demoMeta_);
    dropLayout->addLayout(fileCopy, 1);
    chooseDemoButton_ = new QPushButton(tr("Browse…"), dropCard_);
    chooseDemoButton_->setObjectName(QStringLiteral("SecondaryButton"));
    chooseDemoButton_->setIcon(makeGlyphIcon(Glyph::Folder, QColor(QStringLiteral("#5d6775"))));
    chooseDemoButton_->setIconSize(QSize(18, 18));
    connect(chooseDemoButton_, &QPushButton::clicked, this, &LauncherWindow::chooseDemo);
    dropLayout->addWidget(chooseDemoButton_);
    replay->addWidget(dropCard_);

    auto *actionCard = new QFrame(replayPage);
    actionCard->setObjectName(QStringLiteral("Card"));
    auto *actions = new QVBoxLayout(actionCard);
    actions->setContentsMargins(18, 17, 18, 18);
    actions->setSpacing(12);
    actions->addWidget(sectionEyebrow(tr("Playback controls"), actionCard));
    statusLabel_ = new QLabel(tr("Checking the environment..."), actionCard);
    statusLabel_->setObjectName(QStringLiteral("StatusText"));
    statusLabel_->setProperty("state", QStringLiteral("neutral"));
    statusLabel_->setWordWrap(true);
    actions->addWidget(statusLabel_);
    preparationProgress_ = new QProgressBar(actionCard);
    preparationProgress_->setObjectName(QStringLiteral("PreparationProgress"));
    preparationProgress_->setRange(0, 0);
    preparationProgress_->setTextVisible(false);
    preparationProgress_->setFixedHeight(4);
    preparationProgress_->hide();
    actions->addWidget(preparationProgress_);

    auto *trueViewRow = new QFrame(actionCard);
    trueViewRow->setObjectName(QStringLiteral("SettingsRow"));
    trueViewRow->setMinimumHeight(68);
    auto *trueViewLayout = new QHBoxLayout(trueViewRow);
    trueViewLayout->setContentsMargins(14, 10, 12, 10);
    trueViewLayout->setSpacing(14);
    auto *trueViewCopy = new QVBoxLayout;
    trueViewCopy->setSpacing(2);
    auto *trueViewTitle = new QLabel(tr("TrueView prediction"), trueViewRow);
    trueViewTitle->setObjectName(QStringLiteral("OptionTitle"));
    auto *trueViewDetail = new QLabel(tr("Off by default to prevent flicker in Demos without TrueView command data"), trueViewRow);
    trueViewDetail->setObjectName(QStringLiteral("SecondaryText"));
    trueViewDetail->setWordWrap(true);
    trueViewCopy->addWidget(trueViewTitle);
    trueViewCopy->addWidget(trueViewDetail);
    trueViewLayout->addLayout(trueViewCopy, 1);
    trueViewCheckBox_ = new QCheckBox(trueViewEnabled_ ? tr("Enabled") : tr("Disabled"), trueViewRow);
    trueViewCheckBox_->setObjectName(QStringLiteral("TrueViewCheckBox"));
    trueViewCheckBox_->setChecked(trueViewEnabled_);
    connect(trueViewCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        trueViewEnabled_ = checked;
        trueViewCheckBox_->setText(checked ? tr("Enabled") : tr("Disabled"));
        QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
        settings.setValue(QStringLiteral("trueViewEnabled"), checked);
    });
    trueViewLayout->addWidget(trueViewCheckBox_, 0, Qt::AlignVCenter);
    actions->addWidget(trueViewRow);

    auto *advancedOptionsSection = new QWidget(actionCard);
    advancedOptionsSection->setObjectName(QStringLiteral("AdvancedOptionsSection"));
    auto *advancedOptionsLayout = new QVBoxLayout(advancedOptionsSection);
    advancedOptionsLayout->setContentsMargins(0, 0, 0, 0);
    advancedOptionsLayout->setSpacing(0);

    auto *launchOptionsHeader = new ClickableDisclosureRow(advancedOptionsSection);
    launchOptionsHeader->setObjectName(QStringLiteral("AdvancedOptionsDisclosure"));
    launchOptionsHeader->setFixedHeight(30);
    launchOptionsHeader->setActivatedHandler([this]() { openAdvancedLaunchOptionsDialog(); });
    advancedOptionsDisclosure_ = launchOptionsHeader;
    auto *launchOptionsHeaderLayout = new QHBoxLayout(launchOptionsHeader);
    launchOptionsHeaderLayout->setContentsMargins(4, 0, 4, 0);
    launchOptionsHeaderLayout->setSpacing(10);
    auto *launchOptionsTitle = new QLabel(tr("Advanced launch options"), launchOptionsHeader);
    launchOptionsTitle->setObjectName(QStringLiteral("AdvancedOptionsTitle"));
    launchOptionsTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    launchOptionsHeaderLayout->addWidget(launchOptionsTitle, 0, Qt::AlignVCenter);
    advancedOptionsSummary_ = new QLabel(launchOptionsHeader);
    advancedOptionsSummary_->setObjectName(QStringLiteral("AdvancedOptionsSummary"));
    advancedOptionsSummary_->setAttribute(Qt::WA_TransparentForMouseEvents);
    launchOptionsHeaderLayout->addWidget(advancedOptionsSummary_, 0, Qt::AlignVCenter);
    launchOptionsHeaderLayout->addStretch(1);
    advancedOptionsLayout->addWidget(launchOptionsHeader);
    actions->addWidget(advancedOptionsSection);
    refreshAdvancedOptionsUi();

    auto *actionRow = new QHBoxLayout;
    actionRow->setSpacing(10);
    startButton_ = new QPushButton(tr("Start playback"), actionCard);
    startButton_->setObjectName(QStringLiteral("PrimaryButton"));
    startButton_->setIcon(makeGlyphIcon(Glyph::Play, QColor(QStringLiteral("#ffffff"))));
    startButton_->setIconSize(QSize(18, 18));
    startButton_->setMinimumHeight(50);
    connect(startButton_, &QPushButton::clicked, this, &LauncherWindow::startWatchingDemo);
    actionRow->addWidget(startButton_, 3);
    stopButton_ = new QPushButton(tr("Stop and restore"), actionCard);
    stopButton_->setObjectName(QStringLiteral("StopButton"));
    stopButton_->setIcon(makeGlyphIcon(Glyph::Stop, QColor(QStringLiteral("#657080"))));
    stopButton_->setIconSize(QSize(17, 17));
    stopButton_->setMinimumHeight(50);
    connect(stopButton_, &QPushButton::clicked, this, &LauncherWindow::stopWatchingDemo);
    actionRow->addWidget(stopButton_, 2);
    actions->addLayout(actionRow);
    replay->addWidget(actionCard);
    replay->addStretch(1);

    auto *replayFooter = new QLabel(tr("-insecure is used for this session only  ·  Steam launch options are not changed"), replayPage);
    replayFooter->setObjectName(QStringLiteral("FooterText"));
    replayFooter->setAlignment(Qt::AlignCenter);
    replay->addWidget(replayFooter);
    pages_->addWidget(replayPage);

    auto *menuPage = new QWidget(pages_);
    menuPage->setObjectName(QStringLiteral("Page"));
    auto *menu = new QVBoxLayout(menuPage);
    menu->setContentsMargins(36, 30, 36, 24);
    menu->setSpacing(18);

    auto *menuTitle = new QLabel(tr("DemoUI Management"), menuPage);
    menuTitle->setObjectName(QStringLiteral("PageTitle"));
    auto *menuSubtitle = new QLabel(tr("Manage the CS2 path and Swift DemoUI component"), menuPage);
    menuSubtitle->setObjectName(QStringLiteral("PageSubtitle"));
    menu->addWidget(menuTitle);
    menu->addWidget(menuSubtitle);
    menu->addSpacing(4);

    auto *environmentCard = new QFrame(menuPage);
    environmentCard->setObjectName(QStringLiteral("Card"));
    auto *environment = new QVBoxLayout(environmentCard);
    environment->setContentsMargins(18, 17, 18, 18);
    environment->setSpacing(10);
    environment->addWidget(sectionEyebrow(tr("Game environment"), environmentCard));

    auto *pathRow = new QFrame(environmentCard);
    pathRow->setObjectName(QStringLiteral("SettingsRow"));
    pathRow->setMinimumHeight(68);
    auto *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(14, 11, 10, 11);
    pathLayout->setSpacing(14);
    auto *pathCopy = new QVBoxLayout;
    pathCopy->setSpacing(2);
    auto *pathCaption = new QLabel(tr("CS2 path"), pathRow);
    pathCaption->setObjectName(QStringLiteral("FieldLabel"));
    pathCopy->addWidget(pathCaption);
    cs2Path_ = new QLabel(tr("Detecting..."), pathRow);
    cs2Path_->setObjectName(QStringLiteral("PathText"));
    cs2Path_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathCopy->addWidget(cs2Path_);
    pathLayout->addLayout(pathCopy, 1);
    chooseCs2Button_ = new QPushButton(tr("Change"), pathRow);
    chooseCs2Button_->setObjectName(QStringLiteral("GhostButton"));
    connect(chooseCs2Button_, &QPushButton::clicked, this, &LauncherWindow::chooseCs2Directory);
    pathLayout->addWidget(chooseCs2Button_);
    environment->addWidget(pathRow);

    auto *vpkRow = new QFrame(environmentCard);
    vpkRow->setObjectName(QStringLiteral("SettingsRow"));
    vpkRow->setMinimumHeight(68);
    auto *vpkLayout = new QHBoxLayout(vpkRow);
    vpkLayout->setContentsMargins(14, 11, 10, 11);
    vpkLayout->setSpacing(14);
    auto *vpkCopy = new QVBoxLayout;
    vpkCopy->setSpacing(2);
    auto *vpkCaption = new QLabel(tr("DemoUI component"), vpkRow);
    vpkCaption->setObjectName(QStringLiteral("FieldLabel"));
    vpkCopy->addWidget(vpkCaption);
    vpkStatus_ = new QLabel(tr("Checking..."), vpkRow);
    vpkStatus_->setObjectName(QStringLiteral("VpkStatus"));
    vpkCopy->addWidget(vpkStatus_);
    vpkLayout->addLayout(vpkCopy, 1);
    installButton_ = new QPushButton(tr("Install / Repair"), vpkRow);
    installButton_->setObjectName(QStringLiteral("SecondaryButton"));
    connect(installButton_, &QPushButton::clicked, this, &LauncherWindow::installOrRepairMenu);
    vpkLayout->addWidget(installButton_);
    environment->addWidget(vpkRow);
    menu->addWidget(environmentCard);

    auto *managementInfo = new QFrame(menuPage);
    managementInfo->setObjectName(QStringLiteral("InfoCard"));
    auto *infoLayout = new QVBoxLayout(managementInfo);
    infoLayout->setContentsMargins(18, 16, 18, 17);
    infoLayout->setSpacing(8);
    auto *infoTitle = new QLabel(tr("Handled automatically"), managementInfo);
    infoTitle->setObjectName(QStringLiteral("InfoTitle"));
    infoLayout->addWidget(infoTitle);
    const QStringList details = {
        tr("Install or repair the DemoUI component automatically when starting a Demo"),
        tr("Remove the temporary Demo, configuration, and DemoUI override after playback"),
        tr("Never writes or changes permanent Steam launch options")
    };
    for (const QString &detail : details) {
        auto *label = new QLabel(tr("•  %1").arg(detail), managementInfo);
        label->setObjectName(QStringLiteral("InfoText"));
        infoLayout->addWidget(label);
    }
    menu->addWidget(managementInfo);
    menu->addStretch(1);
    pages_->addWidget(menuPage);

    auto *aboutPage = new QWidget(pages_);
    aboutPage->setObjectName(QStringLiteral("Page"));
    auto *about = new QVBoxLayout(aboutPage);
    about->setContentsMargins(36, 28, 36, 22);
    about->setSpacing(14);

    auto *aboutHeader = new QVBoxLayout;
    aboutHeader->setSpacing(3);
    auto *aboutTitle = new QLabel(tr("About"), aboutPage);
    aboutTitle->setObjectName(QStringLiteral("PageTitle"));
    auto *aboutSubtitle = new QLabel(
        tr("Project overview, release status, official channels, and ways to support"),
        aboutPage);
    aboutSubtitle->setObjectName(QStringLiteral("PageSubtitle"));
    aboutHeader->addWidget(aboutTitle);
    aboutHeader->addWidget(aboutSubtitle);
    about->addLayout(aboutHeader);

    auto *aboutStage = new QFrame(aboutPage);
    aboutStage->setObjectName(QStringLiteral("AboutStage"));
    aboutStage->setMinimumHeight(176);
    aboutStage->setToolTip(
        tr("Build %1 · Qt %2")
            .arg(qApp->property("gitCommit").toString(), QString::fromLatin1(qVersion())));
    auto *stage = new QVBoxLayout(aboutStage);
    stage->setContentsMargins(22, 17, 22, 15);
    stage->setSpacing(10);

    auto *stageTop = new QHBoxLayout;
    stageTop->setSpacing(24);
    auto *story = new QVBoxLayout;
    story->setSpacing(0);
    story->addStretch(1);
    auto *brandRow = new QHBoxLayout;
    brandRow->setSpacing(13);
    auto *stageLogo = new QLabel(aboutStage);
    stageLogo->setPixmap(makeLogo().scaled(52, 52, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    stageLogo->setFixedSize(52, 52);
    brandRow->addWidget(stageLogo);
    auto *stageBrandCopy = new QVBoxLayout;
    stageBrandCopy->setSpacing(4);
    auto *stageProduct = new QLabel(QStringLiteral("Swift DemoUI Pro"), aboutStage);
    stageProduct->setObjectName(QStringLiteral("StageProduct"));
    auto *stageDescription = new QLabel(tr("Counter-Strike 2 DemoUI and playback tool"), aboutStage);
    stageDescription->setObjectName(QStringLiteral("StageDescription"));
    const QString gitCommit = qApp->property("gitCommit").toString();
    auto *stageVersion = new QLabel(
        (gitCommit.isEmpty() || gitCommit == QStringLiteral("unknown"))
            ? tr("Launcher %1 · DemoUI %2")
                  .arg(qApp->applicationVersion(), UpdateService::currentMenuVersion())
            : tr("Launcher %1 (%2) · DemoUI %3")
                  .arg(qApp->applicationVersion(), gitCommit, UpdateService::currentMenuVersion()),
        aboutStage);
    stageVersion->setObjectName(QStringLiteral("StageVersion"));
    stageBrandCopy->addWidget(stageProduct);
    stageBrandCopy->addWidget(stageDescription);
    stageBrandCopy->addWidget(stageVersion);
    brandRow->addLayout(stageBrandCopy);
    brandRow->addStretch(1);
    story->addLayout(brandRow);
    story->addStretch(1);
    stageTop->addLayout(story, 1);

    auto *creatorPanel = new QFrame(aboutStage);
    creatorPanel->setObjectName(QStringLiteral("CreatorIdentity"));
    creatorPanel->setMinimumWidth(238);
    creatorPanel->setMaximumWidth(260);
    auto *creator = new QHBoxLayout(creatorPanel);
    creator->setContentsMargins(22, 0, 0, 0);
    creator->setSpacing(12);
    auto *creatorIdentity = new QHBoxLayout;
    creatorIdentity->setSpacing(12);
    auto *avatar = new QLabel(creatorPanel);
    const QPixmap avatarSource(QStringLiteral(":/images/author_avatar.bmp"));
    avatar->setPixmap(avatarSource.isNull()
            ? makeAuthorFallback(58)
            : makeCircularPixmap(avatarSource, 58));
    avatar->setFixedSize(58, 58);
    creatorIdentity->addWidget(avatar, 0, Qt::AlignVCenter);
    auto *identityCopy = new QVBoxLayout;
    identityCopy->setSpacing(4);
    identityCopy->addStretch(1);
    auto *creatorCaption = new QLabel(tr("CREATOR"), creatorPanel);
    creatorCaption->setObjectName(QStringLiteral("CreatorCaption"));
    identityCopy->addWidget(creatorCaption);
    auto *creatorName = new QLabel(QStringLiteral("nicedayzhu"), creatorPanel);
    creatorName->setObjectName(QStringLiteral("CreatorName"));
    identityCopy->addWidget(creatorName);
    identityCopy->addStretch(1);
    creatorIdentity->addLayout(identityCopy, 1);
    creator->addLayout(creatorIdentity, 1);
    stageTop->addWidget(creatorPanel);
    stage->addLayout(stageTop, 1);

    auto *stageDivider = new QFrame(aboutStage);
    stageDivider->setObjectName(QStringLiteral("StageDivider"));
    stageDivider->setFrameShape(QFrame::HLine);
    stage->addWidget(stageDivider);
    auto *updateRow = new QHBoxLayout;
    updateRow->setSpacing(8);
    updateStatus_ = new QLabel(aboutStage);
    updateStatus_->setObjectName(QStringLiteral("StageUpdateStatus"));
    updateStatus_->setProperty("state", QStringLiteral("neutral"));
    updateStatus_->setWordWrap(false);
    updateStatus_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    updateStatus_->setMaximumWidth(460);
    updateStatus_->setFixedHeight(28);
    updateRow->addWidget(updateStatus_, 0, Qt::AlignVCenter);
    updateRow->addStretch(1);
    checkUpdateButton_ = new QPushButton(tr("Check for updates"), aboutStage);
    checkUpdateButton_->setObjectName(QStringLiteral("StageUpdateButton"));
    connect(checkUpdateButton_, &QPushButton::clicked, this, &LauncherWindow::checkForUpdates);
    updateRow->addWidget(checkUpdateButton_);
    launcherUpdateButton_ = new QPushButton(aboutStage);
    launcherUpdateButton_->setObjectName(QStringLiteral("StageUpdateButton"));
    connect(launcherUpdateButton_, &QPushButton::clicked, this, [this]() {
        const QString url = updateInfo_.launcher.url.isEmpty()
            ? updateInfo_.releasePageUrl
            : updateInfo_.launcher.url;
        if (!url.isEmpty())
            QDesktopServices::openUrl(QUrl(url));
    });
    updateRow->addWidget(launcherUpdateButton_);
    menuUpdateButton_ = new QPushButton(aboutStage);
    menuUpdateButton_->setObjectName(QStringLiteral("StageUpdateButton"));
    connect(menuUpdateButton_, &QPushButton::clicked, this, &LauncherWindow::downloadMenuUpdate);
    updateRow->addWidget(menuUpdateButton_);
    stage->addLayout(updateRow);
    about->addWidget(aboutStage);
    refreshUpdateUi();

    auto *linksHeader = new QHBoxLayout;
    auto *linksTitle = new QLabel(tr("Official links"), aboutPage);
    linksTitle->setObjectName(QStringLiteral("LinksTitle"));
    linksHeader->addWidget(linksTitle);
    linksHeader->addStretch(1);
    about->addLayout(linksHeader);

    const auto makeLinkButton = [aboutPage](
                                    const QString &title,
                                    const QString &detail,
                                    const QString &url,
                                    Glyph glyph,
                                    const QString &accent,
                                    const QColor &iconColor) {
        auto *button = new LinkButton(aboutPage);
        button->setObjectName(QStringLiteral("LinkButton"));
        button->setProperty("accent", accent);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setAccessibleName(title);
        button->setAccessibleDescription(detail);
        button->setToolTip(url);
        button->setFixedHeight(88);

        auto *content = new QHBoxLayout(button);
        content->setContentsMargins(14, 12, 12, 12);
        content->setSpacing(10);
        auto *icon = new QLabel(button);
        icon->setObjectName(QStringLiteral("LinkIcon"));
        icon->setProperty("accent", accent);
        icon->setPixmap(makeGlyph(glyph, 20, iconColor));
        icon->setAlignment(Qt::AlignCenter);
        icon->setFixedSize(36, 36);
        icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        content->addWidget(icon, 0, Qt::AlignTop);

        auto *copy = new QVBoxLayout;
        copy->setSpacing(3);
        auto *linkTitle = new QLabel(title, button);
        linkTitle->setObjectName(QStringLiteral("LinkTitle"));
        linkTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *linkDetail = new QLabel(detail, button);
        linkDetail->setObjectName(QStringLiteral("LinkDescription"));
        linkDetail->setWordWrap(true);
        linkDetail->setAttribute(Qt::WA_TransparentForMouseEvents);
        copy->addWidget(linkTitle);
        copy->addWidget(linkDetail);
        copy->addStretch(1);
        content->addLayout(copy, 1);

        auto *external = new QLabel(button);
        external->setPixmap(makeGlyph(
            Glyph::External,
            15,
            QColor(QStringLiteral("#8b95a3"))));
        external->setFixedSize(18, 18);
        external->setAttribute(Qt::WA_TransparentForMouseEvents);
        content->addWidget(external, 0, Qt::AlignTop);
        QObject::connect(button, &QPushButton::clicked, button, [url]() {
            QDesktopServices::openUrl(QUrl(url));
        });
        return button;
    };

    auto *linkCards = new QHBoxLayout;
    linkCards->setSpacing(10);
    linkCards->addWidget(makeLinkButton(
        tr("Official website"),
        tr("Downloads, guides, and updates"),
        QStringLiteral("https://nicedayzhu.github.io/SwiftDemoUIPro/"),
        Glyph::Website,
        QStringLiteral("website"),
        QColor(QStringLiteral("#347fd8"))), 1);
    linkCards->addWidget(makeLinkButton(
        QStringLiteral("GitHub"),
        tr("Source code and issues"),
        QStringLiteral("https://github.com/nicedayzhu/SwiftDemoUIPro"),
        Glyph::Code,
        QStringLiteral("github"),
        QColor(QStringLiteral("#31363d"))), 1);
    linkCards->addWidget(makeLinkButton(
        tr("Bilibili"),
        tr("Videos and tutorials"),
        QStringLiteral("https://space.bilibili.com/1405728"),
        Glyph::Video,
        QStringLiteral("bilibili"),
        QColor(QStringLiteral("#e85b87"))), 1);
    about->addLayout(linkCards);

    auto *supportCard = new QFrame(aboutPage);
    supportCard->setObjectName(QStringLiteral("SupportCard"));
    supportCard->setMinimumHeight(84);
    auto *support = new QHBoxLayout(supportCard);
    support->setContentsMargins(16, 12, 14, 12);
    support->setSpacing(12);

    auto *supportMark = new QLabel(QStringLiteral("♥"), supportCard);
    supportMark->setObjectName(QStringLiteral("SupportMark"));
    supportMark->setAlignment(Qt::AlignCenter);
    supportMark->setFixedSize(40, 40);
    supportMark->setAttribute(Qt::WA_TransparentForMouseEvents);
    support->addWidget(supportMark, 0, Qt::AlignVCenter);

    const QString supportDescriptionText =
        tr("Help fund continued development and maintenance");
    auto *supportCopy = new QVBoxLayout;
    supportCopy->setSpacing(3);
    auto *supportTitle = new QLabel(tr("Support the project"), supportCard);
    supportTitle->setObjectName(QStringLiteral("SupportTitle"));
    auto *supportDescription = new QLabel(supportDescriptionText, supportCard);
    supportDescription->setObjectName(QStringLiteral("SupportDescription"));
    supportDescription->setWordWrap(true);
    supportCopy->addWidget(supportTitle);
    supportCopy->addWidget(supportDescription);
    support->addLayout(supportCopy, 1);

    const QPixmap afdianIconImage(QStringLiteral(":/images/afdian_icon.png"));
    const QPixmap kofiIconImage(QStringLiteral(":/images/kofi_icon.png"));
    const auto makeSupportButton = [supportCard, supportDescriptionText](
                                       const QString &text,
                                       const QString &accessibleName,
                                       const QString &url,
                                       const QPixmap &iconImage,
                                       const QString &provider,
                                       const QColor &fallbackColor) {
        auto *button = new QPushButton(text, supportCard);
        button->setObjectName(QStringLiteral("SupportPlatformButton"));
        button->setProperty("provider", provider);
        button->setAccessibleName(accessibleName);
        button->setAccessibleDescription(supportDescriptionText);
        button->setToolTip(accessibleName);
        button->setIcon(iconImage.isNull()
                ? makeGlyphIcon(Glyph::Coffee, fallbackColor)
                : QIcon(iconImage));
        button->setIconSize(QSize(24, 24));
        button->setFixedSize(126, 44);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::StrongFocus);
        QObject::connect(button, &QPushButton::clicked, button, [url]() {
            QDesktopServices::openUrl(QUrl(url));
        });
        return button;
    };

    auto *supportActions = new QHBoxLayout;
    supportActions->setSpacing(8);
    supportActions->addWidget(makeSupportButton(
        tr("Afdian"),
        tr("Support on Afdian"),
        QStringLiteral("https://afdian.com/a/nicedayzhu"),
        afdianIconImage,
        QStringLiteral("afdian"),
        QColor(QStringLiteral("#946ce6"))));
    supportActions->addWidget(makeSupportButton(
        QStringLiteral("Ko-fi"),
        tr("Support on Ko-fi"),
        QStringLiteral("https://ko-fi.com/K6C623WHCQ"),
        kofiIconImage,
        QStringLiteral("kofi"),
        QColor(QStringLiteral("#ff5e3a"))));
    support->addLayout(supportActions);
    about->addWidget(supportCard);
    about->addStretch(1);
    pages_->addWidget(aboutPage);

    connect(navReplayButton_, &QPushButton::clicked, this, [this]() { selectPage(0); });
    connect(navMenuButton_, &QPushButton::clicked, this, [this]() { selectPage(1); });
    connect(navAboutButton_, &QPushButton::clicked, this, [this]() { selectPage(2); });
    selectPage(0);

    updateBubble_ = new QFrame(central);
    updateBubble_->setObjectName(QStringLiteral("UpdateBubble"));
    updateBubble_->setFixedWidth(330);
    auto *bubbleLayout = new QHBoxLayout(updateBubble_);
    bubbleLayout->setContentsMargins(14, 11, 9, 11);
    bubbleLayout->setSpacing(9);
    updateBubbleText_ = new QLabel(updateBubble_);
    updateBubbleText_->setObjectName(QStringLiteral("UpdateBubbleText"));
    updateBubbleText_->setWordWrap(true);
    bubbleLayout->addWidget(updateBubbleText_, 1);
    auto *viewUpdateButton = new QPushButton(tr("View"), updateBubble_);
    viewUpdateButton->setObjectName(QStringLiteral("BubbleAction"));
    connect(viewUpdateButton, &QPushButton::clicked, this, [this]() {
        updateBubbleDismissed_ = true;
        updateBubble_->hide();
        selectPage(2);
    });
    bubbleLayout->addWidget(viewUpdateButton);
    auto *dismissUpdateButton = new QPushButton(QStringLiteral("×"), updateBubble_);
    dismissUpdateButton->setObjectName(QStringLiteral("BubbleClose"));
    dismissUpdateButton->setToolTip(tr("Dismiss"));
    connect(dismissUpdateButton, &QPushButton::clicked, this, [this]() {
        updateBubbleDismissed_ = true;
        updateBubble_->hide();
    });
    bubbleLayout->addWidget(dismissUpdateButton);
    updateBubble_->hide();

    setCentralWidget(central);
}

void LauncherWindow::applyStyle()
{
    const QString baseStyle = QStringLiteral(R"CSS(
        QWidget#Root {
            background: #f5f7fa;
            color: #1a1d23;
            font-family: "Noto Sans SC";
            font-size: 14px;
        }
        QFrame#Sidebar {
            background: #f6f7f9;
            border: none;
            border-right: 1px solid #e3e6eb;
        }
        QLabel#BrandTitle {
            color: #171a20;
            font-size: 16px;
            font-weight: 650;
        }
        QLabel#BrandSubtitle {
            color: #8a929e;
            font-size: 11px;
        }
        QLabel#NavCaption {
            color: #9aa1ac;
            font-size: 11px;
            font-weight: 600;
            padding: 4px 10px;
        }
        QPushButton#NavButton {
            min-height: 44px;
            border: none;
            border-radius: 9px;
            background: transparent;
            color: #596270;
            text-align: left;
            padding: 0 13px;
            font-size: 14px;
            font-weight: 500;
        }
        QPushButton#NavButton:hover {
            background: #eceff3;
            color: #262b33;
        }
        QPushButton#NavButton:checked {
            background: #e5f0ff;
            color: #1769c2;
            font-weight: 650;
        }
        QLabel#SidebarFooter {
            color: #a3a9b2;
            font-size: 10px;
        }
        QComboBox#LanguageCombo {
            min-height: 36px;
            border: 1px solid #dde1e7;
            border-radius: 8px;
            background: #ffffff;
            color: #4f5865;
            padding: 0 10px;
            font-size: 12px;
        }
        QComboBox#LanguageCombo:hover { border-color: #b8c0cb; }
        QComboBox#LanguageCombo::drop-down {
            width: 24px;
            border: none;
        }
        QComboBox#LanguageCombo QAbstractItemView {
            border: 1px solid #dde1e7;
            background: #ffffff;
            color: #303640;
            selection-background-color: #e5f0ff;
            selection-color: #1769c2;
            outline: none;
        }
        QStackedWidget#PageStack {
            border: none;
            background: #ffffff;
        }
        QWidget#Page {
            background: #ffffff;
        }
        QLabel#PageTitle {
            color: #171a20;
            font-size: 26px;
            font-weight: 650;
        }
        QLabel#PageSubtitle {
            color: #7a838f;
            font-size: 13px;
        }
        QLabel#SectionEyebrow {
            color: #242831;
            font-size: 14px;
            font-weight: 650;
        }
        QLabel#SecurityBadge {
            border: none;
            border-radius: 16px;
            background: #edf0f3;
            color: #68717e;
            font-size: 12px;
            font-weight: 600;
            padding: 0 13px;
        }
        QLabel#SecurityBadge[state="safe"] {
            background: #e7f7ef;
            color: #178a58;
        }
        QLabel#SecurityBadge[state="active"] {
            background: #fff2d7;
            color: #996615;
        }
        QLabel#SecurityBadge[state="danger"] {
            background: #ffebec;
            color: #bd3b45;
        }
        QFrame#WarningCard {
            border: none;
            border-radius: 11px;
            background: #edf8f2;
        }
        QFrame#WarningCard[state="active"] {
            background: #fff7e7;
        }
        QFrame#WarningCard[state="danger"] {
            background: #fff0f1;
        }
        QLabel#WarningIcon {
            border-radius: 14px;
            background: #45c88a;
            color: #ffffff;
            font-size: 15px;
            font-weight: 700;
        }
        QFrame#WarningCard[state="active"] QLabel#WarningIcon {
            background: #f0b44b;
        }
        QFrame#WarningCard[state="danger"] QLabel#WarningIcon {
            background: #ef6872;
        }
        QLabel#WarningTitle {
            color: #22262e;
            font-size: 14px;
            font-weight: 650;
        }
        QLabel#WarningDetail {
            color: #66707c;
            font-size: 12px;
        }
        QFrame#Card, QFrame#DropCard {
            border: 1px solid #e2e6eb;
            border-radius: 12px;
            background: #ffffff;
        }
        QFrame#DropCard {
            border: 1px dashed #cbd2dc;
            background: #fbfcfe;
        }
        QFrame#DropCard[dragging="true"] {
            border: 1px solid #438ee6;
            background: #f2f7ff;
        }
        QLabel#DemoIcon {
            border: 1px solid #d7e7fb;
            border-radius: 12px;
            background: #eef6ff;
        }
        QLabel#PrimaryText {
            color: #20242b;
            font-size: 16px;
            font-weight: 650;
        }
        QLabel#SecondaryText, QLabel#PathText {
            color: #77818e;
            font-size: 12px;
        }
        QFrame#SettingsRow {
            border: none;
            border-radius: 9px;
            background: #f6f8fa;
        }
        QLabel#FieldLabel {
            color: #7b8490;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel#OptionTitle {
            color: #4d5662;
            font-size: 13px;
            font-weight: 600;
        }
        QWidget#AdvancedOptionsSection,
        QFrame#AdvancedOptionsDisclosure {
            border: none;
            background: transparent;
        }
        QFrame#AdvancedOptionsDisclosure[keyboardFocus="true"] {
            border-radius: 5px;
            background: #f5f7f9;
        }
        QLabel#AdvancedOptionsTitle {
            color: #68727e;
            font-size: 11px;
            font-weight: 550;
        }
        QFrame#AdvancedOptionsDisclosure:hover QLabel#AdvancedOptionsTitle {
            color: #3f4854;
        }
        QLabel#AdvancedOptionsSummary {
            color: #969da7;
            font-size: 10px;
        }
        QLabel#AdvancedOptionsSummary[configured="true"] {
            color: #6f7883;
            font-weight: 550;
        }
        QFrame#AdvancedOptionsDisclosure:disabled QLabel#AdvancedOptionsTitle,
        QFrame#AdvancedOptionsDisclosure:disabled QLabel#AdvancedOptionsSummary {
            color: #b6bbc2;
        }
        QDialog#AdvancedOptionsDialog { background: #ffffff; }
        QLabel#AdvancedOptionsDialogTitle {
            color: #171a20;
            font-size: 20px;
            font-weight: 650;
        }
        QLabel#AdvancedOptionsDialogDetail,
        QLabel#AdvancedOptionsDialogHelper {
            color: #737d89;
            font-size: 12px;
        }
        QLabel#AdvancedOptionsDialogFieldLabel {
            color: #444d59;
            font-size: 12px;
            font-weight: 600;
        }
        QLabel#AdvancedOptionsDialogNotice {
            border: none;
            border-radius: 7px;
            background: #f5f7f9;
            color: #697380;
            padding: 9px 11px;
            font-size: 11px;
        }
        QLabel#AdvancedOptionsDialogError {
            color: #c74752;
            font-size: 11px;
        }
        QCheckBox#TrueViewCheckBox {
            min-width: 72px;
            border: 1px solid #d5dae1;
            border-radius: 8px;
            background: #ffffff;
            color: #4d5662;
            font-size: 12px;
            font-weight: 600;
            padding: 7px 12px;
        }
        QCheckBox#TrueViewCheckBox::indicator {
            width: 0;
            height: 0;
        }
        QCheckBox#TrueViewCheckBox:hover {
            border-color: #aeb7c3;
            background: #f7f9fb;
        }
        QCheckBox#TrueViewCheckBox:checked {
            border-color: #347fd8;
            background: #347fd8;
            color: #ffffff;
        }
        QCheckBox#TrueViewCheckBox:disabled {
            border-color: #e4e7eb;
            background: #f2f4f6;
            color: #abb1ba;
        }
        QLineEdit#LaunchOptionsEdit {
            min-height: 36px;
            border: 1px solid #dde2e8;
            border-radius: 7px;
            background: #fbfcfd;
            color: #303640;
            selection-background-color: #347fd8;
            padding: 0 11px;
            font-size: 12px;
        }
        QLineEdit#LaunchOptionsEdit:hover { border-color: #aeb7c3; }
        QLineEdit#LaunchOptionsEdit:focus { border-color: #347fd8; }
        QLineEdit#LaunchOptionsEdit[invalid="true"] {
            border-color: #d95763;
            background: #fff8f8;
        }
        QLineEdit#LaunchOptionsEdit:disabled {
            border-color: #e4e7eb;
            background: #f0f2f4;
            color: #a5abb4;
        }
        QDialog#AdvancedOptionsDialog QDialogButtonBox QPushButton {
            min-width: 82px;
            min-height: 36px;
            border: 1px solid #d5dae1;
            border-radius: 7px;
            background: #ffffff;
            color: #4d5662;
            padding: 0 14px;
            font-size: 12px;
            font-weight: 600;
        }
        QDialog#AdvancedOptionsDialog QDialogButtonBox QPushButton:hover {
            border-color: #aeb7c3;
            background: #f7f9fb;
        }
        QLabel#VpkStatus {
            color: #4d5662;
            font-size: 13px;
            font-weight: 600;
        }
        QLabel#VpkStatus[installed="true"] { color: #178a58; }
        QLabel#VpkStatus[installed="false"] { color: #a36d16; }
        QLabel#StatusText {
            border: none;
            border-radius: 8px;
            background: #f5f7f9;
            color: #66707c;
            font-size: 12px;
            padding: 10px 12px;
        }
        QLabel#StatusText[state="safe"] { color: #168557; }
        QLabel#StatusText[state="active"] { color: #966315; }
        QLabel#StatusText[state="danger"] { color: #bd3b45; }
        QProgressBar#PreparationProgress {
            border: none;
            border-radius: 2px;
            background: #e8edf4;
        }
        QProgressBar#PreparationProgress::chunk {
            border-radius: 2px;
            background: #347fd8;
        }
        QFrame#InfoCard {
            border: 1px solid #e6e9ed;
            border-radius: 12px;
            background: #f8fafc;
        }
        QLabel#InfoTitle {
            color: #252a32;
            font-size: 14px;
            font-weight: 650;
        }
        QLabel#InfoText {
            color: #68727f;
            font-size: 13px;
        }
    )CSS");

    const QString aboutStyle = QStringLiteral(R"CSS(
        QFrame#AboutStage {
            border: 1px solid #d9e6f3;
            border-radius: 18px;
            background: qlineargradient(
                x1: 0, y1: 0, x2: 1, y2: 1,
                stop: 0 #edf5fd,
                stop: 0.58 #f3f8fd,
                stop: 1 #f8fbff
            );
        }
        QLabel#StageProduct {
            color: #20252d;
            font-size: 23px;
            font-weight: 700;
        }
        QLabel#StageDescription {
            color: #69727e;
            font-size: 12px;
        }
        QLabel#StageVersion {
            color: #64778a;
            font-size: 11px;
            font-weight: 500;
        }
        QFrame#CreatorIdentity {
            border: none;
            border-left: 1px solid #d5e3f1;
            background: transparent;
        }
        QLabel#CreatorCaption {
            color: #3a7ec5;
            font-size: 10px;
            font-weight: 700;
        }
        QLabel#CreatorName {
            color: #20252d;
            font-size: 19px;
            font-weight: 700;
        }
        QFrame#StageDivider {
            border: none;
            border-top: 1px solid #d9e6f2;
            max-height: 1px;
        }
        QLabel#StageUpdateStatus {
            min-height: 28px;
            max-height: 28px;
            border: none;
            background: transparent;
            color: #6a7888;
            padding: 0;
            font-size: 11px;
            font-weight: 500;
        }
        QLabel#StageUpdateStatus[state="success"] {
            color: #526f89;
        }
        QLabel#StageUpdateStatus[state="attention"] {
            color: #89601d;
        }
        QLabel#StageUpdateStatus[state="error"] {
            color: #a34848;
        }
        QPushButton#StageUpdateButton {
            min-height: 32px;
            border: 1px solid #cbdbea;
            border-radius: 8px;
            background: #ffffff;
            color: #394552;
            padding: 0 13px;
            font-size: 11px;
            font-weight: 650;
        }
        QPushButton#StageUpdateButton:hover {
            border-color: #9fbedc;
            background: #edf6ff;
            color: #2869aa;
        }
        QPushButton#StageUpdateButton:pressed { background: #dfedfa; }
        QPushButton#StageUpdateButton:disabled {
            border-color: #d9e4ee;
            background: #eaf1f7;
            color: #9aa9b8;
        }
        QLabel#LinksTitle {
            color: #1b2028;
            font-size: 16px;
            font-weight: 650;
        }
        QPushButton#LinkButton {
            border: 1px solid #e5e9ef;
            border-radius: 14px;
            background: #f7f8fa;
            padding: 0;
            text-align: left;
        }
        QPushButton#LinkButton:hover {
            border-color: #c8d0db;
            background: #ffffff;
        }
        QPushButton#LinkButton[accent="website"]:hover {
            border-color: #b9d5f5;
            background: #f8fbff;
        }
        QPushButton#LinkButton[accent="github"]:hover {
            border-color: #c7ccd2;
            background: #fafbfc;
        }
        QPushButton#LinkButton[accent="bilibili"]:hover {
            border-color: #f1c5d4;
            background: #fff9fb;
        }
        QPushButton#LinkButton:pressed { background: #f0f3f6; }
        QPushButton#LinkButton[keyboardFocus="true"] {
            border: 2px solid #75abe2;
        }
        QLabel#LinkIcon {
            border: none;
            border-radius: 10px;
            background: #eaf3ff;
        }
        QLabel#LinkIcon[accent="github"] { background: #eceff2; }
        QLabel#LinkIcon[accent="bilibili"] { background: #fff0f5; }
        QLabel#LinkTitle {
            color: #242a33;
            font-size: 14px;
            font-weight: 650;
        }
        QLabel#LinkDescription {
            color: #7e8793;
            font-size: 11px;
        }
        QFrame#SupportCard {
            border: 1px solid #e1e6ed;
            border-radius: 14px;
            background: #f9fafc;
        }
        QLabel#SupportMark {
            border: none;
            border-radius: 10px;
            background: #eaf3ff;
            color: #438ee6;
            font-size: 18px;
            font-weight: 650;
        }
        QLabel#SupportTitle {
            color: #252a32;
            font-size: 14px;
            font-weight: 650;
        }
        QLabel#SupportDescription {
            color: #7b8490;
            font-size: 11px;
        }
        QPushButton#SupportPlatformButton {
            border: 1px solid #dce2e9;
            border-radius: 10px;
            background: #ffffff;
            color: #414954;
            text-align: left;
            padding: 0 12px;
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton#SupportPlatformButton[provider="afdian"] {
            color: #5f4a8c;
        }
        QPushButton#SupportPlatformButton[provider="afdian"]:hover {
            border-color: #bca7ea;
            background: #faf8ff;
            color: #6549a1;
        }
        QPushButton#SupportPlatformButton[provider="afdian"]:pressed {
            background: #f0eaff;
        }
        QPushButton#SupportPlatformButton[provider="kofi"]:hover {
            border-color: #ffc0b0;
            background: #fff8f5;
            color: #7b382b;
        }
        QPushButton#SupportPlatformButton[provider="kofi"]:pressed {
            background: #ffede7;
        }
        QPushButton#SupportPlatformButton:focus {
            border: 2px solid #75abe2;
        }
        QFrame#UpdateBubble {
            border: 1px solid #cfd9e7;
            border-radius: 11px;
            background: #f7fbff;
        }
        QLabel#UpdateBubbleText {
            color: #35404e;
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton#BubbleAction {
            min-width: 48px;
            min-height: 30px;
            border: none;
            border-radius: 7px;
            background: #347fd8;
            color: #ffffff;
            padding: 0 10px;
            font-size: 12px;
        }
        QPushButton#BubbleAction:hover { background: #438ee6; }
        QPushButton#BubbleClose {
            min-width: 26px;
            max-width: 26px;
            min-height: 26px;
            max-height: 26px;
            border: none;
            border-radius: 6px;
            background: transparent;
            color: #7c8795;
            padding: 0;
            font-size: 17px;
        }
        QPushButton#BubbleClose:hover { background: #e7edf5; color: #35404e; }
        QPushButton {
            min-height: 36px;
            border-radius: 8px;
            padding: 0 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#PrimaryButton {
            border: none;
            background: #347fd8;
            color: #ffffff;
            font-size: 15px;
        }
        QPushButton#PrimaryButton:hover { background: #438ee6; }
        QPushButton#PrimaryButton:pressed { background: #286fca; }
        QPushButton#StopButton,
        QPushButton#SecondaryButton,
        QPushButton#GhostButton {
            border: 1px solid #d5dae1;
            background: #ffffff;
            color: #48515e;
        }
        QPushButton#StopButton:hover,
        QPushButton#SecondaryButton:hover,
        QPushButton#GhostButton:hover {
            border-color: #aeb7c3;
            background: #f7f9fb;
            color: #242a32;
        }
        QPushButton#GhostButton { min-width: 58px; padding: 0 10px; }
        QPushButton#SupportButton {
            min-width: 116px;
            border: none;
            background: #347fd8;
            color: #ffffff;
        }
        QPushButton#SupportButton:hover { background: #438ee6; }
        QPushButton#SupportButton:pressed { background: #286fca; }
        QPushButton:disabled,
        QPushButton#PrimaryButton:disabled,
        QPushButton#StopButton:disabled,
        QPushButton#SecondaryButton:disabled,
        QPushButton#GhostButton:disabled {
            border: 1px solid #e4e7eb;
            background: #f2f4f6;
            color: #abb1ba;
        }
        QLabel#FooterText {
            color: #9ba2ac;
            font-size: 11px;
        }
        QMessageBox {
            background: #ffffff;
            color: #20242b;
        }
        QMessageBox QLabel {
            background: transparent;
            color: #20242b;
        }
        QMessageBox QLabel#qt_msgbox_label {
            color: #20242b;
            font-size: 14px;
            font-weight: 650;
        }
        QMessageBox QLabel#qt_msgbox_informativelabel {
            color: #66707c;
            font-size: 12px;
            font-weight: 400;
        }
        QMessageBox QPushButton {
            min-width: 84px;
            min-height: 34px;
            border: 1px solid #d5dae1;
            border-radius: 8px;
            background: #ffffff;
            color: #48515e;
            padding: 0 14px;
            font-size: 13px;
            font-weight: 600;
        }
        QMessageBox QPushButton:hover {
            border-color: #aeb7c3;
            background: #f7f9fb;
            color: #242a32;
        }
        QMessageBox QPushButton#DialogPrimaryButton,
        QMessageBox QPushButton:default {
            border-color: #347fd8;
            background: #347fd8;
            color: #ffffff;
        }
        QMessageBox QPushButton#DialogPrimaryButton:hover,
        QMessageBox QPushButton:default:hover {
            border-color: #438ee6;
            background: #438ee6;
        }
        QToolTip {
            border: 1px solid #d9dde3;
            background: #ffffff;
            color: #282d35;
            padding: 5px;
        }
    )CSS");
    qApp->setStyleSheet(baseStyle + aboutStyle);
}

bool LauncherWindow::loadLanguage(const QString &language)
{
    if (translator_) {
        qApp->removeTranslator(translator_);
        delete translator_;
    }
    translator_ = new QTranslator(this);

    QString effectiveLanguage = language;
    const bool followsSystem = language.compare(QStringLiteral("system"), Qt::CaseInsensitive) == 0;
    if (followsSystem)
        effectiveLanguage = QLocale::system().name();

    if (effectiveLanguage.startsWith(QStringLiteral("en"), Qt::CaseInsensitive))
        return true;

    QStringList candidates { effectiveLanguage };
    const QString languageOnly = effectiveLanguage.section(QLatin1Char('_'), 0, 0);
    if (languageOnly != effectiveLanguage)
        candidates.append(languageOnly);

    const QDir translationsDir(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations")));
    for (const QString &candidate : candidates) {
        const QString fileName = QStringLiteral("swift_demoui_pro_%1.qm").arg(candidate);
        if (translator_->load(translationsDir.filePath(fileName))) {
            qApp->installTranslator(translator_);
            return true;
        }
    }

    return followsSystem;
}

void LauncherWindow::changeLanguage(const QString &language)
{
    if (language.isEmpty() || language == currentLanguage_)
        return;

    const QString previousLanguage = currentLanguage_;
    const int previousPage = pages_ ? pages_->currentIndex() : 0;
    if (!loadLanguage(language)) {
        loadLanguage(previousLanguage);
        QMessageBox::warning(this, tr("Unable to switch language"), tr("The translation file for the selected language was not found. Reinstall the app or check the translations folder."));
        if (languageCombo_) {
            languageCombo_->blockSignals(true);
            languageCombo_->setCurrentIndex(languageCombo_->findData(previousLanguage));
            languageCombo_->blockSignals(false);
        }
        return;
    }

    currentLanguage_ = language;
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    settings.setValue(QStringLiteral("uiLanguage"), currentLanguage_);

    QWidget *oldCentral = takeCentralWidget();
    buildInterface();
    if (oldCentral)
        oldCentral->deleteLater();

    lastStatus_.clear();
    refreshDemoDetails();
    if (!demoPath_.isEmpty())
        lastStatus_ = tr("Demo selected. Start playback to install DemoUI automatically and launch CS2.");
    selectPage(previousPage);
    refreshState();
}

void LauncherWindow::checkForUpdates()
{
    if (!updateService_ || updateCheckInProgress_)
        return;
    updateCheckInProgress_ = true;
    updateBubbleDismissed_ = false;
    updateInfo_ = {};
    refreshUpdateUi();
    updateService_->checkForUpdates();
}

void LauncherWindow::downloadMenuUpdate()
{
    if (!updateService_ || !updateInfo_.menu.isNewerThan(UpdateService::currentMenuVersion()))
        return;
    if (menuUpdateButton_)
        menuUpdateButton_->setEnabled(false);
    if (checkUpdateButton_)
        checkUpdateButton_->setEnabled(false);
    if (updateStatus_)
        updateStatus_->setText(tr("Downloading DemoUI update..."));
    updateService_->downloadMenuUpdate(updateInfo_.menu);
}

void LauncherWindow::handleUpdateCheck(const UpdateInfo &info)
{
    updateCheckInProgress_ = false;
    updateInfo_ = info;
    refreshUpdateUi();

    const bool launcherAvailable = info.valid
        && info.launcher.isNewerThan(qApp->applicationVersion());
    const bool menuAvailable = info.valid
        && info.menu.isNewerThan(UpdateService::currentMenuVersion());
    if (!updateBubble_ || !updateBubbleText_)
        return;
    if ((!launcherAvailable && !menuAvailable) || updateBubbleDismissed_) {
        updateBubble_->hide();
        return;
    }
    if (launcherAvailable && menuAvailable) {
        updateBubbleText_->setText(
            tr("Updates available: launcher %1 and DemoUI %2")
                .arg(info.launcher.version, info.menu.version));
    } else if (launcherAvailable) {
        updateBubbleText_->setText(tr("Launcher %1 is available").arg(info.launcher.version));
    } else {
        updateBubbleText_->setText(tr("DemoUI %1 is available").arg(info.menu.version));
    }
    updateBubble_->adjustSize();
    positionUpdateBubble();
    updateBubble_->show();
    updateBubble_->raise();
    QFrame *bubble = updateBubble_;
    QTimer::singleShot(9000, bubble, [bubble]() { bubble->hide(); });
}

void LauncherWindow::handleMenuDownload(bool ok, const QString &message)
{
    if (ok) {
        refreshUpdateUi();
        QMessageBox::information(this, tr("DemoUI update ready"), message);
    } else {
        refreshUpdateUi();
        QMessageBox::warning(this, tr("Unable to update DemoUI"), message);
    }
}

void LauncherWindow::refreshUpdateUi()
{
    if (!updateStatus_ || !checkUpdateButton_ || !launcherUpdateButton_ || !menuUpdateButton_)
        return;

    const auto setUpdateStatus = [this](const QString &text, const QString &state) {
        updateStatus_->setProperty("state", state);
        updateStatus_->setText(text);
        updateStatus_->style()->unpolish(updateStatus_);
        updateStatus_->style()->polish(updateStatus_);
        updateStatus_->updateGeometry();
    };

    checkUpdateButton_->setEnabled(!updateCheckInProgress_);
    checkUpdateButton_->setText(updateCheckInProgress_ ? tr("Checking...") : tr("Check for updates"));
    launcherUpdateButton_->setVisible(false);
    menuUpdateButton_->setVisible(false);

    if (updateCheckInProgress_) {
        setUpdateStatus(tr("Checking the latest GitHub Release..."), QStringLiteral("neutral"));
        return;
    }

    if (!updateInfo_.valid) {
        if (!updateInfo_.error.isEmpty()) {
            setUpdateStatus(
                tr("Unable to check for updates: %1").arg(updateInfo_.error),
                QStringLiteral("error"));
        } else {
            setUpdateStatus(tr("Update status not checked"), QStringLiteral("neutral"));
        }
        return;
    }

    const bool launcherAvailable = updateInfo_.launcher.isNewerThan(qApp->applicationVersion());
    const bool menuAvailable = updateInfo_.menu.isNewerThan(UpdateService::currentMenuVersion());
    launcherUpdateButton_->setVisible(launcherAvailable);
    launcherUpdateButton_->setText(tr("Download launcher %1").arg(updateInfo_.launcher.version));
    menuUpdateButton_->setVisible(menuAvailable);
    menuUpdateButton_->setEnabled(true);
    menuUpdateButton_->setText(tr("Update DemoUI to %1").arg(updateInfo_.menu.version));

    if (launcherAvailable && menuAvailable) {
        setUpdateStatus(
            tr("Launcher %1 and DemoUI %2 are available.")
                .arg(updateInfo_.launcher.version, updateInfo_.menu.version),
            QStringLiteral("attention"));
    } else if (launcherAvailable) {
        setUpdateStatus(
            tr("Launcher %1 is available.").arg(updateInfo_.launcher.version),
            QStringLiteral("attention"));
    } else if (menuAvailable) {
        setUpdateStatus(
            tr("DemoUI %1 is available independently.").arg(updateInfo_.menu.version),
            QStringLiteral("attention"));
    } else {
        setUpdateStatus(
            tr("You're up to date"),
            QStringLiteral("success"));
    }
}

void LauncherWindow::positionUpdateBubble()
{
    if (!updateBubble_ || !centralWidget())
        return;
    const int x = qMax(16, centralWidget()->width() - updateBubble_->width() - 18);
    const int y = qMax(16, centralWidget()->height() - updateBubble_->height() - 18);
    updateBubble_->move(x, y);
}

void LauncherWindow::detectEnvironment()
{
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    const QString preferred = settings.value(QStringLiteral("cs2Root")).toString();
    QString error;
    paths_ = Cs2Manager::detect(preferred, &error);
    if (paths_.isValid()) {
        settings.setValue(QStringLiteral("cs2Root"), paths_.cs2Root);
        cs2Path_->setText(QDir::toNativeSeparators(paths_.cs2Root));
        lastStatus_ = tr("CS2 was detected automatically. You can now choose a Demo.");
    } else {
        cs2Path_->setText(tr("CS2 not found"));
        lastStatus_ = error;
    }
}

void LauncherWindow::chooseDemo()
{
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    const QString startDirectory = QFileInfo(demoPath_).exists()
        ? QFileInfo(demoPath_).absolutePath()
        : settings.value(QStringLiteral("lastDemoDirectory"), QDir::homePath()).toString();
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Choose a CS2 Demo"),
        startDirectory,
        tr("CS2 Demo files (*.dem *.zip *.zst);;Demo files (*.dem);;Zstandard Demos (*.zst);;ZIP archives (*.zip)"));
    if (!path.isEmpty())
        setDemoPath(path);
}

void LauncherWindow::setDemoPath(const QString &path, const QString &preferredArchiveEntry)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || !isDemoSource(path)) {
        QMessageBox::warning(this, tr("Unable to use this file"), tr("Select a valid CS2 .dem, .zip, or .zst file."));
        return;
    }

    QString selectedArchiveEntry;
    qint64 selectedDemoSize = info.size();
    if (info.suffix().compare(QStringLiteral("zip"), Qt::CaseInsensitive) == 0) {
        QList<DemoArchiveEntry> entries;
        const LauncherResult inspected = Cs2Manager::inspectDemoArchive(info.absoluteFilePath(), &entries);
        if (!inspected.ok) {
            QMessageBox::warning(this, tr("Unable to use this file"), inspected.message);
            return;
        }

        auto selected = std::find_if(entries.cbegin(), entries.cend(), [&preferredArchiveEntry](const DemoArchiveEntry &entry) {
            return !preferredArchiveEntry.isEmpty() && entry.path == preferredArchiveEntry;
        });
        if (selected == entries.cend() && entries.size() == 1) {
            selected = entries.cbegin();
        } else if (selected == entries.cend()) {
            QStringList choices;
            choices.reserve(entries.size());
            for (const DemoArchiveEntry &entry : std::as_const(entries))
                choices.append(tr("%1  ·  %2").arg(entry.path, Cs2Manager::displayFileSize(entry.size)));

            bool accepted = false;
            const QString choice = QInputDialog::getItem(
                this,
                tr("Choose a Demo from the ZIP"),
                tr("This ZIP contains multiple Demo files. Choose one to play:"),
                choices,
                0,
                false,
                &accepted);
            if (!accepted)
                return;
            const int selectedIndex = choices.indexOf(choice);
            if (selectedIndex < 0)
                return;
            selected = entries.cbegin() + selectedIndex;
        }
        selectedArchiveEntry = selected->path;
        selectedDemoSize = selected->size;
    }

    demoPath_ = info.absoluteFilePath();
    demoArchiveEntry_ = selectedArchiveEntry;
    demoSize_ = selectedDemoSize;
    refreshDemoDetails();
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    settings.setValue(QStringLiteral("lastDemo"), demoPath_);
    settings.setValue(QStringLiteral("lastDemoDirectory"), info.absolutePath());
    if (demoArchiveEntry_.isEmpty())
        settings.remove(QStringLiteral("lastDemoArchiveEntry"));
    else
        settings.setValue(QStringLiteral("lastDemoArchiveEntry"), demoArchiveEntry_);
    lastStatus_ = tr("Demo selected. Start playback to install DemoUI automatically and launch CS2.");
    selectPage(0);
    refreshState();
}

void LauncherWindow::refreshDemoDetails()
{
    const QFileInfo info(demoPath_);
    if (!info.exists() || !info.isFile())
        return;
    if (info.suffix().compare(QStringLiteral("zst"), Qt::CaseInsensitive) == 0) {
        demoName_->setText(info.completeBaseName());
        demoMeta_->setText(tr("%1  ·  Zstandard ·  %2").arg(Cs2Manager::displayFileSize(info.size()), info.fileName()));
        demoMeta_->setToolTip(tr("Compressed Demo: %1").arg(QDir::toNativeSeparators(demoPath_)));
    } else if (demoArchiveEntry_.isEmpty()) {
        demoName_->setText(info.fileName());
        demoMeta_->setText(tr("%1  ·  %2").arg(Cs2Manager::displayFileSize(info.size()), info.dir().dirName()));
        demoMeta_->setToolTip(QDir::toNativeSeparators(demoPath_));
    } else {
        demoName_->setText(QFileInfo(demoArchiveEntry_).fileName());
        demoMeta_->setText(tr("%1  ·  ZIP ·  %2").arg(Cs2Manager::displayFileSize(demoSize_), info.fileName()));
        demoMeta_->setToolTip(tr("Archive: %1\nDemo: %2").arg(QDir::toNativeSeparators(demoPath_), demoArchiveEntry_));
    }
}

void LauncherWindow::chooseCs2Directory()
{
    const QString initial = paths_.isValid() ? paths_.cs2Root : QDir::homePath();
    const QString selected = QFileDialog::getExistingDirectory(this, tr("Choose the CS2 installation folder"), initial);
    if (selected.isEmpty())
        return;

    QString error;
    const Cs2Paths selectedPaths = Cs2Manager::fromSelection(selected, &error);
    if (!selectedPaths.isValid()) {
        QMessageBox::warning(this, tr("Invalid folder"), error);
        return;
    }

    paths_ = selectedPaths;
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    settings.setValue(QStringLiteral("cs2Root"), paths_.cs2Root);
    cs2Path_->setText(QDir::toNativeSeparators(paths_.cs2Root));
    lastStatus_ = tr("The CS2 path has been updated.");
    refreshState();
}

void LauncherWindow::installOrRepairMenu()
{
    if (Cs2Manager::isCs2Running()) {
        QMessageBox::warning(this, tr("Exit CS2 first"), tr("The VPK cannot be replaced while CS2 is running. Fully exit the game before installing or repairing it."));
        return;
    }
    const QString vpk = UpdateService::preferredMenuVpk();
    showResult(Cs2Manager::installOverride(paths_, vpk));
    refreshState();
}

void LauncherWindow::startWatchingDemo()
{
    if (demoPreparationInProgress_)
        return;

    if (!paths_.isValid() || demoPath_.isEmpty()) {
        QMessageBox::warning(this, tr("Not ready"), tr("Choose a valid Demo and confirm the CS2 installation folder first."));
        return;
    }
    if (Cs2Manager::isCs2Running()) {
        QMessageBox::warning(this, tr("CS2 is already running"), tr("To enable -insecure, fully exit CS2 and let the launcher reopen it."));
        return;
    }

    QStringList advancedLaunchArguments;
    const LauncherResult optionsResult = Cs2Manager::parseAdvancedLaunchOptions(
        advancedLaunchOptionsText_,
        &advancedLaunchArguments);
    if (!optionsResult.ok) {
        QMessageBox::warning(this, tr("Invalid launch options"), optionsResult.message);
        openAdvancedLaunchOptionsDialog();
        return;
    }
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    settings.setValue(QStringLiteral("advancedLaunchOptions"), advancedLaunchOptionsText_);

    QMessageBox confirmation(this);
    confirmation.setIcon(QMessageBox::Warning);
    confirmation.setWindowTitle(tr("Start Demo playback mode"));
    confirmation.setText(tr("CS2 will start in -insecure mode"));
    QString confirmationDetail = tr("This session cannot be used for normal matchmaking. After watching, exit CS2 and return to the launcher to stop Demo playback.\n\nThe launcher does not change permanent Steam launch options.");
    if (!advancedLaunchOptionsText_.isEmpty())
        confirmationDetail += tr("\n\nAdditional launch options: %1").arg(advancedLaunchOptionsText_);
    confirmation.setInformativeText(confirmationDetail);
    auto *continueButton = confirmation.addButton(tr("Continue"), QMessageBox::AcceptRole);
    continueButton->setObjectName(QStringLiteral("DialogPrimaryButton"));
    confirmation.setDefaultButton(continueButton);
    confirmation.addButton(tr("Cancel"), QMessageBox::RejectRole);
    confirmation.exec();
    if (confirmation.clickedButton() != continueButton)
        return;

    const Cs2Paths paths = paths_;
    const QString demoPath = demoPath_;
    const QString archiveEntry = demoArchiveEntry_;
    const bool trueViewEnabled = trueViewEnabled_;
    const QString vpk = UpdateService::preferredMenuVpk();
    const QString installStage = tr("Preparing the DemoUI component...");
    const QString stagingStage = tr("Preparing the Demo file...");
    const QString voiceStage = tr("Analyzing Demo voice data... Large files may take a moment.");
    const QString launchStage = tr("Voice data is ready. Starting CS2...");

    demoPreparationInProgress_ = true;
    lastStatus_ = installStage;
    if (stateTimer_)
        stateTimer_->stop();
    refreshState();

    const QPointer<LauncherWindow> window(this);
    const auto reportStage = [window](const QString &message) {
        if (!window)
            return;
        QMetaObject::invokeMethod(window, [window, message]() {
            if (!window || !window->demoPreparationInProgress_)
                return;
            window->lastStatus_ = message;
            window->statusLabel_->setText(message);
        }, Qt::QueuedConnection);
    };

    auto *watcher = new QFutureWatcher<LauncherResult>(this);
    connect(watcher, &QFutureWatcher<LauncherResult>::finished, this, [this, watcher]() {
        const LauncherResult result = watcher->result();
        watcher->deleteLater();
        demoPreparationInProgress_ = false;
        if (stateTimer_)
            stateTimer_->start();
        showResult(result);
        refreshState();
    });
    watcher->setFuture(QtConcurrent::run([
        paths,
        demoPath,
        archiveEntry,
        trueViewEnabled,
        advancedLaunchArguments,
        vpk,
        installStage,
        stagingStage,
        voiceStage,
        launchStage,
        reportStage
    ]() {
        reportStage(installStage);
        LauncherResult result = Cs2Manager::installOverride(paths, vpk);
        if (!result.ok)
            return result;

        reportStage(stagingStage);
        result = Cs2Manager::prepareDemoSession(paths, demoPath, archiveEntry, trueViewEnabled);
        if (!result.ok)
            return result;

        reportStage(voiceStage);
        result = Cs2Manager::prepareVoiceStatusData(paths);
        if (!result.ok)
            return result;

        reportStage(launchStage);
        return Cs2Manager::launchDemo(paths, advancedLaunchArguments);
    }));
}

void LauncherWindow::stopWatchingDemo()
{
    if (Cs2Manager::isCs2Running()) {
        QMessageBox::warning(this, tr("Demo session is still active"), tr("Exit CS2 from the game menu and wait for the process to close completely before stopping Demo playback."));
        return;
    }

    const LauncherResult result = Cs2Manager::removeDemoSession(paths_);
    showResult(result);
    refreshState();
}

void LauncherWindow::showResult(const LauncherResult &result, bool dialogOnFailure)
{
    lastStatus_ = result.message;
    statusLabel_->setText(lastStatus_);
    if (!result.ok && dialogOnFailure)
        QMessageBox::critical(this, tr("Operation not completed"), result.message);
}

void LauncherWindow::openAdvancedLaunchOptionsDialog()
{
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("AdvancedOptionsDialog"));
    dialog.setWindowTitle(tr("Advanced launch options"));
    dialog.setModal(true);
    dialog.setMinimumWidth(560);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(10);

    auto *title = new QLabel(tr("Advanced launch options"), &dialog);
    title->setObjectName(QStringLiteral("AdvancedOptionsDialogTitle"));
    layout->addWidget(title);

    auto *detail = new QLabel(
        tr("For advanced users. Settings are saved locally and used only when this launcher starts CS2."),
        &dialog);
    detail->setObjectName(QStringLiteral("AdvancedOptionsDialogDetail"));
    detail->setWordWrap(true);
    layout->addWidget(detail);
    layout->addSpacing(5);

    auto *fieldLabel = new QLabel(tr("Launch options"), &dialog);
    fieldLabel->setObjectName(QStringLiteral("AdvancedOptionsDialogFieldLabel"));
    layout->addWidget(fieldLabel);

    auto *input = new QLineEdit(advancedLaunchOptionsText_, &dialog);
    input->setObjectName(QStringLiteral("LaunchOptionsEdit"));
    input->setPlaceholderText(tr("Example: -fullscreen -w 1920 -h 1080 +fps_max 0"));
    input->setClearButtonEnabled(true);
    input->setMaxLength(2048);
    input->setToolTip(tr("Spaces separate arguments. Put values containing spaces in double quotes."));
    layout->addWidget(input);

    auto *helper = new QLabel(
        tr("Spaces separate arguments. Put values containing spaces in double quotes."),
        &dialog);
    helper->setObjectName(QStringLiteral("AdvancedOptionsDialogHelper"));
    helper->setWordWrap(true);
    layout->addWidget(helper);

    auto *error = new QLabel(&dialog);
    error->setObjectName(QStringLiteral("AdvancedOptionsDialogError"));
    error->setWordWrap(true);
    error->hide();
    layout->addWidget(error);

    auto *notice = new QLabel(
        tr("The launcher already manages -insecure and other session-critical options."),
        &dialog);
    notice->setObjectName(QStringLiteral("AdvancedOptionsDialogNotice"));
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addSpacing(4);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        Qt::Horizontal,
        &dialog);
    auto *saveButton = buttons->button(QDialogButtonBox::Save);
    saveButton->setObjectName(QStringLiteral("AdvancedOptionsSaveButton"));
    saveButton->setText(tr("Save"));
    saveButton->setDefault(true);
    saveButton->setStyleSheet(QStringLiteral(R"CSS(
        QPushButton {
            border-color: #347fd8;
            background: #347fd8;
            color: #ffffff;
        }
        QPushButton:hover {
            border-color: #438ee6;
            background: #438ee6;
        }
        QPushButton:pressed { background: #2f73c4; }
    )CSS"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(input, &QLineEdit::textChanged, &dialog, [this, input, error]() {
        if (input->property("invalid").toBool()) {
            input->setProperty("invalid", false);
            repolish(input);
        }
        error->hide();
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [this, &dialog, input, error]() {
        QStringList parsedArguments;
        const LauncherResult result = Cs2Manager::parseAdvancedLaunchOptions(input->text(), &parsedArguments);
        if (!result.ok) {
            input->setProperty("invalid", true);
            repolish(input);
            error->setText(result.message);
            error->show();
            input->setFocus();
            return;
        }

        advancedLaunchOptionsText_ = input->text().trimmed();
        QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
        settings.setValue(QStringLiteral("advancedLaunchOptions"), advancedLaunchOptionsText_);
        refreshAdvancedOptionsUi();
        dialog.accept();
    });
    layout->addWidget(buttons);

    input->setFocus();
    input->selectAll();
    dialog.adjustSize();
    dialog.exec();
}

void LauncherWindow::refreshAdvancedOptionsUi()
{
    if (!advancedOptionsSummary_ || !advancedOptionsDisclosure_)
        return;

    const bool configured = !advancedLaunchOptionsText_.trimmed().isEmpty();
    advancedOptionsSummary_->setText(configured ? tr("Configured") : tr("Advanced users only"));
    advancedOptionsSummary_->setProperty("configured", configured);
    const QString openHint = tr("Open advanced launch options");
    advancedOptionsDisclosure_->setToolTip(openHint);
    advancedOptionsDisclosure_->setAccessibleName(openHint);
    advancedOptionsDisclosure_->setAccessibleDescription(
        configured ? tr("Launch options are configured") : tr("Advanced users only"));
    repolish(advancedOptionsSummary_);
}

void LauncherWindow::setSecurityState(const QString &state, const QString &title, const QString &detail)
{
    securityBadge_->setProperty("state", state);
    warningCard_->setProperty("state", state == QStringLiteral("safe") ? QStringLiteral("ready") : state);
    statusLabel_->setProperty("state", state);
    warningIcon_->setText(state == QStringLiteral("safe") ? tr("✓") : QStringLiteral("!"));
    securityBadge_->setText(title);
    warningTitle_->setText(title);
    warningDetail_->setText(detail);
    repolish(securityBadge_);
    repolish(warningCard_);
    repolish(statusLabel_);
}

void LauncherWindow::refreshState()
{
    const bool valid = paths_.isValid();
    const bool running = valid && Cs2Manager::isCs2Running();
    const bool installed = valid && Cs2Manager::isOverrideInstalled(paths_);
    const bool active = valid && Cs2Manager::isSessionActive(paths_);
    const bool preparing = demoPreparationInProgress_;

    if (valid)
        cs2Path_->setText(QDir::toNativeSeparators(paths_.cs2Root));
    vpkStatus_->setProperty("installed", installed);
    vpkStatus_->setText(!valid ? tr("Waiting for the CS2 path")
                               : installed ? tr("Installed and ready") : tr("Not installed · handled automatically at launch"));
    repolish(vpkStatus_);

    chooseDemoButton_->setEnabled(!preparing && !active);
    chooseCs2Button_->setEnabled(!preparing && !running && !active);
    installButton_->setEnabled(!preparing && valid && !running && !active);
    startButton_->setEnabled(!preparing && valid && !demoPath_.isEmpty() && !running && !active);
    startButton_->setText(preparing ? tr("Preparing...") : tr("Start playback"));
    stopButton_->setEnabled(!preparing && valid && !running && (active || installed));
    trueViewCheckBox_->setEnabled(!preparing && !running && !active);
    advancedOptionsDisclosure_->setEnabled(!preparing && !running && !active);
    languageCombo_->setEnabled(!preparing);
    preparationProgress_->setVisible(preparing);
    setAcceptDrops(!preparing);

    if (preparing) {
        setSecurityState(QStringLiteral("active"), tr("Preparing Demo"), tr("The launcher is copying and analyzing the Demo in the background. You can continue using the window."));
        statusLabel_->setText(lastStatus_);
    } else if (!valid) {
        setSecurityState(QStringLiteral("danger"), tr("Path required"), tr("No valid CS2 installation was found. Select Change to choose it manually."));
        statusLabel_->setText(lastStatus_);
    } else if (active && running) {
        setSecurityState(QStringLiteral("active"), tr("Demo mode is active"), tr("CS2 is running with -insecure. Do not enter normal matchmaking. Exit the game after playback."));
        statusLabel_->setText(tr("A Demo session is active. After exiting the game, return here and stop Demo playback."));
    } else if (active && !running) {
        setSecurityState(QStringLiteral("danger"), tr("Cleanup required"), tr("CS2 has exited, but Demo resources are still enabled. Select Stop and restore."));
        statusLabel_->setText(tr("Demo resources can now be removed safely to restore the normal game environment."));
    } else if (running) {
        setSecurityState(QStringLiteral("active"), tr("CS2 is running"), tr("Exit the current game before starting Demo playback mode here."));
        statusLabel_->setText(tr("Waiting for CS2 to exit."));
    } else if (installed) {
        setSecurityState(QStringLiteral("active"), tr("DemoUI is ready"), tr("You can start Demo playback. To play normally, select Stop and restore first."));
        statusLabel_->setText(lastStatus_.isEmpty() ? tr("DemoUI is ready.") : lastStatus_);
    } else {
        setSecurityState(QStringLiteral("safe"), tr("Ready for normal play"), tr("No Demo session or DemoUI override is active. CS2 can be started normally."));
        statusLabel_->setText(lastStatus_.isEmpty() ? tr("Choose a Demo.") : lastStatus_);
    }
}

void LauncherWindow::repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void LauncherWindow::selectPage(int index)
{
    if (!pages_ || index < 0 || index >= pages_->count())
        return;
    pages_->setCurrentIndex(index);
    navReplayButton_->setChecked(index == 0);
    navMenuButton_->setChecked(index == 1);
    navAboutButton_->setChecked(index == 2);
}

void LauncherWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (paths_.isValid() && Cs2Manager::isSessionActive(paths_))
        return;
    bool acceptsDemo = false;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (isDemoSource(url.toLocalFile())) {
            acceptsDemo = true;
            break;
        }
    }
    if (acceptsDemo) {
        dropCard_->setProperty("dragging", true);
        repolish(dropCard_);
        event->acceptProposedAction();
    }
}

void LauncherWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    dropCard_->setProperty("dragging", false);
    repolish(dropCard_);
    event->accept();
}

void LauncherWindow::dropEvent(QDropEvent *event)
{
    dropCard_->setProperty("dragging", false);
    repolish(dropCard_);
    for (const QUrl &url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (isDemoSource(path)) {
            setDemoPath(path);
            event->acceptProposedAction();
            return;
        }
    }
}

void LauncherWindow::closeEvent(QCloseEvent *event)
{
    if (demoPreparationInProgress_) {
        QMessageBox::information(
            this,
            tr("Demo preparation is in progress"),
            tr("Please wait for Demo preparation to finish before closing the launcher."));
        event->ignore();
        return;
    }

    if (paths_.isValid() && (Cs2Manager::isSessionActive(paths_) || Cs2Manager::isOverrideInstalled(paths_))) {
        QMessageBox warning(this);
        warning.setIcon(QMessageBox::Warning);
        warning.setWindowTitle(tr("Demo mode has not been cleaned up"));
        warning.setText(tr("Closing the launcher does not restore the normal matchmaking environment automatically.\n\nAfter watching, exit CS2, reopen the launcher, and stop Demo playback."));
        auto *closeButton = warning.addButton(tr("Close launcher"), QMessageBox::AcceptRole);
        warning.addButton(tr("Cancel"), QMessageBox::RejectRole);
        warning.exec();
        if (warning.clickedButton() != closeButton) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void LauncherWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    positionUpdateBubble();
}
