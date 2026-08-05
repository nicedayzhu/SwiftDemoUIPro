#pragma once

#include "Cs2Manager.h"

#include <QMainWindow>

class QCloseEvent;
class QCheckBox;
class QComboBox;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QFrame;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class QTranslator;

class LauncherWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit LauncherWindow(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void chooseDemo();
    void chooseCs2Directory();
    void installOrRepairMenu();
    void startWatchingDemo();
    void stopWatchingDemo();
    void refreshState();

private:
    void buildInterface();
    void applyStyle();
    void detectEnvironment();
    void setDemoPath(const QString &path, const QString &preferredArchiveEntry = {});
    void refreshDemoDetails();
    void showResult(const LauncherResult &result, bool dialogOnFailure = true);
    void setSecurityState(const QString &state, const QString &title, const QString &detail);
    bool loadLanguage(const QString &language);
    void changeLanguage(const QString &language);
    void selectPage(int index);
    void repolish(QWidget *widget);

    Cs2Paths paths_;
    QString demoPath_;
    QString demoArchiveEntry_;
    qint64 demoSize_ = 0;
    QString lastStatus_;
    QString currentLanguage_;
    bool trueViewEnabled_ = false;

    QLabel *securityBadge_ = nullptr;
    QFrame *warningCard_ = nullptr;
    QLabel *warningIcon_ = nullptr;
    QLabel *warningTitle_ = nullptr;
    QLabel *warningDetail_ = nullptr;
    QFrame *dropCard_ = nullptr;
    QLabel *demoName_ = nullptr;
    QLabel *demoMeta_ = nullptr;
    QLabel *cs2Path_ = nullptr;
    QLabel *vpkStatus_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QPushButton *chooseDemoButton_ = nullptr;
    QPushButton *chooseCs2Button_ = nullptr;
    QPushButton *installButton_ = nullptr;
    QPushButton *startButton_ = nullptr;
    QPushButton *stopButton_ = nullptr;
    QCheckBox *trueViewCheckBox_ = nullptr;
    QPushButton *navReplayButton_ = nullptr;
    QPushButton *navMenuButton_ = nullptr;
    QPushButton *navAboutButton_ = nullptr;
    QComboBox *languageCombo_ = nullptr;
    QStackedWidget *pages_ = nullptr;
    QTimer *stateTimer_ = nullptr;
    QTranslator *translator_ = nullptr;
};
