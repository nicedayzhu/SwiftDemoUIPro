#include "LauncherWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
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

enum class Glyph
{
    Demo,
    Folder,
    Play,
    Stop,
    Menu,
    About,
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

}

LauncherWindow::LauncherWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Swift DemoUI Pro"));
    setWindowIcon(QIcon(makeLogo()));
    setMinimumSize(920, 640);
    resize(1000, 720);
    setAcceptDrops(true);

    buildInterface();
    applyStyle();
    detectEnvironment();

    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    const QString rememberedDemo = settings.value(QStringLiteral("lastDemo")).toString();
    if (QFileInfo::exists(rememberedDemo))
        setDemoPath(rememberedDemo);

    stateTimer_ = new QTimer(this);
    stateTimer_->setInterval(1000);
    connect(stateTimer_, &QTimer::timeout, this, &LauncherWindow::refreshState);
    stateTimer_->start();
    refreshState();
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
    sidebar->setFixedWidth(220);
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
    auto *brandSubtitle = new QLabel(QStringLiteral("CS2 回放工具"), sidebar);
    brandSubtitle->setObjectName(QStringLiteral("BrandSubtitle"));
    brandCopy->addWidget(brandTitle);
    brandCopy->addWidget(brandSubtitle);
    brand->addLayout(brandCopy, 1);
    sideLayout->addLayout(brand);
    sideLayout->addSpacing(22);

    auto *workspaceCaption = new QLabel(QStringLiteral("工作区"), sidebar);
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

    navReplayButton_ = makeNavButton(QStringLiteral("Demo 回放"), Glyph::Demo);
    navMenuButton_ = makeNavButton(QStringLiteral("DemoUI 管理"), Glyph::Menu);
    sideLayout->addWidget(navReplayButton_);
    sideLayout->addWidget(navMenuButton_);
    sideLayout->addStretch(1);

    auto *moreCaption = new QLabel(QStringLiteral("其他"), sidebar);
    moreCaption->setObjectName(QStringLiteral("NavCaption"));
    sideLayout->addWidget(moreCaption);
    navAboutButton_ = makeNavButton(QStringLiteral("关于"), Glyph::About);
    sideLayout->addWidget(navAboutButton_);

    auto *version = new QLabel(QStringLiteral("Swift DemoUI Pro  ·  0.1.0"), sidebar);
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
    auto *replayTitle = new QLabel(QStringLiteral("Demo 回放"), replayPage);
    replayTitle->setObjectName(QStringLiteral("PageTitle"));
    auto *replaySubtitle = new QLabel(QStringLiteral("选择录像，并以安全可恢复的方式启动 CS2"), replayPage);
    replaySubtitle->setObjectName(QStringLiteral("PageSubtitle"));
    replayTitleCopy->addWidget(replayTitle);
    replayTitleCopy->addWidget(replaySubtitle);
    replayHeader->addLayout(replayTitleCopy, 1);

    securityBadge_ = new QLabel(QStringLiteral("正在检查"), replayPage);
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
    warningIcon_ = new QLabel(QStringLiteral("✓"), warningCard_);
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
    fileCopy->addWidget(sectionEyebrow(QStringLiteral("选择录像"), dropCard_));
    demoName_ = new QLabel(QStringLiteral("拖放 Demo 到这里"), dropCard_);
    demoName_->setObjectName(QStringLiteral("PrimaryText"));
    demoMeta_ = new QLabel(QStringLiteral("支持 CS2 .dem 文件，也可以点击右侧浏览"), dropCard_);
    demoMeta_->setObjectName(QStringLiteral("SecondaryText"));
    fileCopy->addWidget(demoName_);
    fileCopy->addWidget(demoMeta_);
    dropLayout->addLayout(fileCopy, 1);
    chooseDemoButton_ = new QPushButton(QStringLiteral("浏览…"), dropCard_);
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
    actions->addWidget(sectionEyebrow(QStringLiteral("观看控制"), actionCard));
    statusLabel_ = new QLabel(QStringLiteral("正在检查环境..."), actionCard);
    statusLabel_->setObjectName(QStringLiteral("StatusText"));
    statusLabel_->setProperty("state", QStringLiteral("neutral"));
    statusLabel_->setWordWrap(true);
    actions->addWidget(statusLabel_);
    auto *actionRow = new QHBoxLayout;
    actionRow->setSpacing(10);
    startButton_ = new QPushButton(QStringLiteral("开始观看"), actionCard);
    startButton_->setObjectName(QStringLiteral("PrimaryButton"));
    startButton_->setIcon(makeGlyphIcon(Glyph::Play, QColor(QStringLiteral("#ffffff"))));
    startButton_->setIconSize(QSize(18, 18));
    startButton_->setMinimumHeight(50);
    connect(startButton_, &QPushButton::clicked, this, &LauncherWindow::startWatchingDemo);
    actionRow->addWidget(startButton_, 3);
    stopButton_ = new QPushButton(QStringLiteral("停止观看并恢复"), actionCard);
    stopButton_->setObjectName(QStringLiteral("StopButton"));
    stopButton_->setIcon(makeGlyphIcon(Glyph::Stop, QColor(QStringLiteral("#657080"))));
    stopButton_->setIconSize(QSize(17, 17));
    stopButton_->setMinimumHeight(50);
    connect(stopButton_, &QPushButton::clicked, this, &LauncherWindow::stopWatchingDemo);
    actionRow->addWidget(stopButton_, 2);
    actions->addLayout(actionRow);
    replay->addWidget(actionCard);
    replay->addStretch(1);

    auto *replayFooter = new QLabel(QStringLiteral("仅本次使用 -insecure  ·  不会修改 Steam 永久启动项"), replayPage);
    replayFooter->setObjectName(QStringLiteral("FooterText"));
    replayFooter->setAlignment(Qt::AlignCenter);
    replay->addWidget(replayFooter);
    pages_->addWidget(replayPage);

    auto *menuPage = new QWidget(pages_);
    menuPage->setObjectName(QStringLiteral("Page"));
    auto *menu = new QVBoxLayout(menuPage);
    menu->setContentsMargins(36, 30, 36, 24);
    menu->setSpacing(18);

    auto *menuTitle = new QLabel(QStringLiteral("DemoUI 管理"), menuPage);
    menuTitle->setObjectName(QStringLiteral("PageTitle"));
    auto *menuSubtitle = new QLabel(QStringLiteral("管理 CS2 路径和 Swift DemoUI 组件"), menuPage);
    menuSubtitle->setObjectName(QStringLiteral("PageSubtitle"));
    menu->addWidget(menuTitle);
    menu->addWidget(menuSubtitle);
    menu->addSpacing(4);

    auto *environmentCard = new QFrame(menuPage);
    environmentCard->setObjectName(QStringLiteral("Card"));
    auto *environment = new QVBoxLayout(environmentCard);
    environment->setContentsMargins(18, 17, 18, 18);
    environment->setSpacing(10);
    environment->addWidget(sectionEyebrow(QStringLiteral("游戏环境"), environmentCard));

    auto *pathRow = new QFrame(environmentCard);
    pathRow->setObjectName(QStringLiteral("SettingsRow"));
    pathRow->setMinimumHeight(68);
    auto *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(14, 11, 10, 11);
    pathLayout->setSpacing(14);
    auto *pathCopy = new QVBoxLayout;
    pathCopy->setSpacing(2);
    auto *pathCaption = new QLabel(QStringLiteral("CS2 路径"), pathRow);
    pathCaption->setObjectName(QStringLiteral("FieldLabel"));
    pathCopy->addWidget(pathCaption);
    cs2Path_ = new QLabel(QStringLiteral("正在检测..."), pathRow);
    cs2Path_->setObjectName(QStringLiteral("PathText"));
    cs2Path_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathCopy->addWidget(cs2Path_);
    pathLayout->addLayout(pathCopy, 1);
    chooseCs2Button_ = new QPushButton(QStringLiteral("更改"), pathRow);
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
    auto *vpkCaption = new QLabel(QStringLiteral("DemoUI 组件"), vpkRow);
    vpkCaption->setObjectName(QStringLiteral("FieldLabel"));
    vpkCopy->addWidget(vpkCaption);
    vpkStatus_ = new QLabel(QStringLiteral("正在检查..."), vpkRow);
    vpkStatus_->setObjectName(QStringLiteral("VpkStatus"));
    vpkCopy->addWidget(vpkStatus_);
    vpkLayout->addLayout(vpkCopy, 1);
    installButton_ = new QPushButton(QStringLiteral("安装 / 修复"), vpkRow);
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
    auto *infoTitle = new QLabel(QStringLiteral("启动器会自动处理"), managementInfo);
    infoTitle->setObjectName(QStringLiteral("InfoTitle"));
    infoLayout->addWidget(infoTitle);
    const QStringList details = {
        QStringLiteral("启动 Demo 时自动安装或修复 DemoUI 组件"),
        QStringLiteral("停止观看后移除临时录像、配置和 DemoUI 覆盖"),
        QStringLiteral("不会写入或修改 Steam 的永久启动参数")
    };
    for (const QString &detail : details) {
        auto *label = new QLabel(QStringLiteral("•  %1").arg(detail), managementInfo);
        label->setObjectName(QStringLiteral("InfoText"));
        infoLayout->addWidget(label);
    }
    menu->addWidget(managementInfo);
    menu->addStretch(1);
    pages_->addWidget(menuPage);

    auto *aboutPage = new QWidget(pages_);
    aboutPage->setObjectName(QStringLiteral("Page"));
    auto *about = new QVBoxLayout(aboutPage);
    about->setContentsMargins(36, 30, 36, 24);
    about->setSpacing(18);

    auto *aboutTitle = new QLabel(QStringLiteral("关于"), aboutPage);
    aboutTitle->setObjectName(QStringLiteral("PageTitle"));
    auto *aboutSubtitle = new QLabel(QStringLiteral("项目信息、作者主页与支持方式"), aboutPage);
    aboutSubtitle->setObjectName(QStringLiteral("PageSubtitle"));
    about->addWidget(aboutTitle);
    about->addWidget(aboutSubtitle);
    about->addSpacing(4);

    auto *aboutHero = new QFrame(aboutPage);
    aboutHero->setObjectName(QStringLiteral("AboutHero"));
    auto *heroLayout = new QHBoxLayout(aboutHero);
    heroLayout->setContentsMargins(22, 20, 22, 20);
    heroLayout->setSpacing(16);
    auto *heroLogo = new QLabel(aboutHero);
    heroLogo->setPixmap(makeLogo());
    heroLogo->setFixedSize(48, 48);
    heroLayout->addWidget(heroLogo);
    auto *heroCopy = new QVBoxLayout;
    heroCopy->setSpacing(3);
    auto *heroTitle = new QLabel(QStringLiteral("Swift DemoUI Pro"), aboutHero);
    heroTitle->setObjectName(QStringLiteral("AboutTitle"));
    auto *heroDescription = new QLabel(QStringLiteral("轻量、原生的 Counter-Strike 2 DemoUI 增强与回放工具"), aboutHero);
    heroDescription->setObjectName(QStringLiteral("AboutDescription"));
    auto *heroVersion = new QLabel(QStringLiteral("版本 0.1.0  ·  Qt 6 Widgets"), aboutHero);
    heroVersion->setObjectName(QStringLiteral("AboutVersion"));
    heroCopy->addWidget(heroTitle);
    heroCopy->addWidget(heroDescription);
    heroCopy->addWidget(heroVersion);
    heroLayout->addLayout(heroCopy, 1);
    about->addWidget(aboutHero);

    auto *socialCard = new QFrame(aboutPage);
    socialCard->setObjectName(QStringLiteral("Card"));
    auto *socials = new QVBoxLayout(socialCard);
    socials->setContentsMargins(18, 17, 18, 18);
    socials->setSpacing(10);
    socials->addWidget(sectionEyebrow(QStringLiteral("找到我"), socialCard));

    const auto addSocial = [this, socials, socialCard](const QString &name, const QString &detail, const QString &url, bool support) {
        auto *row = new QFrame(socialCard);
        row->setObjectName(QStringLiteral("SocialRow"));
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(14, 10, 10, 10);
        rowLayout->setSpacing(12);
        auto *copy = new QVBoxLayout;
        copy->setSpacing(2);
        auto *title = new QLabel(name, row);
        title->setObjectName(QStringLiteral("SocialTitle"));
        auto *description = new QLabel(detail, row);
        description->setObjectName(QStringLiteral("SocialDescription"));
        copy->addWidget(title);
        copy->addWidget(description);
        rowLayout->addLayout(copy, 1);
        auto *openButton = new QPushButton(support ? QStringLiteral("支持我") : QStringLiteral("打开"), row);
        openButton->setObjectName(support ? QStringLiteral("SupportButton") : QStringLiteral("SocialButton"));
        openButton->setIcon(makeGlyphIcon(support ? Glyph::Coffee : Glyph::External, QColor(support ? QStringLiteral("#ffffff") : QStringLiteral("#5d6775"))));
        openButton->setIconSize(QSize(17, 17));
        connect(openButton, &QPushButton::clicked, this, [url]() {
            QDesktopServices::openUrl(QUrl(url));
        });
        rowLayout->addWidget(openButton);
        socials->addWidget(row);
    };

    addSocial(QStringLiteral("GitHub"), QStringLiteral("github.com/nicedayzhu"), QStringLiteral("https://github.com/nicedayzhu"), false);
    addSocial(QStringLiteral("哔哩哔哩"), QStringLiteral("space.bilibili.com/1405728"), QStringLiteral("https://space.bilibili.com/1405728"), false);
    addSocial(QStringLiteral("Ko-fi"), QStringLiteral("如果这个工具对你有帮助，可以请我喝杯咖啡"), QStringLiteral("https://ko-fi.com/K6C623WHCQ"), true);
    about->addWidget(socialCard);
    about->addStretch(1);

    auto *aboutFooter = new QLabel(QStringLiteral("Made for the CS2 demo community"), aboutPage);
    aboutFooter->setObjectName(QStringLiteral("FooterText"));
    aboutFooter->setAlignment(Qt::AlignCenter);
    about->addWidget(aboutFooter);
    pages_->addWidget(aboutPage);

    connect(navReplayButton_, &QPushButton::clicked, this, [this]() { selectPage(0); });
    connect(navMenuButton_, &QPushButton::clicked, this, [this]() { selectPage(1); });
    connect(navAboutButton_, &QPushButton::clicked, this, [this]() { selectPage(2); });
    selectPage(0);

    setCentralWidget(central);
}
void LauncherWindow::applyStyle()
{
    qApp->setStyleSheet(QStringLiteral(R"CSS(
        QWidget#Root {
            background: #f5f7fa;
            color: #1a1d23;
            font-family: "Segoe UI Variable", "Microsoft YaHei UI", "Segoe UI";
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
        QFrame#AboutHero {
            border: 1px solid #dce8f8;
            border-radius: 12px;
            background: #f3f8ff;
        }
        QLabel#AboutTitle {
            color: #1c2027;
            font-size: 18px;
            font-weight: 650;
        }
        QLabel#AboutDescription {
            color: #5f6976;
            font-size: 13px;
        }
        QLabel#AboutVersion {
            color: #929aa5;
            font-size: 11px;
        }
        QFrame#SocialRow {
            border: none;
            border-radius: 9px;
            background: #f6f8fa;
        }
        QLabel#SocialTitle {
            color: #242831;
            font-size: 14px;
            font-weight: 650;
        }
        QLabel#SocialDescription {
            color: #7b8490;
            font-size: 12px;
        }
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
        QPushButton#GhostButton,
        QPushButton#SocialButton {
            border: 1px solid #d5dae1;
            background: #ffffff;
            color: #48515e;
        }
        QPushButton#StopButton:hover,
        QPushButton#SecondaryButton:hover,
        QPushButton#GhostButton:hover,
        QPushButton#SocialButton:hover {
            border-color: #aeb7c3;
            background: #f7f9fb;
            color: #242a32;
        }
        QPushButton#GhostButton { min-width: 58px; padding: 0 10px; }
        QPushButton#SocialButton { min-width: 74px; }
        QPushButton#SupportButton {
            min-width: 94px;
            border: none;
            background: #ff5f5f;
            color: #ffffff;
        }
        QPushButton#SupportButton:hover { background: #ef4e4e; }
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
        QToolTip {
            border: 1px solid #d9dde3;
            background: #ffffff;
            color: #282d35;
            padding: 5px;
        }
    )CSS"));
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
        lastStatus_ = QStringLiteral("已自动检测到 CS2，可以选择 Demo。");
    } else {
        cs2Path_->setText(QStringLiteral("未找到 CS2"));
        lastStatus_ = error;
    }
}

void LauncherWindow::chooseDemo()
{
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    const QString startDirectory = QFileInfo(demoPath_).exists()
        ? QFileInfo(demoPath_).absolutePath()
        : settings.value(QStringLiteral("lastDemoDirectory"), QDir::homePath()).toString();
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择 CS2 Demo"), startDirectory, QStringLiteral("CS2 Demo (*.dem)"));
    if (!path.isEmpty())
        setDemoPath(path);
}

void LauncherWindow::setDemoPath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.suffix().compare(QStringLiteral("dem"), Qt::CaseInsensitive) != 0) {
        QMessageBox::warning(this, QStringLiteral("无法使用该文件"), QStringLiteral("请选择有效的 CS2 .dem 文件。"));
        return;
    }

    demoPath_ = info.absoluteFilePath();
    demoName_->setText(info.fileName());
    demoMeta_->setText(QStringLiteral("%1  ·  %2").arg(Cs2Manager::displayFileSize(info.size()), info.dir().dirName()));
    demoMeta_->setToolTip(QDir::toNativeSeparators(demoPath_));
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    settings.setValue(QStringLiteral("lastDemo"), demoPath_);
    settings.setValue(QStringLiteral("lastDemoDirectory"), info.absolutePath());
    lastStatus_ = QStringLiteral("Demo 已选择。点击“开始观看”会自动安装 DemoUI 并启动 CS2。");
    selectPage(0);
    refreshState();
}

void LauncherWindow::chooseCs2Directory()
{
    const QString initial = paths_.isValid() ? paths_.cs2Root : QDir::homePath();
    const QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral("选择 CS2 安装目录"), initial);
    if (selected.isEmpty())
        return;

    QString error;
    const Cs2Paths selectedPaths = Cs2Manager::fromSelection(selected, &error);
    if (!selectedPaths.isValid()) {
        QMessageBox::warning(this, QStringLiteral("目录无效"), error);
        return;
    }

    paths_ = selectedPaths;
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    settings.setValue(QStringLiteral("cs2Root"), paths_.cs2Root);
    cs2Path_->setText(QDir::toNativeSeparators(paths_.cs2Root));
    lastStatus_ = QStringLiteral("CS2 路径已更新。");
    refreshState();
}

void LauncherWindow::installOrRepairMenu()
{
    if (Cs2Manager::isCs2Running()) {
        QMessageBox::warning(this, QStringLiteral("请先退出 CS2"), QStringLiteral("CS2 运行时不会替换 VPK。请完全退出游戏后再安装或修复。"));
        return;
    }
    const QString vpk = Cs2Manager::findBundledVpk();
    showResult(Cs2Manager::installOverride(paths_, vpk));
    refreshState();
}

void LauncherWindow::startWatchingDemo()
{
    if (!paths_.isValid() || demoPath_.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("尚未准备好"), QStringLiteral("请先选择有效的 Demo，并确认 CS2 安装目录。"));
        return;
    }
    if (Cs2Manager::isCs2Running()) {
        QMessageBox::warning(this, QStringLiteral("CS2 已在运行"), QStringLiteral("要启用 -insecure，必须完全退出当前 CS2，再由启动器重新打开。"));
        return;
    }

    QMessageBox confirmation(this);
    confirmation.setIcon(QMessageBox::Warning);
    confirmation.setWindowTitle(QStringLiteral("启动 Demo 观看模式"));
    confirmation.setText(QStringLiteral("CS2 将以 -insecure 模式启动"));
    confirmation.setInformativeText(QStringLiteral("此会话不能用于正常匹配。看完后请先退出 CS2，再回到启动器点击“停止观看 Demo”。\n\n启动器不会修改 Steam 的永久启动参数。"));
    auto *continueButton = confirmation.addButton(QStringLiteral("继续启动"), QMessageBox::AcceptRole);
    confirmation.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    confirmation.exec();
    if (confirmation.clickedButton() != continueButton)
        return;

    const QString vpk = Cs2Manager::findBundledVpk();
    LauncherResult result = Cs2Manager::installOverride(paths_, vpk);
    if (!result.ok) {
        showResult(result);
        return;
    }
    result = Cs2Manager::prepareDemoSession(paths_, demoPath_);
    if (!result.ok) {
        showResult(result);
        refreshState();
        return;
    }
    result = Cs2Manager::launchDemo(paths_);
    showResult(result);
    refreshState();
}

void LauncherWindow::stopWatchingDemo()
{
    if (Cs2Manager::isCs2Running()) {
        QMessageBox::warning(this, QStringLiteral("仍处于 Demo 会话"), QStringLiteral("请先从游戏菜单退出 CS2，并等待进程完全关闭，然后再点击“停止观看 Demo”。"));
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
        QMessageBox::critical(this, QStringLiteral("操作未完成"), result.message);
}

void LauncherWindow::setSecurityState(const QString &state, const QString &title, const QString &detail)
{
    securityBadge_->setProperty("state", state);
    warningCard_->setProperty("state", state == QStringLiteral("safe") ? QStringLiteral("ready") : state);
    statusLabel_->setProperty("state", state);
    warningIcon_->setText(state == QStringLiteral("safe") ? QStringLiteral("✓") : QStringLiteral("!"));
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

    if (valid)
        cs2Path_->setText(QDir::toNativeSeparators(paths_.cs2Root));
    vpkStatus_->setProperty("installed", installed);
    vpkStatus_->setText(!valid ? QStringLiteral("等待设置 CS2 路径")
                               : installed ? QStringLiteral("已安装，可用") : QStringLiteral("未安装 · 启动时自动处理"));
    repolish(vpkStatus_);

    chooseDemoButton_->setEnabled(!active);
    chooseCs2Button_->setEnabled(!running && !active);
    installButton_->setEnabled(valid && !running && !active);
    startButton_->setEnabled(valid && !demoPath_.isEmpty() && !running && !active);
    stopButton_->setEnabled(valid && !running && (active || installed));

    if (!valid) {
        setSecurityState(QStringLiteral("danger"), QStringLiteral("需要设置路径"), QStringLiteral("没有找到有效的 CS2 安装目录，请点击“更改”手动选择。"));
        statusLabel_->setText(lastStatus_);
    } else if (active && running) {
        setSecurityState(QStringLiteral("active"), QStringLiteral("Demo 模式运行中"), QStringLiteral("CS2 正在以 -insecure 运行，请勿进入正常匹配。观看结束后先退出游戏。"));
        statusLabel_->setText(QStringLiteral("Demo 会话进行中。游戏退出后，请回到这里点击“停止观看 Demo”。"));
    } else if (active && !running) {
        setSecurityState(QStringLiteral("danger"), QStringLiteral("需要完成清理"), QStringLiteral("CS2 已退出，但 Demo 资源仍在启用。请点击“停止观看并恢复”。"));
        statusLabel_->setText(QStringLiteral("现在可以安全清理 Demo 资源并恢复正常游戏环境。"));
    } else if (running) {
        setSecurityState(QStringLiteral("active"), QStringLiteral("CS2 正在运行"), QStringLiteral("请先退出当前游戏，再从这里启动 Demo 观看模式。"));
        statusLabel_->setText(QStringLiteral("等待 CS2 退出。"));
    } else if (installed) {
        setSecurityState(QStringLiteral("active"), QStringLiteral("DemoUI 已准备"), QStringLiteral("可以开始观看 Demo。若要正常游戏，请先点击“停止观看并恢复”。"));
        statusLabel_->setText(lastStatus_.isEmpty() ? QStringLiteral("DemoUI 已准备好。") : lastStatus_);
    } else {
        setSecurityState(QStringLiteral("safe"), QStringLiteral("可以正常游戏"), QStringLiteral("当前没有 Demo 会话或 DemoUI 覆盖，可以正常启动 CS2。"));
        statusLabel_->setText(lastStatus_.isEmpty() ? QStringLiteral("请选择一个 Demo。") : lastStatus_);
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
        if (QFileInfo(url.toLocalFile()).suffix().compare(QStringLiteral("dem"), Qt::CaseInsensitive) == 0) {
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
        if (QFileInfo(path).suffix().compare(QStringLiteral("dem"), Qt::CaseInsensitive) == 0) {
            setDemoPath(path);
            event->acceptProposedAction();
            return;
        }
    }
}

void LauncherWindow::closeEvent(QCloseEvent *event)
{
    if (paths_.isValid() && (Cs2Manager::isSessionActive(paths_) || Cs2Manager::isOverrideInstalled(paths_))) {
        const QMessageBox::StandardButton answer = QMessageBox::warning(
            this,
            QStringLiteral("Demo 模式尚未清理"),
            QStringLiteral("关闭启动器不会自动恢复正常匹配环境。\n\n观看结束后仍需退出 CS2、重新打开启动器并点击“停止观看 Demo”。"),
            QMessageBox::Close | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (answer != QMessageBox::Close) {
            event->ignore();
            return;
        }
    }
    event->accept();
}
