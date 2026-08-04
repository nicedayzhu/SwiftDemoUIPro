#include "Cs2Manager.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

class LauncherCoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesSteamLibraries();
    void installsSearchPathIdempotently();
    void refusesUnknownGameInfoShape();
    void removesOnlyOwnedSearchPath();
    void buildsTemporaryInsecureLaunch();
    void installsAndPreparesIsolatedSession();
};

void LauncherCoreTest::parsesSteamLibraries()
{
    const QString vdf = QStringLiteral(
        R"("libraryfolders"
{
    "0" { "path" "C:\\Program Files (x86)\\Steam" }
    "1" { "path" "F:\\SteamLibrary" }
    "2" { "path" "F:\\SteamLibrary" }
})");
    const QStringList paths = Cs2Manager::parseSteamLibraryFolders(vdf);
    QCOMPARE(paths.size(), 2);
    QCOMPARE(QDir::toNativeSeparators(paths[1]), QStringLiteral("F:\\SteamLibrary"));
}

void LauncherCoreTest::installsSearchPathIdempotently()
{
    const QString original = QStringLiteral(
        "\"GameInfo\"\r\n"
        "{\r\n"
        "\tFileSystem\r\n"
        "\t{\r\n"
        "\t\tSearchPaths\r\n"
        "\t\t{\r\n"
        "\t\t\tGame\tcsgo\r\n"
        "\t\t}\r\n"
        "\t}\r\n"
        "}\r\n");
    bool changed = false;
    QString error;
    const QString installed = Cs2Manager::addOverrideSearchPath(original, &changed, &error);
    QVERIFY2(changed, qPrintable(error));
    QVERIFY(installed.contains(QStringLiteral("Game\tcsgo/overrides/swift_demo_menu_override.vpk\r\n\t\t\tGame\tcsgo")));

    const QString secondPass = Cs2Manager::addOverrideSearchPath(installed, &changed, &error);
    QVERIFY(!changed);
    QCOMPARE(secondPass, installed);
    QCOMPARE(installed.count(QStringLiteral("csgo/overrides/swift_demo_menu_override.vpk")), 1);
}

void LauncherCoreTest::refusesUnknownGameInfoShape()
{
    bool changed = false;
    QString error;
    const QString original = QStringLiteral("not a Source 2 gameinfo file\n");
    QCOMPARE(Cs2Manager::addOverrideSearchPath(original, &changed, &error), original);
    QVERIFY(!changed);
    QVERIFY(!error.isEmpty());
}

void LauncherCoreTest::removesOnlyOwnedSearchPath()
{
    const QString installed = QStringLiteral(
        "\tGame\tcsgo/overrides/some_other_mod.vpk\n"
        "\tGame\tcsgo/overrides/swift_demo_menu_override.vpk\n"
        "\tGame\tcsgo\n");
    bool changed = false;
    const QString cleaned = Cs2Manager::removeOverrideSearchPath(installed, &changed);
    QVERIFY(changed);
    QVERIFY(cleaned.contains(QStringLiteral("some_other_mod.vpk")));
    QVERIFY(!cleaned.contains(QStringLiteral("swift_demo_menu_override.vpk")));
}

void LauncherCoreTest::buildsTemporaryInsecureLaunch()
{
    const QStringList arguments = Cs2Manager::buildSteamLaunchArguments();
    QCOMPARE(arguments.mid(0, 2), QStringList({ QStringLiteral("-applaunch"), QStringLiteral("730") }));
    QVERIFY(arguments.contains(QStringLiteral("-insecure")));
    QVERIFY(arguments.contains(QStringLiteral("+exec")));
    QVERIFY(!arguments.join(QLatin1Char(' ')).contains(QStringLiteral("-launchoption"), Qt::CaseInsensitive));

    const QString cfg = Cs2Manager::buildDemoCfg();
    QVERIFY(cfg.contains(QStringLiteral("playdemo \"demos/swift_demo_launcher/current.dem\"")));
    QVERIFY(cfg.contains(QStringLiteral("tv_listen_voice_indices -1")));
}

void LauncherCoreTest::installsAndPreparesIsolatedSession()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QDir root(temporary.path());
    QVERIFY(root.mkpath(QStringLiteral("game/bin/win64")));
    QVERIFY(root.mkpath(QStringLiteral("game/csgo")));

    QFile cs2(root.filePath(QStringLiteral("game/bin/win64/cs2.exe")));
    QVERIFY(cs2.open(QIODevice::WriteOnly));
    cs2.close();

    QFile gameInfo(root.filePath(QStringLiteral("game/csgo/gameinfo.gi")));
    QVERIFY(gameInfo.open(QIODevice::WriteOnly));
    QVERIFY(gameInfo.write("\"GameInfo\"\r\n{\r\n\tFileSystem\r\n\t{\r\n\t\tSearchPaths\r\n\t\t{\r\n\t\t\tGame\tcsgo\r\n\t\t}\r\n\t}\r\n}\r\n") > 0);
    gameInfo.close();

    QFile sourceVpk(root.filePath(QStringLiteral("source.vpk")));
    QVERIFY(sourceVpk.open(QIODevice::WriteOnly));
    QVERIFY(sourceVpk.write("test-vpk") > 0);
    sourceVpk.close();

    QFile sourceDemo(root.filePath(QStringLiteral("match.dem")));
    QVERIFY(sourceDemo.open(QIODevice::WriteOnly));
    QVERIFY(sourceDemo.write("test-demo") > 0);
    sourceDemo.close();

    QString error;
    const Cs2Paths paths = Cs2Manager::fromSelection(temporary.path(), &error);
    QVERIFY2(paths.isValid(), qPrintable(error));
    const LauncherResult install = Cs2Manager::installOverride(paths, sourceVpk.fileName());
    QVERIFY2(install.ok, qPrintable(install.message));
    QVERIFY(Cs2Manager::isOverrideInstalled(paths));
    QVERIFY(QFileInfo::exists(paths.gameInfo + QStringLiteral(".swift_demo_launcher.restore.bak")));

    const LauncherResult prepared = Cs2Manager::prepareDemoSession(paths, sourceDemo.fileName());
    QVERIFY2(prepared.ok, qPrintable(prepared.message));
    QVERIFY(Cs2Manager::isSessionActive(paths));
    QVERIFY(QFileInfo::exists(root.filePath(QStringLiteral("game/csgo/cfg/swift_demo_launcher.cfg"))));
    QVERIFY(QFileInfo::exists(root.filePath(QStringLiteral("game/csgo/demos/swift_demo_launcher/current.dem"))));
}

QTEST_APPLESS_MAIN(LauncherCoreTest)
#include "tst_launcher_core.moc"
