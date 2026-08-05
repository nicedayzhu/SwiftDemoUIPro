#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct Cs2Paths
{
    QString steamRoot;
    QString steamExe;
    QString cs2Root;
    QString csgoDir;
    QString cs2Exe;
    QString gameInfo;

    bool isValid() const;
};

struct LauncherResult
{
    bool ok = false;
    QString message;

    static LauncherResult success(const QString &message = {});
    static LauncherResult failure(const QString &message);
};

struct DemoArchiveEntry
{
    QString path;
    qint64 size = 0;
};

class Cs2Manager
{
public:
    static constexpr const char *kVpkName = "swift_demo_menu_override.vpk";
    static constexpr const char *kCfgName = "swift_demo_launcher.cfg";
    static constexpr const char *kSessionMarker = ".swift_demo_launcher_active";

    static Cs2Paths detect(const QString &preferredRoot = {}, QString *error = nullptr);
    static Cs2Paths fromSelection(const QString &selectedPath, QString *error = nullptr);
    static QString findBundledVpk();

    static bool isCs2Running();
    static bool isOverrideInstalled(const Cs2Paths &paths);
    static bool isSessionActive(const Cs2Paths &paths);

    static LauncherResult installOverride(const Cs2Paths &paths, const QString &sourceVpk);
    static LauncherResult inspectDemoArchive(const QString &archivePath, QList<DemoArchiveEntry> *entries);
    static LauncherResult prepareDemoSession(const Cs2Paths &paths, const QString &demoPath, const QString &archiveEntry = {});
    static LauncherResult removeDemoSession(const Cs2Paths &paths);
    static LauncherResult launchDemo(const Cs2Paths &paths);

    // Pure helpers kept public so the risky file transformations are unit-testable.
    static QStringList parseSteamLibraryFolders(const QString &vdfText);
    static QString addOverrideSearchPath(const QString &gameInfoText, bool *changed, QString *error);
    static QString removeOverrideSearchPath(const QString &gameInfoText, bool *changed);
    static QString buildDemoCfg();
    static QStringList buildSteamLaunchArguments();
    static QString displayFileSize(qint64 bytes);

private:
    static QString overrideSearchPath();
};
