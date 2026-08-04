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
#include <QGridLayout>
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
    QPixmap pixmap(56, 56);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(QStringLiteral("#e4ae39")), 2));
    painter.setBrush(QColor(QStringLiteral("#26272a")));
    painter.drawRoundedRect(QRectF(2, 2, 52, 52), 10, 10);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#e4ae39")));
    QPolygonF triangle;
    triangle << QPointF(23, 17) << QPointF(23, 39) << QPointF(41, 28);
    painter.drawPolygon(triangle);
    painter.setBrush(QColor(QStringLiteral("#78b9dc")));
    painter.drawEllipse(QPointF(15, 28), 3.5, 3.5);
    return pixmap;
}

QLabel *sectionEyebrow(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("SectionEyebrow"));
    return label;
}

QFrame *horizontalLine(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("Separator"));
    line->setFixedHeight(1);
    return line;
}
}

LauncherWindow::LauncherWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Swift Demo Launcher"));
    setWindowIcon(QIcon(makeLogo()));
    setMinimumSize(760, 650);
    resize(780, 680);
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
    root->setContentsMargins(30, 26, 30, 26);
    root->setSpacing(14);

    auto *header = new QHBoxLayout;
    header->setSpacing(16);
    auto *logo = new QLabel(central);
    logo->setPixmap(makeLogo());
    logo->setFixedSize(56, 56);
    header->addWidget(logo);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(0);
    auto *title = new QLabel(QStringLiteral("SWIFT DEMO LAUNCHER"), central);
    title->setObjectName(QStringLiteral("AppTitle"));
    auto *subtitle = new QLabel(QStringLiteral("CS2 REVIEW SESSION  /  NATIVE QT"), central);
    subtitle->setObjectName(QStringLiteral("AppSubtitle"));
    titles->addWidget(title);
    titles->addWidget(subtitle);
    header->addLayout(titles, 1);

    securityBadge_ = new QLabel(QStringLiteral("CHECKING"), central);
    securityBadge_->setObjectName(QStringLiteral("SecurityBadge"));
    securityBadge_->setProperty("state", QStringLiteral("neutral"));
    securityBadge_->setAlignment(Qt::AlignCenter);
    securityBadge_->setMinimumWidth(144);
    securityBadge_->setFixedHeight(34);
    header->addWidget(securityBadge_, 0, Qt::AlignVCenter);
    root->addLayout(header);

    warningCard_ = new QFrame(central);
    warningCard_->setObjectName(QStringLiteral("WarningCard"));
    warningCard_->setProperty("state", QStringLiteral("ready"));
    auto *warningLayout = new QHBoxLayout(warningCard_);
    warningLayout->setContentsMargins(16, 13, 16, 13);
    warningLayout->setSpacing(13);
    auto *warningIcon = new QLabel(QStringLiteral("!"), warningCard_);
    warningIcon->setObjectName(QStringLiteral("WarningIcon"));
    warningIcon->setAlignment(Qt::AlignCenter);
    warningIcon->setFixedSize(34, 34);
    warningLayout->addWidget(warningIcon);
    auto *warningCopy = new QVBoxLayout;
    warningCopy->setSpacing(2);
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
    dropLayout->setContentsMargins(18, 15, 18, 15);
    dropLayout->setSpacing(15);
    auto *fileIcon = new QLabel(dropCard_);
    fileIcon->setPixmap(style()->standardIcon(QStyle::SP_FileIcon).pixmap(30, 30));
    fileIcon->setFixedSize(34, 34);
    dropLayout->addWidget(fileIcon);
    auto *fileCopy = new QVBoxLayout;
    fileCopy->setSpacing(2);
    fileCopy->addWidget(sectionEyebrow(QStringLiteral("1  SELECT DEMO"), dropCard_));
    demoName_ = new QLabel(QStringLiteral("拖放 .dem 文件到这里，或点击选择"), dropCard_);
    demoName_->setObjectName(QStringLiteral("PrimaryText"));
    demoMeta_ = new QLabel(QStringLiteral("尚未选择 Demo"), dropCard_);
    demoMeta_->setObjectName(QStringLiteral("SecondaryText"));
    fileCopy->addWidget(demoName_);
    fileCopy->addWidget(demoMeta_);
    dropLayout->addLayout(fileCopy, 1);
    chooseDemoButton_ = new QPushButton(QStringLiteral("选择文件"), dropCard_);
    chooseDemoButton_->setObjectName(QStringLiteral("SecondaryButton"));
    chooseDemoButton_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    connect(chooseDemoButton_, &QPushButton::clicked, this, &LauncherWindow::chooseDemo);
    dropLayout->addWidget(chooseDemoButton_);
    root->addWidget(dropCard_);

    auto *environmentCard = new QFrame(central);
    environmentCard->setObjectName(QStringLiteral("Card"));
    auto *environment = new QGridLayout(environmentCard);
    environment->setContentsMargins(18, 15, 18, 15);
    environment->setHorizontalSpacing(14);
    environment->setVerticalSpacing(8);
    auto *environmentHeading = sectionEyebrow(QStringLiteral("2  GAME & MENU"), environmentCard);
    environment->addWidget(environmentHeading, 0, 0, 1, 3);
    auto *pathCaption = new QLabel(QStringLiteral("CS2 路径"), environmentCard);
    pathCaption->setObjectName(QStringLiteral("FieldLabel"));
    environment->addWidget(pathCaption, 1, 0);
    cs2Path_ = new QLabel(QStringLiteral("正在检测..."), environmentCard);
    cs2Path_->setObjectName(QStringLiteral("PathText"));
    cs2Path_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    environment->addWidget(cs2Path_, 1, 1);
    chooseCs2Button_ = new QPushButton(QStringLiteral("更改"), environmentCard);
    chooseCs2Button_->setObjectName(QStringLiteral("GhostButton"));
    connect(chooseCs2Button_, &QPushButton::clicked, this, &LauncherWindow::chooseCs2Directory);
    environment->addWidget(chooseCs2Button_, 1, 2);

    auto *vpkCaption = new QLabel(QStringLiteral("菜单 VPK"), environmentCard);
    vpkCaption->setObjectName(QStringLiteral("FieldLabel"));
    environment->addWidget(vpkCaption, 2, 0);
    vpkStatus_ = new QLabel(QStringLiteral("正在检查..."), environmentCard);
    vpkStatus_->setObjectName(QStringLiteral("VpkStatus"));
    environment->addWidget(vpkStatus_, 2, 1);
    installButton_ = new QPushButton(QStringLiteral("安装 / 修复"), environmentCard);
    installButton_->setObjectName(QStringLiteral("SecondaryButton"));
    connect(installButton_, &QPushButton::clicked, this, &LauncherWindow::installOrRepairMenu);
    environment->addWidget(installButton_, 2, 2);
    environment->setColumnStretch(1, 1);
    root->addWidget(environmentCard);

    auto *actionCard = new QFrame(central);
    actionCard->setObjectName(QStringLiteral("Card"));
    auto *actions = new QVBoxLayout(actionCard);
    actions->setContentsMargins(18, 15, 18, 17);
    actions->setSpacing(11);
    actions->addWidget(sectionEyebrow(QStringLiteral("3  REVIEW SESSION"), actionCard));
    auto *actionRow = new QHBoxLayout;
    actionRow->setSpacing(10);
    startButton_ = new QPushButton(QStringLiteral("开始观看 DEMO"), actionCard);
    startButton_->setObjectName(QStringLiteral("PrimaryButton"));
    startButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    startButton_->setMinimumHeight(48);
    connect(startButton_, &QPushButton::clicked, this, &LauncherWindow::startWatchingDemo);
    actionRow->addWidget(startButton_, 3);
    stopButton_ = new QPushButton(QStringLiteral("停止观看 DEMO"), actionCard);
    stopButton_->setObjectName(QStringLiteral("StopButton"));
    stopButton_->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    stopButton_->setMinimumHeight(48);
    connect(stopButton_, &QPushButton::clicked, this, &LauncherWindow::stopWatchingDemo);
    actionRow->addWidget(stopButton_, 2);
    actions->addLayout(actionRow);
    actions->addWidget(horizontalLine(actionCard));
    statusLabel_ = new QLabel(QStringLiteral("正在检查环境..."), actionCard);
    statusLabel_->setObjectName(QStringLiteral("StatusText"));
    statusLabel_->setWordWrap(true);
    actions->addWidget(statusLabel_);
    root->addWidget(actionCard);

    auto *footer = new QLabel(QStringLiteral("启动参数仅用于本次会话，不会写入 Steam 永久启动项。"), central);
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
            background: #111214;
            color: #e8e8e8;
            font-family: "Segoe UI", "Microsoft YaHei UI";
            font-size: 14px;
        }
        QLabel#AppTitle {
            color: #f3f3f3;
            font-size: 24px;
            font-weight: 700;
            letter-spacing: 1px;
        }
        QLabel#AppSubtitle, QLabel#SectionEyebrow {
            color: #e4ae39;
            font-size: 11px;
            font-weight: 700;
            letter-spacing: 1px;
        }
        QLabel#SecurityBadge {
            border-radius: 3px;
            border: 1px solid #5d6164;
            background: #292b2e;
            color: #c9cbcc;
            font-size: 11px;
            font-weight: 700;
            letter-spacing: 1px;
            padding: 0 12px;
        }
        QLabel#SecurityBadge[state="safe"] {
            border-color: #5f8d72;
            background: #1d3025;
            color: #9fd6b2;
        }
        QLabel#SecurityBadge[state="active"] {
            border-color: #c58b32;
            background: #3a2a18;
            color: #f1bd60;
        }
        QLabel#SecurityBadge[state="danger"] {
            border-color: #b65c51;
            background: #3b211f;
            color: #ef9c92;
        }
        QFrame#WarningCard {
            border: 1px solid #3d5144;
            border-left: 3px solid #6da17e;
            border-radius: 4px;
            background: #18221c;
        }
        QFrame#WarningCard[state="active"] {
            border-color: #70542d;
            border-left-color: #e4ae39;
            background: #2b2419;
        }
        QFrame#WarningCard[state="danger"] {
            border-color: #75433e;
            border-left-color: #d36d61;
            background: #2d1d1b;
        }
        QLabel#WarningIcon {
            border-radius: 17px;
            background: #e4ae39;
            color: #181818;
            font-size: 20px;
            font-weight: 800;
        }
        QLabel#WarningTitle {
            color: #f0f0f0;
            font-size: 15px;
            font-weight: 700;
        }
        QLabel#WarningDetail {
            color: #adaeaf;
            font-size: 12px;
        }
        QFrame#Card, QFrame#DropCard {
            border: 1px solid #343639;
            border-radius: 5px;
            background: #1b1c1f;
        }
        QFrame#DropCard {
            border: 1px dashed #5a5d60;
        }
        QFrame#DropCard[dragging="true"] {
            border: 1px solid #e4ae39;
            background: #262319;
        }
        QLabel#PrimaryText {
            color: #efefef;
            font-size: 16px;
            font-weight: 650;
        }
        QLabel#SecondaryText, QLabel#PathText, QLabel#StatusText {
            color: #9a9da0;
            font-size: 12px;
        }
        QLabel#FieldLabel {
            color: #777b7e;
            font-size: 12px;
            font-weight: 650;
        }
        QLabel#VpkStatus {
            color: #c9cbcc;
            font-size: 12px;
            font-weight: 650;
        }
        QLabel#VpkStatus[installed="true"] { color: #8fcba4; }
        QLabel#VpkStatus[installed="false"] { color: #d0a158; }
        QFrame#Separator { background: #303235; border: none; }
        QPushButton {
            min-height: 34px;
            border-radius: 3px;
            padding: 0 14px;
            font-size: 12px;
            font-weight: 650;
        }
        QPushButton#PrimaryButton {
            border: 1px solid #e4ae39;
            border-left: 3px solid #f0be52;
            background: #4b3a1c;
            color: #fff0c9;
            font-size: 14px;
        }
        QPushButton#PrimaryButton:hover { background: #614a23; }
        QPushButton#PrimaryButton:pressed { background: #372b18; }
        QPushButton#StopButton {
            border: 1px solid #6f4541;
            background: #31201f;
            color: #e2a39d;
            font-size: 13px;
        }
        QPushButton#StopButton:hover { background: #472825; border-color: #b65c51; }
        QPushButton#SecondaryButton, QPushButton#GhostButton {
            border: 1px solid #505357;
            background: #292b2e;
            color: #d0d2d3;
        }
        QPushButton#SecondaryButton:hover, QPushButton#GhostButton:hover {
            border-color: #8b8f92;
            background: #343639;
        }
        QPushButton#GhostButton { min-width: 58px; padding: 0 10px; }
        QPushButton:disabled {
            border-color: #343638;
            background: #202124;
            color: #5e6164;
        }
        QLabel#FooterText {
            color: #65686b;
            font-size: 11px;
        }
        QMessageBox { background: #1b1c1f; }
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
    securityBadge_->setText(title);
    warningTitle_->setText(title);
    warningDetail_->setText(detail);
    repolish(securityBadge_);
    repolish(warningCard_);
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
    vpkStatus_->setText(!valid ? QStringLiteral("等待 CS2 路径")
                               : installed ? QStringLiteral("● 已安装") : QStringLiteral("○ 尚未安装（启动时会自动安装）"));
    repolish(vpkStatus_);

    chooseDemoButton_->setEnabled(!active);
    chooseCs2Button_->setEnabled(!running && !active);
    installButton_->setEnabled(valid && !running && !active);
    startButton_->setEnabled(valid && !demoPath_.isEmpty() && !running && !active);
    stopButton_->setEnabled(valid && !running && (active || installed));

    if (!valid) {
        setSecurityState(QStringLiteral("danger"), QStringLiteral("SETUP REQUIRED"), QStringLiteral("未找到有效的 CS2 安装目录，请点击“更改”手动选择。"));
        statusLabel_->setText(lastStatus_);
    } else if (active && running) {
        setSecurityState(QStringLiteral("active"), QStringLiteral("DEMO MODE ACTIVE"), QStringLiteral("CS2 正在以 -insecure 运行。请勿进入正常匹配；观看结束后先退出游戏。"));
        statusLabel_->setText(QStringLiteral("Demo 会话进行中。游戏退出后，请回到这里点击“停止观看 Demo”。"));
    } else if (active && !running) {
        setSecurityState(QStringLiteral("danger"), QStringLiteral("CLEANUP REQUIRED"), QStringLiteral("CS2 已退出，但 Demo 资源仍处于启用状态。点击“停止观看 Demo”恢复正常游戏环境。"));
        statusLabel_->setText(QStringLiteral("现在可以点击“停止观看 Demo”完成清理。"));
    } else if (running) {
        setSecurityState(QStringLiteral("active"), QStringLiteral("CS2 RUNNING"), QStringLiteral("当前 CS2 不是由本次 Demo 会话启动。请先退出游戏，才能开始 Demo 观看模式。"));
        statusLabel_->setText(QStringLiteral("等待 CS2 退出。"));
    } else if (installed) {
        setSecurityState(QStringLiteral("active"), QStringLiteral("MENU INSTALLED"), QStringLiteral("菜单 VPK 已启用。可以开始观看 Demo；若要正常游戏，请先点击“停止观看 Demo”。"));
        statusLabel_->setText(lastStatus_.isEmpty() ? QStringLiteral("菜单已准备好。") : lastStatus_);
    } else {
        setSecurityState(QStringLiteral("safe"), QStringLiteral("SAFE / READY"), QStringLiteral("当前没有 -insecure Demo 会话，也没有启用菜单覆盖，可以正常启动 CS2。"));
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
