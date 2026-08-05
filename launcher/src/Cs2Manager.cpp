#include "Cs2Manager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QSet>
#include <QVector>

#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace
{
enum class TextEncoding
{
    Utf8,
    Utf8Bom,
    Utf16Le,
    Utf16Be
};

QString decodeText(const QByteArray &bytes, TextEncoding *encoding)
{
    if (bytes.startsWith("\xEF\xBB\xBF")) {
        *encoding = TextEncoding::Utf8Bom;
        return QString::fromUtf8(bytes.mid(3));
    }

    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF
        && static_cast<unsigned char>(bytes[1]) == 0xFE) {
        *encoding = TextEncoding::Utf16Le;
        const int count = (bytes.size() - 2) / 2;
        QVector<char16_t> text(count);
        for (int i = 0; i < count; ++i) {
            const int offset = 2 + i * 2;
            text[i] = static_cast<unsigned char>(bytes[offset])
                | (static_cast<char16_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8);
        }
        return QString::fromUtf16(text.constData(), count);
    }

    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE
        && static_cast<unsigned char>(bytes[1]) == 0xFF) {
        *encoding = TextEncoding::Utf16Be;
        const int count = (bytes.size() - 2) / 2;
        QVector<char16_t> text(count);
        for (int i = 0; i < count; ++i) {
            const int offset = 2 + i * 2;
            text[i] = (static_cast<char16_t>(static_cast<unsigned char>(bytes[offset])) << 8)
                | static_cast<unsigned char>(bytes[offset + 1]);
        }
        return QString::fromUtf16(text.constData(), count);
    }

    *encoding = TextEncoding::Utf8;
    return QString::fromUtf8(bytes);
}

QByteArray encodeText(const QString &text, TextEncoding encoding)
{
    if (encoding == TextEncoding::Utf8 || encoding == TextEncoding::Utf8Bom) {
        QByteArray bytes = text.toUtf8();
        if (encoding == TextEncoding::Utf8Bom)
            bytes.prepend("\xEF\xBB\xBF");
        return bytes;
    }

    QByteArray bytes;
    bytes.reserve(2 + text.size() * 2);
    if (encoding == TextEncoding::Utf16Le) {
        bytes.append(QByteArray::fromHex("fffe"));
    } else {
        bytes.append(QByteArray::fromHex("feff"));
    }

    for (QChar character : text) {
        const ushort value = character.unicode();
        if (encoding == TextEncoding::Utf16Le) {
            bytes.append(char(value & 0xFF));
            bytes.append(char((value >> 8) & 0xFF));
        } else {
            bytes.append(char((value >> 8) & 0xFF));
            bytes.append(char(value & 0xFF));
        }
    }
    return bytes;
}

bool readTextFile(const QString &path, QString *text, TextEncoding *encoding, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QCoreApplication::translate("Cs2Manager", "Unable to read %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    *text = decodeText(file.readAll(), encoding);
    return true;
}

bool writeTextFile(const QString &path, const QString &text, TextEncoding encoding, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QCoreApplication::translate("Cs2Manager", "Unable to write %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    if (file.write(encodeText(text, encoding)) < 0 || !file.commit()) {
        *error = QCoreApplication::translate("Cs2Manager", "Failed to save %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    return true;
}

bool filesMatch(const QString &firstPath, const QString &secondPath)
{
    QFile first(firstPath);
    QFile second(secondPath);
    if (!first.open(QIODevice::ReadOnly) || !second.open(QIODevice::ReadOnly))
        return false;
    if (first.size() != second.size())
        return false;

    QCryptographicHash firstHash(QCryptographicHash::Sha256);
    QCryptographicHash secondHash(QCryptographicHash::Sha256);
    if (!firstHash.addData(&first) || !secondHash.addData(&second))
        return false;
    return firstHash.result() == secondHash.result();
}

bool copyFileAtomically(const QString &source, const QString &target, QString *error)
{
    const QFileInfo targetInfo(target);
    if (!QDir().mkpath(targetInfo.absolutePath())) {
        *error = QCoreApplication::translate("Cs2Manager", "Unable to create directory: %1").arg(QDir::toNativeSeparators(targetInfo.absolutePath()));
        return false;
    }

    if (QFileInfo::exists(target) && filesMatch(source, target))
        return true;

    const QString temporary = target + QStringLiteral(".part");
    QFile::remove(temporary);
    if (!QFile::copy(source, temporary)) {
        *error = QCoreApplication::translate("Cs2Manager", "Failed to copy file: %1").arg(QDir::toNativeSeparators(target));
        return false;
    }

    if (QFileInfo::exists(target) && !QFile::remove(target)) {
        QFile::remove(temporary);
        *error = QCoreApplication::translate("Cs2Manager", "Unable to replace %1. Make sure CS2 has fully exited.").arg(QDir::toNativeSeparators(target));
        return false;
    }
    if (!QFile::rename(temporary, target)) {
        QFile::remove(temporary);
        *error = QCoreApplication::translate("Cs2Manager", "Unable to finish installing: %1").arg(QDir::toNativeSeparators(target));
        return false;
    }
    return true;
}

QString normalizedRoot(const QString &candidate)
{
    QDir dir(candidate);
    if (!dir.exists())
        return {};

    for (int depth = 0; depth < 7; ++depth) {
        const QString root = QDir::cleanPath(dir.absolutePath());
        if (QFileInfo::exists(QDir(root).filePath(QStringLiteral("game/bin/win64/cs2.exe")))
            && QFileInfo::exists(QDir(root).filePath(QStringLiteral("game/csgo/gameinfo.gi")))) {
            return root;
        }
        if (!dir.cdUp())
            break;
    }
    return {};
}

Cs2Paths pathsForRoot(const QString &root, const QString &steamRoot)
{
    Cs2Paths paths;
    paths.steamRoot = steamRoot.isEmpty() ? QString() : QDir::cleanPath(steamRoot);
    paths.steamExe = paths.steamRoot.isEmpty() ? QString() : QDir(paths.steamRoot).filePath(QStringLiteral("steam.exe"));
    paths.cs2Root = QDir::cleanPath(root);
    paths.csgoDir = QDir(paths.cs2Root).filePath(QStringLiteral("game/csgo"));
    paths.cs2Exe = QDir(paths.cs2Root).filePath(QStringLiteral("game/bin/win64/cs2.exe"));
    paths.gameInfo = QDir(paths.csgoDir).filePath(QStringLiteral("gameinfo.gi"));
    return paths;
}

QString registrySteamRoot()
{
#ifdef Q_OS_WIN
    QSettings registry(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"), QSettings::NativeFormat);
    QString path = registry.value(QStringLiteral("SteamPath")).toString();
    if (path.isEmpty())
        path = registry.value(QStringLiteral("SteamExe")).toString();
    if (path.endsWith(QStringLiteral("steam.exe"), Qt::CaseInsensitive))
        path = QFileInfo(path).absolutePath();
    if (path.trimmed().isEmpty())
        return {};
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
#else
    return {};
#endif
}

QString markerPath(const Cs2Paths &paths)
{
    return QDir(paths.csgoDir).filePath(QStringLiteral("overrides/") + QString::fromLatin1(Cs2Manager::kSessionMarker));
}

QString targetVpkPath(const Cs2Paths &paths)
{
    return QDir(paths.csgoDir).filePath(QStringLiteral("overrides/") + QString::fromLatin1(Cs2Manager::kVpkName));
}

QString cfgPath(const Cs2Paths &paths)
{
    return QDir(paths.csgoDir).filePath(QStringLiteral("cfg/") + QString::fromLatin1(Cs2Manager::kCfgName));
}

QString stagedDemoPath(const Cs2Paths &paths)
{
    return QDir(paths.csgoDir).filePath(QStringLiteral("demos/swift_demo_launcher/current.dem"));
}
}

bool Cs2Paths::isValid() const
{
    return QFileInfo::exists(cs2Exe) && QFileInfo::exists(gameInfo) && QDir(csgoDir).exists();
}

LauncherResult LauncherResult::success(const QString &message)
{
    return { true, message };
}

LauncherResult LauncherResult::failure(const QString &message)
{
    return { false, message };
}

QStringList Cs2Manager::parseSteamLibraryFolders(const QString &vdfText)
{
    QStringList libraries;
    QSet<QString> seen;
    const QRegularExpression expression(QStringLiteral(R"re("path"\s+"([^"]+)")re"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator matches = expression.globalMatch(vdfText);
    while (matches.hasNext()) {
        QString path = matches.next().captured(1);
        path.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
        path = QDir::cleanPath(QDir::fromNativeSeparators(path));
        const QString key = path.toLower();
        if (!path.isEmpty() && !seen.contains(key)) {
            seen.insert(key);
            libraries.append(path);
        }
    }
    return libraries;
}

Cs2Paths Cs2Manager::detect(const QString &preferredRoot, QString *error)
{
    const QString steamRoot = registrySteamRoot();
    if (!preferredRoot.isEmpty()) {
        const QString root = normalizedRoot(preferredRoot);
        if (!root.isEmpty())
            return pathsForRoot(root, steamRoot);
    }

    QStringList libraries;
    if (!steamRoot.isEmpty())
        libraries.append(steamRoot);

    if (!steamRoot.isEmpty()) {
        QFile vdf(QDir(steamRoot).filePath(QStringLiteral("steamapps/libraryfolders.vdf")));
        if (vdf.open(QIODevice::ReadOnly))
            libraries.append(parseSteamLibraryFolders(QString::fromUtf8(vdf.readAll())));
    }

    QSet<QString> checked;
    for (const QString &library : std::as_const(libraries)) {
        const QString cleanLibrary = QDir::cleanPath(library);
        const QString key = cleanLibrary.toLower();
        if (checked.contains(key))
            continue;
        checked.insert(key);
        const QString candidate = QDir(cleanLibrary).filePath(QStringLiteral("steamapps/common/Counter-Strike Global Offensive"));
        const QString root = normalizedRoot(candidate);
        if (!root.isEmpty())
            return pathsForRoot(root, steamRoot);
    }

    if (error) {
        *error = steamRoot.isEmpty()
            ? QCoreApplication::translate("Cs2Manager", "Steam was not found in the registry. Select the CS2 installation folder manually.")
            : QCoreApplication::translate("Cs2Manager", "Steam was found, but CS2 was not. Select the Counter-Strike Global Offensive folder manually.");
    }
    return {};
}

Cs2Paths Cs2Manager::fromSelection(const QString &selectedPath, QString *error)
{
    QString input = selectedPath;
    if (QFileInfo(input).isFile())
        input = QFileInfo(input).absolutePath();
    const QString root = normalizedRoot(input);
    if (root.isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("Cs2Manager", "The selected folder is not a valid CS2 installation. Select the Counter-Strike Global Offensive, game, or game\\csgo folder.");
        return {};
    }
    return pathsForRoot(root, registrySteamRoot());
}

QString Cs2Manager::findBundledVpk()
{
    QDir cursor(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 6; ++depth) {
        const QStringList candidates = {
            cursor.filePath(QString::fromLatin1(kVpkName)),
            cursor.filePath(QStringLiteral("resources/") + QString::fromLatin1(kVpkName)),
            cursor.filePath(QStringLiteral("dist/") + QString::fromLatin1(kVpkName))
        };
        for (const QString &candidate : candidates) {
            if (QFileInfo::exists(candidate))
                return QDir::cleanPath(candidate);
        }
        if (!cursor.cdUp())
            break;
    }
    return {};
}

bool Cs2Manager::isCs2Running()
{
#ifdef Q_OS_WIN
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    bool running = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"cs2.exe") == 0) {
                running = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return running;
#else
    return false;
#endif
}

bool Cs2Manager::isOverrideInstalled(const Cs2Paths &paths)
{
    if (QFileInfo::exists(targetVpkPath(paths)))
        return true;
    QFile gameInfo(paths.gameInfo);
    if (!gameInfo.open(QIODevice::ReadOnly))
        return false;
    TextEncoding encoding = TextEncoding::Utf8;
    const QString text = decodeText(gameInfo.readAll(), &encoding);
    const QRegularExpression overrideLine(QStringLiteral(R"((?m)^\s*Game\s+csgo/overrides/swift_demo_menu_override\.vpk\s*$)"), QRegularExpression::CaseInsensitiveOption);
    return overrideLine.match(text).hasMatch();
}

bool Cs2Manager::isSessionActive(const Cs2Paths &paths)
{
    return QFileInfo::exists(markerPath(paths));
}

QString Cs2Manager::overrideSearchPath()
{
    return QStringLiteral("Game\tcsgo/overrides/") + QString::fromLatin1(kVpkName);
}

QString Cs2Manager::addOverrideSearchPath(const QString &gameInfoText, bool *changed, QString *error)
{
    *changed = false;
    const QString newline = gameInfoText.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    QStringList lines = gameInfoText.split(QRegularExpression(QStringLiteral("\r\n|\n")), Qt::KeepEmptyParts);
    const QRegularExpression overrideLine(QStringLiteral(R"(^\s*Game\s+csgo/overrides/swift_demo_menu_override\.vpk\s*$)"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression baseGameLine(QStringLiteral(R"(^(\s*)Game\s+csgo\s*(?://.*)?$)"), QRegularExpression::CaseInsensitiveOption);

    for (const QString &line : std::as_const(lines)) {
        if (overrideLine.match(line).hasMatch())
            return gameInfoText;
    }

    for (int index = 0; index < lines.size(); ++index) {
        const QRegularExpressionMatch match = baseGameLine.match(lines[index]);
        if (match.hasMatch()) {
            lines.insert(index, match.captured(1) + overrideSearchPath());
            *changed = true;
            return lines.join(newline);
        }
    }

    if (error)
        *error = QCoreApplication::translate("Cs2Manager", "The base 'Game csgo' SearchPath was not found in gameinfo.gi. Installation was stopped to avoid damaging the file.");
    return gameInfoText;
}

QString Cs2Manager::removeOverrideSearchPath(const QString &gameInfoText, bool *changed)
{
    *changed = false;
    const QString newline = gameInfoText.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    const QStringList lines = gameInfoText.split(QRegularExpression(QStringLiteral("\r\n|\n")), Qt::KeepEmptyParts);
    const QRegularExpression overrideLine(QStringLiteral(R"(^\s*Game\s+csgo/overrides/swift_demo_menu_override\.vpk\s*$)"), QRegularExpression::CaseInsensitiveOption);
    QStringList kept;
    kept.reserve(lines.size());
    for (const QString &line : lines) {
        if (overrideLine.match(line).hasMatch()) {
            *changed = true;
            continue;
        }
        kept.append(line);
    }
    return kept.join(newline);
}

LauncherResult Cs2Manager::installOverride(const Cs2Paths &paths, const QString &sourceVpk)
{
    if (!paths.isValid())
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "The CS2 installation folder is invalid."));
    if (!QFileInfo::exists(sourceVpk))
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "The DemoUI VPK was not found. Build the project first, or place %1 next to the launcher.").arg(QString::fromLatin1(kVpkName)));

    QString error;
    const QString backup = paths.gameInfo + QStringLiteral(".swift_demo_launcher.restore.bak");
    if (!QFileInfo::exists(backup) && !QFile::copy(paths.gameInfo, backup))
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to back up gameinfo.gi. Check the folder permissions."));

    QString text;
    TextEncoding encoding = TextEncoding::Utf8;
    if (!readTextFile(paths.gameInfo, &text, &encoding, &error))
        return LauncherResult::failure(error);

    bool changed = false;
    const QString updated = addOverrideSearchPath(text, &changed, &error);
    if (!error.isEmpty())
        return LauncherResult::failure(error);
    if (changed && !writeTextFile(paths.gameInfo, updated, encoding, &error))
        return LauncherResult::failure(error);

    if (!copyFileAtomically(sourceVpk, targetVpkPath(paths), &error)) {
        if (changed) {
            QString ignored;
            writeTextFile(paths.gameInfo, text, encoding, &ignored);
        }
        return LauncherResult::failure(error);
    }

    return LauncherResult::success(QCoreApplication::translate("Cs2Manager", "The DemoUI VPK has been installed and verified."));
}

QString Cs2Manager::buildDemoCfg()
{
    return QStringLiteral(
        "echo \"Swift DemoUI Pro session\"\n"
        "demo_ui_mode 2\n"
        "tv_listen_voice_indices -1\n"
        "tv_listen_voice_indices_h -1\n"
        "playdemo \"demos/swift_demo_launcher/current.dem\"\n");
}

LauncherResult Cs2Manager::prepareDemoSession(const Cs2Paths &paths, const QString &demoPath)
{
    const QFileInfo demoInfo(demoPath);
    if (!demoInfo.exists() || !demoInfo.isFile() || demoInfo.suffix().compare(QStringLiteral("dem"), Qt::CaseInsensitive) != 0)
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Select a valid .dem file."));

    QString error;
    if (!copyFileAtomically(demoInfo.absoluteFilePath(), stagedDemoPath(paths), &error))
        return LauncherResult::failure(error);

    if (!QDir().mkpath(QFileInfo(cfgPath(paths)).absolutePath()))
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to create the CS2 cfg folder."));
    QSaveFile cfg(cfgPath(paths));
    if (!cfg.open(QIODevice::WriteOnly) || cfg.write(buildDemoCfg().toUtf8()) < 0 || !cfg.commit())
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to create the Demo launch configuration: %1").arg(cfg.errorString()));

    if (!QDir().mkpath(QFileInfo(markerPath(paths)).absolutePath()))
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to create the Demo session marker."));
    QSaveFile marker(markerPath(paths));
    const QJsonObject state {
        { QStringLiteral("demo"), demoInfo.absoluteFilePath() },
        { QStringLiteral("createdUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) }
    };
    if (!marker.open(QIODevice::WriteOnly) || marker.write(QJsonDocument(state).toJson(QJsonDocument::Compact)) < 0 || !marker.commit())
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to save the Demo session state."));

    return LauncherResult::success(QCoreApplication::translate("Cs2Manager", "The Demo and launch configuration are ready."));
}

QStringList Cs2Manager::buildSteamLaunchArguments()
{
    return {
        QStringLiteral("-applaunch"),
        QStringLiteral("730"),
        QStringLiteral("-insecure"),
        QStringLiteral("-novid"),
        QStringLiteral("+exec"),
        QString::fromLatin1(kCfgName)
    };
}

LauncherResult Cs2Manager::launchDemo(const Cs2Paths &paths)
{
    bool launched = false;
    qint64 processId = 0;
    if (QFileInfo::exists(paths.steamExe)) {
        launched = QProcess::startDetached(paths.steamExe, buildSteamLaunchArguments(), paths.steamRoot, &processId);
    } else if (QFileInfo::exists(paths.cs2Exe)) {
        QStringList arguments = buildSteamLaunchArguments();
        arguments.removeFirst();
        arguments.removeFirst();
        launched = QProcess::startDetached(paths.cs2Exe, arguments, QFileInfo(paths.cs2Exe).absolutePath(), &processId);
    }

    if (!launched)
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to start Steam/CS2. Make sure Steam is installed and try running the launcher as administrator."));
    return LauncherResult::success(QCoreApplication::translate("Cs2Manager", "CS2 is starting in -insecure Demo mode."));
}

LauncherResult Cs2Manager::removeDemoSession(const Cs2Paths &paths)
{
    if (!paths.isValid())
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "The CS2 installation folder is invalid, so cleanup cannot be completed safely."));
    if (isCs2Running())
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "CS2 is still running. Fully exit the game before stopping Demo playback."));

    QStringList errors;
    QString text;
    QString error;
    TextEncoding encoding = TextEncoding::Utf8;
    if (readTextFile(paths.gameInfo, &text, &encoding, &error)) {
        bool changed = false;
        const QString updated = removeOverrideSearchPath(text, &changed);
        if (changed && !writeTextFile(paths.gameInfo, updated, encoding, &error))
            errors.append(error);
    } else {
        errors.append(error);
    }

    const QString vpk = targetVpkPath(paths);
    if (QFileInfo::exists(vpk) && !QFile::remove(vpk))
        errors.append(QCoreApplication::translate("Cs2Manager", "Unable to remove the DemoUI VPK: %1").arg(QDir::toNativeSeparators(vpk)));

    const QString config = cfgPath(paths);
    if (QFileInfo::exists(config) && !QFile::remove(config))
        errors.append(QCoreApplication::translate("Cs2Manager", "Unable to remove the temporary CFG."));

    const QString marker = markerPath(paths);
    if (QFileInfo::exists(marker) && !QFile::remove(marker))
        errors.append(QCoreApplication::translate("Cs2Manager", "Unable to remove the Demo session marker."));

    QDir demoDirectory(QFileInfo(stagedDemoPath(paths)).absolutePath());
    const QString expectedSuffix = QDir::cleanPath(QStringLiteral("demos/swift_demo_launcher"));
    if (QDir::cleanPath(demoDirectory.path()).endsWith(expectedSuffix, Qt::CaseInsensitive)
        && demoDirectory.exists() && !demoDirectory.removeRecursively()) {
        errors.append(QCoreApplication::translate("Cs2Manager", "Unable to remove the copied Demo file."));
    }

    if (!errors.isEmpty())
        return LauncherResult::failure(errors.join(QStringLiteral("\n")));
    return LauncherResult::success(QCoreApplication::translate("Cs2Manager", "Demo mode has stopped. The VPK and temporary files were removed, and CS2 can now be started normally from Steam."));
}

QString Cs2Manager::displayFileSize(qint64 bytes)
{
    const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mib >= 1024.0)
        return QString::number(mib / 1024.0, 'f', 2) + QStringLiteral(" GB");
    return QString::number(mib, 'f', 1) + QStringLiteral(" MB");
}
