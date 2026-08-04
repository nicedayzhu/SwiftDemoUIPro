#include "LauncherWindow.h"

#include <QApplication>
#include <QCloseEvent>
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
    Stop
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
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(side * 0.25, side * 0.25, side * 0.50, side * 0.50), side * 0.08, side * 0.08);
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
    setWindowTitle(QStringLiteral("Swift Demo Launcher"));
    setWindowIcon(QIcon(makeLogo()));
    setMinimumSize(780, 680);
    resize(820, 700);
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
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(34, 28, 34, 24);
    root->setSpacing(16);

    auto *header = new QHBoxLayout;
    header->setSpacing(14);
    auto *logo = new QLabel(central);
    logo->setPixmap(makeLogo());
    logo->setFixedSize(48, 48);
    header->addWidget(logo);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("Demo 回放"), central);
    title->setObjectName(QStringLiteral("AppTitle"));
    auto *subtitle = new QLabel(QStringLiteral("SWIFT  ·  COUNTER-STRIKE 2"), central);
    subtitle->setObjectName(QStringLiteral("AppSubtitle"));
    titles->addWidget(title);
    titles->addWidget(subtitle);
    header->addLayout(titles, 1);

    securityBadge_ = new QLabel(QStringLiteral("正在检查"), central);
    securityBadge_->setObjectName(QStringLiteral("SecurityBadge"));
    securityBadge_->setProperty("state", QStringLiteral("neutral"));
    securityBadge_->setAlignment(Qt::AlignCenter);
    securityBadge_->setMinimumWidth(116);
    securityBadge_->setFixedHeight(32);
    header->addWidget(securityBadge_, 0, Qt::AlignVCenter);
    root->addLayout(header);

    warningCard_ = new QFrame(central);
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
    root->addWidget(warningCard_);

    dropCard_ = new QFrame(central);
    dropCard_->setObjectName(QStringLiteral("DropCard"));
    auto *dropLayout = new QHBoxLayout(dropCard_);
    dropLayout->setContentsMargins(18, 16, 18, 16);
    dropLayout->setSpacing(16);
    auto *fileIcon = new QLabel(dropCard_);
    fileIcon->setObjectName(QStringLiteral("DemoIcon"));
    fileIcon->setPixmap(makeGlyph(Glyph::Demo, 30, QColor(QStringLiteral("#73b7ff"))));
    fileIcon->setAlignment(Qt::AlignCenter);
    fileIcon->setFixedSize(48, 48);
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
    chooseDemoButton_->setIcon(makeGlyphIcon(Glyph::Folder, QColor(QStringLiteral("#d7dce5"))));
    chooseDemoButton_->setIconSize(QSize(18, 18));
    connect(chooseDemoButton_, &QPushButton::clicked, this, &LauncherWindow::chooseDemo);
    dropLayout->addWidget(chooseDemoButton_);
    root->addWidget(dropCard_);

    auto *environmentCard = new QFrame(central);
    environmentCard->setObjectName(QStringLiteral("Card"));
    auto *environment = new QVBoxLayout(environmentCard);
    environment->setContentsMargins(18, 16, 18, 17);
    environment->setSpacing(10);
    environment->addWidget(sectionEyebrow(QStringLiteral("游戏与菜单"), environmentCard));

    auto *pathRow = new QFrame(environmentCard);
    pathRow->setObjectName(QStringLiteral("SettingsRow"));
    pathRow->setMinimumHeight(60);
    auto *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(14, 10, 10, 10);
    pathLayout->setSpacing(14);
    auto *pathCopy = new QVBoxLayout;
    pathCopy->setSpacing(2);
    auto *pathCaption = new QLabel(QStringLiteral("CS2 路径"), environmentCard);
    pathCaption->setObjectName(QStringLiteral("FieldLabel"));
    pathCopy->addWidget(pathCaption);
    cs2Path_ = new QLabel(QStringLiteral("正在检测..."), environmentCard);
    cs2Path_->setObjectName(QStringLiteral("PathText"));
    cs2Path_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathCopy->addWidget(cs2Path_);
    pathLayout->addLayout(pathCopy, 1);
    chooseCs2Button_ = new QPushButton(QStringLiteral("更改"), environmentCard);
    chooseCs2Button_->setObjectName(QStringLiteral("GhostButton"));
    connect(chooseCs2Button_, &QPushButton::clicked, this, &LauncherWindow::chooseCs2Directory);
    pathLayout->addWidget(chooseCs2Button_);
    environment->addWidget(pathRow);

    auto *vpkRow = new QFrame(environmentCard);
    vpkRow->setObjectName(QStringLiteral("SettingsRow"));
    vpkRow->setMinimumHeight(60);
    auto *vpkLayout = new QHBoxLayout(vpkRow);
    vpkLayout->setContentsMargins(14, 10, 10, 10);
    vpkLayout->setSpacing(14);
    auto *vpkCopy = new QVBoxLayout;
    vpkCopy->setSpacing(2);
    auto *vpkCaption = new QLabel(QStringLiteral("菜单组件"), environmentCard);
    vpkCaption->setObjectName(QStringLiteral("FieldLabel"));
    vpkCopy->addWidget(vpkCaption);
    vpkStatus_ = new QLabel(QStringLiteral("正在检查..."), environmentCard);
    vpkStatus_->setObjectName(QStringLiteral("VpkStatus"));
    vpkCopy->addWidget(vpkStatus_);
    vpkLayout->addLayout(vpkCopy, 1);
    installButton_ = new QPushButton(QStringLiteral("安装 / 修复"), environmentCard);
    installButton_->setObjectName(QStringLiteral("SecondaryButton"));
    connect(installButton_, &QPushButton::clicked, this, &LauncherWindow::installOrRepairMenu);
    vpkLayout->addWidget(installButton_);
    environment->addWidget(vpkRow);
    root->addWidget(environmentCard);

    auto *actionCard = new QFrame(central);
    actionCard->setObjectName(QStringLiteral("Card"));
    auto *actions = new QVBoxLayout(actionCard);
    actions->setContentsMargins(18, 16, 18, 17);
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
    stopButton_->setIcon(makeGlyphIcon(Glyph::Stop, QColor(QStringLiteral("#c9ced8"))));
    stopButton_->setIconSize(QSize(17, 17));
    stopButton_->setMinimumHeight(50);
    connect(stopButton_, &QPushButton::clicked, this, &LauncherWindow::stopWatchingDemo);
    actionRow->addWidget(stopButton_, 2);
    actions->addLayout(actionRow);
    root->addWidget(actionCard);

    auto *footer = new QLabel(QStringLiteral("仅本次使用 -insecure  ·  不会修改 Steam 永久启动项"), central);
    footer->setObjectName(QStringLiteral("FooterText"));
    footer->setAlignment(Qt::AlignCenter);
    root->addWidget(footer);
    root->addStretch(1);

    setCentralWidget(central);
}

void LauncherWindow::applyStyle()
{
    qApp->setStyleSheet(QStringLiteral(R"CSS(
        QWidget#Root {
            background: #0d0f13;
            color: #eef1f6;
            font-family: "Segoe UI Variable", "Microsoft YaHei UI", "Segoe UI";
            font-size: 14px;
        }
        QLabel#AppTitle {
            color: #f5f7fb;
            font-size: 23px;
            font-weight: 600;
        }
        QLabel#AppSubtitle {
            color: #6f7785;
            font-size: 10px;
            font-weight: 600;
            letter-spacing: 1.2px;
        }
        QLabel#SectionEyebrow {
            color: #e7eaf0;
            font-size: 14px;
            font-weight: 600;
        }
        QLabel#SecurityBadge {
            border: none;
            border-radius: 16px;
            background: #242932;
            color: #b9c0cb;
            font-size: 12px;
            font-weight: 600;
            padding: 0 12px;
        }
        QLabel#SecurityBadge[state="safe"] {
            background: #153026;
            color: #7de2b2;
        }
        QLabel#SecurityBadge[state="active"] {
            background: #352a16;
            color: #f5c76b;
        }
        QLabel#SecurityBadge[state="danger"] {
            background: #3b2023;
            color: #ff9ca3;
        }
        QFrame#WarningCard {
            border: none;
            border-radius: 11px;
            background: #14231d;
        }
        QFrame#WarningCard[state="active"] {
            background: #282116;
        }
        QFrame#WarningCard[state="danger"] {
            background: #2c191c;
        }
        QLabel#WarningIcon {
            border-radius: 14px;
            background: #48c78e;
            color: #09120e;
            font-size: 15px;
            font-weight: 700;
        }
        QFrame#WarningCard[state="active"] QLabel#WarningIcon {
            background: #f0b94d;
            color: #211805;
        }
        QFrame#WarningCard[state="danger"] QLabel#WarningIcon {
            background: #ff7079;
            color: #20080a;
        }
        QLabel#WarningTitle {
            color: #f1f3f7;
            font-size: 14px;
            font-weight: 600;
        }
        QLabel#WarningDetail {
            color: #a0a7b2;
            font-size: 12px;
        }
        QFrame#Card, QFrame#DropCard {
            border: 1px solid #252a33;
            border-radius: 12px;
            background: #16191f;
        }
        QFrame#DropCard {
            background: #15191f;
            border: 1px dashed #343b47;
        }
        QFrame#DropCard[dragging="true"] {
            border: 1px solid #5aa9ff;
            background: #172333;
        }
        QLabel#DemoIcon {
            border: 1px solid #293343;
            border-radius: 12px;
            background: #1b2430;
        }
        QLabel#PrimaryText {
            color: #f1f3f7;
            font-size: 16px;
            font-weight: 600;
        }
        QLabel#SecondaryText, QLabel#PathText, QLabel#StatusText {
            color: #8c94a1;
            font-size: 12px;
        }
        QFrame#SettingsRow {
            border: none;
            border-radius: 8px;
            background: #1c2027;
        }
        QLabel#FieldLabel {
            color: #7f8794;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel#VpkStatus {
            color: #c9ced7;
            font-size: 13px;
            font-weight: 600;
        }
        QLabel#VpkStatus[installed="true"] { color: #72d9a7; }
        QLabel#VpkStatus[installed="false"] { color: #d9b26d; }
        QLabel#StatusText {
            border: none;
            border-radius: 7px;
            background: #1c2027;
            color: #a8afba;
            padding: 9px 12px;
        }
        QLabel#StatusText[state="safe"] { color: #78d9aa; }
        QLabel#StatusText[state="active"] { color: #e9bf6d; }
        QLabel#StatusText[state="danger"] { color: #ff969d; }
        QPushButton {
            min-height: 36px;
            border-radius: 8px;
            padding: 0 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#PrimaryButton {
            border: none;
            background: #4f9cf9;
            color: #ffffff;
            font-size: 15px;
        }
        QPushButton#PrimaryButton:hover { background: #73b7ff; }
        QPushButton#PrimaryButton:pressed { background: #4697ee; }
        QPushButton#StopButton {
            border: 1px solid #343a45;
            background: #20242b;
            color: #cbd0d9;
        }
        QPushButton#StopButton:hover { background: #292e37; border-color: #4c5563; }
        QPushButton#SecondaryButton, QPushButton#GhostButton {
            border: 1px solid #343a45;
            background: #24282f;
            color: #d8dce4;
        }
        QPushButton#SecondaryButton:hover, QPushButton#GhostButton:hover {
            border-color: #596473;
            background: #2d323b;
        }
        QPushButton#GhostButton { min-width: 58px; padding: 0 10px; }
        QPushButton:disabled {
            border-color: #23272e;
            background: #1a1d22;
            color: #505762;
        }
        QPushButton#PrimaryButton:disabled,
        QPushButton#StopButton:disabled,
        QPushButton#SecondaryButton:disabled,
        QPushButton#GhostButton:disabled {
            border: 1px solid #23272e;
            background: #1a1d22;
            color: #505762;
        }
        QLabel#FooterText {
            color: #59616d;
            font-size: 11px;
        }
        QMessageBox { background: #16191f; }
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
    demoMeta_->setText(QStringLiteral("%1  /  %2").arg(Cs2Manager::displayFileSize(info.size()), QDir::toNativeSeparators(info.absolutePath())));
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    settings.setValue(QStringLiteral("lastDemo"), demoPath_);
    settings.setValue(QStringLiteral("lastDemoDirectory"), info.absolutePath());
    lastStatus_ = QStringLiteral("Demo 已选择。点击“开始观看 Demo”会自动安装菜单并启动 CS2。");
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
        setSecurityState(QStringLiteral("active"), QStringLiteral("菜单已准备"), QStringLiteral("可以开始观看 Demo。若要正常游戏，请先点击“停止观看并恢复”。"));
        statusLabel_->setText(lastStatus_.isEmpty() ? QStringLiteral("菜单已准备好。") : lastStatus_);
    } else {
        setSecurityState(QStringLiteral("safe"), QStringLiteral("可以正常游戏"), QStringLiteral("当前没有 Demo 会话或菜单覆盖，可以正常启动 CS2。"));
        statusLabel_->setText(lastStatus_.isEmpty() ? QStringLiteral("请选择一个 Demo。") : lastStatus_);
    }
}

void LauncherWindow::repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
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
