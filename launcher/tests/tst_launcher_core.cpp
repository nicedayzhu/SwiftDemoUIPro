#include "Cs2Manager.h"
#include "UpdateService.h"

#include "miniz.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

namespace
{
bool createZip(const QString &path, const QList<QPair<QByteArray, QByteArray>> &files)
{
    mz_zip_archive archive {};
    mz_zip_zero_struct(&archive);
    const QByteArray encodedPath = QFile::encodeName(path);
    if (!mz_zip_writer_init_file(&archive, encodedPath.constData(), 0))
        return false;

    bool ok = true;
    for (const auto &file : files) {
        if (!mz_zip_writer_add_mem(
                &archive,
                file.first.constData(),
                file.second.constData(),
                static_cast<size_t>(file.second.size()),
                MZ_DEFAULT_COMPRESSION)) {
            ok = false;
            break;
        }
    }
    if (ok)
        ok = mz_zip_writer_finalize_archive(&archive) == MZ_TRUE;
    const bool ended = mz_zip_writer_end(&archive) == MZ_TRUE;
    return ok && ended;
}
}

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
    void listsAndPreparesDemoFromZip();
    void rejectsInvalidDemoArchives();
    void parsesIndependentReleaseUpdates();
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
    QVERIFY(cfg.contains(QStringLiteral("cl_demo_predict 0")));

    const QString trueViewCfg = Cs2Manager::buildDemoCfg(true);
    QVERIFY(trueViewCfg.contains(QStringLiteral("cl_demo_predict 1")));
    QVERIFY(!trueViewCfg.contains(QStringLiteral("cl_demo_predict 0")));
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
    QFile generatedCfg(root.filePath(QStringLiteral("game/csgo/cfg/swift_demo_launcher.cfg")));
    QVERIFY(generatedCfg.open(QIODevice::ReadOnly));
    QVERIFY(generatedCfg.readAll().contains("cl_demo_predict 0"));
    QVERIFY(QFileInfo::exists(root.filePath(QStringLiteral("game/csgo/demos/swift_demo_launcher/current.dem"))));
}

void LauncherCoreTest::listsAndPreparesDemoFromZip()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QDir root(temporary.path());
    QVERIFY(root.mkpath(QStringLiteral("game/csgo")));

    const QString archivePath = root.filePath(QStringLiteral("downloaded matches.zip"));
    const QString unicodeDemo = QString::fromUtf8("比赛.dem");
    QVERIFY(createZip(archivePath, {
        { QByteArrayLiteral("readme.txt"), QByteArrayLiteral("not a demo") },
        { QByteArrayLiteral("nested/first.DEM"), QByteArrayLiteral("first-demo") },
        { unicodeDemo.toUtf8(), QByteArrayLiteral("selected-demo") }
    }));

    QList<DemoArchiveEntry> entries;
    const LauncherResult inspected = Cs2Manager::inspectDemoArchive(archivePath, &entries);
    QVERIFY2(inspected.ok, qPrintable(inspected.message));
    QCOMPARE(entries.size(), 2);
    QVERIFY(std::any_of(entries.cbegin(), entries.cend(), [](const DemoArchiveEntry &entry) {
        return entry.path == QStringLiteral("nested/first.DEM") && entry.size == 10;
    }));
    QVERIFY(std::any_of(entries.cbegin(), entries.cend(), [&unicodeDemo](const DemoArchiveEntry &entry) {
        return entry.path == unicodeDemo && entry.size == 13;
    }));

    Cs2Paths paths;
    paths.csgoDir = root.filePath(QStringLiteral("game/csgo"));
    const LauncherResult prepared = Cs2Manager::prepareDemoSession(paths, archivePath, unicodeDemo);
    QVERIFY2(prepared.ok, qPrintable(prepared.message));

    QFile staged(root.filePath(QStringLiteral("game/csgo/demos/swift_demo_launcher/current.dem")));
    QVERIFY(staged.open(QIODevice::ReadOnly));
    QCOMPARE(staged.readAll(), QByteArrayLiteral("selected-demo"));

    QFile marker(root.filePath(QStringLiteral("game/csgo/overrides/.swift_demo_launcher_active")));
    QVERIFY(marker.open(QIODevice::ReadOnly));
    QVERIFY(marker.readAll().contains(unicodeDemo.toUtf8()));
}

void LauncherCoreTest::rejectsInvalidDemoArchives()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QDir root(temporary.path());

    const QString noDemoPath = root.filePath(QStringLiteral("no-demo.zip"));
    QVERIFY(createZip(noDemoPath, {
        { QByteArrayLiteral("readme.txt"), QByteArrayLiteral("not a demo") }
    }));
    QList<DemoArchiveEntry> entries;
    const LauncherResult noDemo = Cs2Manager::inspectDemoArchive(noDemoPath, &entries);
    QVERIFY(!noDemo.ok);
    QVERIFY(entries.isEmpty());

    const QString corruptPath = root.filePath(QStringLiteral("corrupt.zip"));
    QFile corrupt(corruptPath);
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    QVERIFY(corrupt.write("this is not a zip") > 0);
    corrupt.close();
    const LauncherResult corruptResult = Cs2Manager::inspectDemoArchive(corruptPath, &entries);
    QVERIFY(!corruptResult.ok);

    const QString duplicatePath = root.filePath(QStringLiteral("duplicates.zip"));
    QVERIFY(createZip(duplicatePath, {
        { QByteArrayLiteral("same.dem"), QByteArrayLiteral("first") },
        { QByteArrayLiteral("same.dem"), QByteArrayLiteral("second") }
    }));
    const LauncherResult duplicateResult = Cs2Manager::inspectDemoArchive(duplicatePath, &entries);
    QVERIFY(!duplicateResult.ok);
}

void LauncherCoreTest::parsesIndependentReleaseUpdates()
{
    const QByteArray releaseJson = R"JSON({
        "html_url": "https://github.com/nicedayzhu/SwiftDemoUIPro/releases/tag/menu-v0.1.1",
        "assets": [
            {
                "name": "update-manifest.json",
                "browser_download_url": "https://github.com/nicedayzhu/SwiftDemoUIPro/releases/download/menu-v0.1.1/update-manifest.json"
            }
        ]
    })JSON";
    const QByteArray manifestJson = R"JSON({
        "schemaVersion": 1,
        "launcher": {
            "version": "0.1.0",
            "url": "https://github.com/nicedayzhu/SwiftDemoUIPro/releases/download/v0.1.0/SwiftDemoUIPro-v0.1.0-win64.zip",
            "sha256": "c2e0c4604e9b2f1787963eba32bcfdd104e40ba30182729ad1e8055de3dd696f"
        },
        "menu": {
            "version": "0.1.1",
            "url": "https://github.com/nicedayzhu/SwiftDemoUIPro/releases/download/menu-v0.1.1/swift_demo_menu_override-v0.1.1.vpk",
            "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        }
    })JSON";

    const UpdateInfo independent = UpdateService::parseLatestRelease(releaseJson, manifestJson);
    QVERIFY2(independent.valid, qPrintable(independent.error));
    QCOMPARE(independent.launcher.version, QStringLiteral("0.1.0"));
    QCOMPARE(independent.menu.version, QStringLiteral("0.1.1"));
    QVERIFY(!independent.launcher.isNewerThan(QStringLiteral("0.1.0")));
    QVERIFY(independent.menu.isNewerThan(QStringLiteral("0.1.0")));

    const QByteArray legacyRelease = R"JSON({
        "html_url": "https://github.com/nicedayzhu/SwiftDemoUIPro/releases/tag/v0.2.0",
        "assets": [
            {
                "name": "SwiftDemoUIPro-v0.2.0-win64.zip",
                "browser_download_url": "https://github.com/nicedayzhu/SwiftDemoUIPro/releases/download/v0.2.0/SwiftDemoUIPro-v0.2.0-win64.zip",
                "digest": "sha256:abcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcd"
            }
        ]
    })JSON";
    const UpdateInfo legacy = UpdateService::parseLatestRelease(legacyRelease);
    QVERIFY2(legacy.valid, qPrintable(legacy.error));
    QCOMPARE(legacy.launcher.version, QStringLiteral("0.2.0"));
    QVERIFY(!legacy.menu.isValid());
}

QTEST_APPLESS_MAIN(LauncherCoreTest)
#include "tst_launcher_core.moc"
