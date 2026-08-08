#include "Cs2Manager.h"

#include "miniz.h"

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
#include <QStorageInfo>
#include <QVector>

#include <algorithm>
#include <limits>
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

QString voiceSessionDirectory(const Cs2Paths &paths)
{
    return QDir(paths.csgoDir).filePath(QStringLiteral("overrides/swift_demo_voice_session"));
}

QString targetVoiceSessionVpkPath(const Cs2Paths &paths)
{
    return QDir(paths.csgoDir).filePath(QStringLiteral("overrides/swift_demo_voice_session.vpk"));
}

QString voiceContentAddonDirectory(const Cs2Paths &paths)
{
    return QDir(paths.cs2Root).filePath(QStringLiteral("content/csgo_addons/swift_demoui_voice_session"));
}

QString voiceGameAddonDirectory(const Cs2Paths &paths)
{
    return QDir(paths.cs2Root).filePath(QStringLiteral("game/csgo_addons/swift_demoui_voice_session"));
}

bool removeOwnedDirectory(const QString &path, const QString &expectedSuffix, QString *error)
{
    const QString cleanPath = QDir::cleanPath(path);
    if (!cleanPath.endsWith(QDir::cleanPath(expectedSuffix), Qt::CaseInsensitive)) {
        if (error) {
            *error = QCoreApplication::translate("Cs2Manager", "Refusing to remove an unexpected voice-session directory: %1")
                         .arg(QDir::toNativeSeparators(cleanPath));
        }
        return false;
    }
    QDir directory(cleanPath);
    if (!directory.exists())
        return true;
    if (!directory.removeRecursively()) {
        if (error) {
            *error = QCoreApplication::translate("Cs2Manager", "Unable to remove the temporary voice-session directory: %1")
                         .arg(QDir::toNativeSeparators(cleanPath));
        }
        return false;
    }
    return true;
}

LauncherResult runSessionTool(const QString &program, const QStringList &arguments, const QString &workingDirectory, const QString &toolName)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(workingDirectory);
    process.start();
    if (!process.waitForStarted(10000)) {
        return LauncherResult::failure(
            QCoreApplication::translate("Cs2Manager", "Unable to start %1: %2").arg(toolName, process.errorString()));
    }
    if (!process.waitForFinished(300000)) {
        process.kill();
        process.waitForFinished(5000);
        return LauncherResult::failure(
            QCoreApplication::translate("Cs2Manager", "%1 timed out while preparing parsed voice status data.").arg(toolName));
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QString output = QString::fromLocal8Bit(process.readAll()).trimmed();
        if (output.size() > 3000)
            output = output.right(3000);
        return LauncherResult::failure(
            QCoreApplication::translate("Cs2Manager", "%1 failed with exit code %2.\n%3")
                .arg(toolName)
                .arg(process.exitCode())
                .arg(output));
    }
    return LauncherResult::success();
}

QString cfgPath(const Cs2Paths &paths)
{
    return QDir(paths.csgoDir).filePath(QStringLiteral("cfg/") + QString::fromLatin1(Cs2Manager::kCfgName));
}

QString stagedDemoPath(const Cs2Paths &paths)
{
    return QDir(paths.csgoDir).filePath(QStringLiteral("demos/swift_demo_launcher/current.dem"));
}

constexpr qint64 kMaximumArchivedDemoSize = 8LL * 1024 * 1024 * 1024;
constexpr qint64 kExtractionDiskReserve = 64LL * 1024 * 1024;

class MinizArchiveReader
{
public:
    ~MinizArchiveReader()
    {
        if (initialized_)
            mz_zip_reader_end(&archive_);
    }

    bool open(const QString &path, QString *error)
    {
        file_.setFileName(path);
        if (!file_.open(QIODevice::ReadOnly)) {
            *error = QCoreApplication::translate("Cs2Manager", "Unable to open the ZIP archive: %1").arg(file_.errorString());
            return false;
        }

        mz_zip_zero_struct(&archive_);
        archive_.m_pRead = &MinizArchiveReader::readAt;
        archive_.m_pIO_opaque = &file_;
        if (!mz_zip_reader_init(&archive_, static_cast<mz_uint64>(file_.size()), 0)) {
            *error = QCoreApplication::translate("Cs2Manager", "Unable to read the ZIP archive: %1").arg(lastError());
            return false;
        }
        initialized_ = true;
        return true;
    }

    mz_zip_archive *archive() { return &archive_; }

    QString lastError()
    {
        return QString::fromLatin1(mz_zip_get_error_string(mz_zip_peek_last_error(&archive_)));
    }

private:
    static size_t readAt(void *opaque, mz_uint64 offset, void *buffer, size_t byteCount)
    {
        auto *file = static_cast<QFile *>(opaque);
        if (offset > static_cast<mz_uint64>(std::numeric_limits<qint64>::max())
            || byteCount > static_cast<size_t>(std::numeric_limits<qint64>::max())
            || !file->seek(static_cast<qint64>(offset))) {
            return 0;
        }
        const qint64 bytesRead = file->read(static_cast<char *>(buffer), static_cast<qint64>(byteCount));
        return bytesRead > 0 ? static_cast<size_t>(bytesRead) : 0;
    }

    QFile file_;
    mz_zip_archive archive_ {};
    bool initialized_ = false;
};

struct ArchiveDemoRecord
{
    DemoArchiveEntry entry;
    mz_uint index = 0;
};

QString archiveEntryName(mz_zip_archive *archive, mz_uint index, QString *error)
{
    const mz_uint required = mz_zip_reader_get_filename(archive, index, nullptr, 0);
    if (required == 0 || required > 65536) {
        *error = QCoreApplication::translate("Cs2Manager", "The ZIP archive contains an invalid file name.");
        return {};
    }

    QByteArray encoded(static_cast<qsizetype>(required), Qt::Uninitialized);
    if (mz_zip_reader_get_filename(archive, index, encoded.data(), required) != required) {
        *error = QCoreApplication::translate("Cs2Manager", "Unable to read a file name from the ZIP archive.");
        return {};
    }
    if (!encoded.isEmpty() && encoded.back() == '\0')
        encoded.chop(1);
    if (encoded.isEmpty() || encoded.contains('\0')) {
        *error = QCoreApplication::translate("Cs2Manager", "The ZIP archive contains an invalid file name.");
        return {};
    }

    QString decoded = QString::fromUtf8(encoded);
    if (decoded.contains(QChar::ReplacementCharacter))
        decoded = QString::fromLocal8Bit(encoded);
    return QDir::fromNativeSeparators(decoded);
}

LauncherResult listArchiveDemos(MinizArchiveReader *reader, QList<ArchiveDemoRecord> *records)
{
    records->clear();
    QSet<QString> names;
    int unsupportedDemoCount = 0;
    mz_zip_archive *archive = reader->archive();
    const mz_uint fileCount = mz_zip_reader_get_num_files(archive);
    if (fileCount > 100000) {
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "The ZIP archive contains too many files."));
    }

    for (mz_uint index = 0; index < fileCount; ++index) {
        mz_zip_archive_file_stat stat {};
        if (!mz_zip_reader_file_stat(archive, index, &stat)) {
            return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to inspect the ZIP archive: %1").arg(reader->lastError()));
        }
        if (stat.m_is_directory)
            continue;

        QString nameError;
        const QString name = archiveEntryName(archive, index, &nameError);
        if (name.isEmpty())
            return LauncherResult::failure(nameError);
        if (QFileInfo(name).suffix().compare(QStringLiteral("dem"), Qt::CaseInsensitive) != 0)
            continue;
        if (!stat.m_is_supported || stat.m_is_encrypted) {
            ++unsupportedDemoCount;
            continue;
        }
        if (names.contains(name)) {
            return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "The ZIP archive contains duplicate Demo paths: %1").arg(name));
        }
        if (stat.m_uncomp_size > static_cast<mz_uint64>(std::numeric_limits<qint64>::max())) {
            return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "A Demo in the ZIP archive is too large to use."));
        }

        names.insert(name);
        records->append({ { name, static_cast<qint64>(stat.m_uncomp_size) }, index });
    }

    if (records->isEmpty()) {
        if (unsupportedDemoCount > 0) {
            return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "The ZIP contains .dem files, but they are encrypted or use an unsupported compression method."));
        }
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "The ZIP archive does not contain a CS2 .dem file."));
    }

    std::sort(records->begin(), records->end(), [](const ArchiveDemoRecord &left, const ArchiveDemoRecord &right) {
        return left.entry.path.compare(right.entry.path, Qt::CaseInsensitive) < 0;
    });
    return LauncherResult::success();
}

LauncherResult stageArchiveDemo(const QString &archivePath, const QString &entryPath, const QString &targetPath)
{
    MinizArchiveReader reader;
    QString openError;
    if (!reader.open(archivePath, &openError))
        return LauncherResult::failure(openError);

    QList<ArchiveDemoRecord> records;
    const LauncherResult inspected = listArchiveDemos(&reader, &records);
    if (!inspected.ok)
        return inspected;

    const auto selected = std::find_if(records.cbegin(), records.cend(), [&entryPath](const ArchiveDemoRecord &record) {
        return record.entry.path == entryPath;
    });
    if (selected == records.cend()) {
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "The selected Demo is no longer present in the ZIP archive."));
    }
    if (selected->entry.size <= 0) {
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "The selected Demo in the ZIP archive is empty."));
    }
    if (selected->entry.size > kMaximumArchivedDemoSize) {
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "The selected Demo exceeds the 8 GB extraction safety limit."));
    }

    const QFileInfo targetInfo(targetPath);
    if (!QDir().mkpath(targetInfo.absolutePath())) {
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to create directory: %1").arg(QDir::toNativeSeparators(targetInfo.absolutePath())));
    }
    const QStorageInfo storage(targetInfo.absolutePath());
    if (storage.isValid() && storage.isReady()
        && (storage.bytesAvailable() < kExtractionDiskReserve
            || selected->entry.size > storage.bytesAvailable() - kExtractionDiskReserve)) {
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Not enough free disk space to extract the selected Demo."));
    }

    QSaveFile output(targetPath);
    if (!output.open(QIODevice::WriteOnly)) {
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to create the staged Demo file: %1").arg(output.errorString()));
    }

    mz_zip_reader_extract_iter_state *iterator = mz_zip_reader_extract_iter_new(reader.archive(), selected->index, 0);
    if (!iterator) {
        output.cancelWriting();
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to extract the selected Demo: %1").arg(reader.lastError()));
    }

    QByteArray buffer(1024 * 1024, Qt::Uninitialized);
    qint64 totalWritten = 0;
    bool writeFailed = false;
    while (true) {
        const size_t extracted = mz_zip_reader_extract_iter_read(iterator, buffer.data(), static_cast<size_t>(buffer.size()));
        if (extracted == 0)
            break;
        if (extracted > static_cast<size_t>(std::numeric_limits<qint64>::max() - totalWritten)
            || totalWritten + static_cast<qint64>(extracted) > selected->entry.size
            || output.write(buffer.constData(), static_cast<qint64>(extracted)) != static_cast<qint64>(extracted)) {
            writeFailed = true;
            break;
        }
        totalWritten += static_cast<qint64>(extracted);
    }
    const bool extractionFinished = mz_zip_reader_extract_iter_free(iterator) == MZ_TRUE;

    if (writeFailed) {
        output.cancelWriting();
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to write the staged Demo file: %1").arg(output.errorString()));
    }
    if (!extractionFinished || totalWritten != selected->entry.size) {
        output.cancelWriting();
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to extract the selected Demo: %1").arg(reader.lastError()));
    }
    if (!output.commit()) {
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to finish staging the Demo: %1").arg(output.errorString()));
    }
    return LauncherResult::success();
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

QString Cs2Manager::findBundledVoiceIndexer()
{
    QDir cursor(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 6; ++depth) {
        const QStringList candidates = {
            cursor.filePath(QString::fromLatin1(kVoiceIndexerName)),
            cursor.filePath(QStringLiteral("resources/") + QString::fromLatin1(kVoiceIndexerName)),
            cursor.filePath(QStringLiteral("tools/voice-indexer/target/release/") + QString::fromLatin1(kVoiceIndexerName))
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

QString Cs2Manager::voiceSessionSearchPath()
{
    return QStringLiteral("Game\tcsgo/overrides/swift_demo_voice_session.vpk");
}

QString Cs2Manager::addOverrideSearchPath(const QString &gameInfoText, bool *changed, QString *error)
{
    *changed = false;
    const QString newline = gameInfoText.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    QStringList lines = gameInfoText.split(QRegularExpression(QStringLiteral("\r\n|\n")), Qt::KeepEmptyParts);
    const QRegularExpression overrideLine(QStringLiteral(R"(^\s*Game\s+csgo/overrides/swift_demo_menu_override\.vpk\s*$)"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression voiceLine(QStringLiteral(R"(^\s*Game\s+csgo/overrides/swift_demo_voice_session\.vpk\s*$)"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression legacyVoiceLine(QStringLiteral(R"(^\s*Game\s+csgo/overrides/swift_demo_voice_session\s*$)"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression baseGameLine(QStringLiteral(R"(^(\s*)Game\s+csgo\s*(?://.*)?$)"), QRegularExpression::CaseInsensitiveOption);

    int overrideIndex = -1;
    bool hasVoicePath = false;
    bool hasLegacyVoicePath = false;
    for (int index = 0; index < lines.size(); ++index) {
        if (overrideLine.match(lines[index]).hasMatch())
            overrideIndex = index;
        if (voiceLine.match(lines[index]).hasMatch())
            hasVoicePath = true;
        if (legacyVoiceLine.match(lines[index]).hasMatch())
            hasLegacyVoicePath = true;
    }
    if (overrideIndex >= 0 && hasVoicePath && !hasLegacyVoicePath)
        return gameInfoText;

    if (hasLegacyVoicePath) {
        for (int index = lines.size() - 1; index >= 0; --index) {
            if (legacyVoiceLine.match(lines[index]).hasMatch())
                lines.removeAt(index);
        }
        overrideIndex = -1;
        for (int index = 0; index < lines.size(); ++index) {
            if (overrideLine.match(lines[index]).hasMatch()) {
                overrideIndex = index;
                break;
            }
        }
    }

    QString indentation;
    if (overrideIndex < 0) {
        for (int index = 0; index < lines.size(); ++index) {
            const QRegularExpressionMatch match = baseGameLine.match(lines[index]);
            if (match.hasMatch()) {
                indentation = match.captured(1);
                lines.insert(index, indentation + overrideSearchPath());
                overrideIndex = index;
                break;
            }
        }
    }

    if (overrideIndex >= 0 && !hasVoicePath) {
        if (indentation.isEmpty()) {
            const QRegularExpressionMatch match = QRegularExpression(QStringLiteral(R"(^(\s*))")).match(lines[overrideIndex]);
            indentation = match.captured(1);
        }
        lines.insert(overrideIndex, indentation + voiceSessionSearchPath());
    }

    if (overrideIndex >= 0) {
        *changed = true;
        return lines.join(newline);
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
    const QRegularExpression overrideLine(QStringLiteral(R"(^\s*Game\s+csgo/overrides/(?:swift_demo_menu_override\.vpk|swift_demo_voice_session(?:\.vpk)?)\s*$)"), QRegularExpression::CaseInsensitiveOption);
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

QString Cs2Manager::buildDemoCfg(bool trueViewEnabled)
{
    return QStringLiteral(
        "echo \"Swift DemoUI Pro session\"\n"
        "demo_ui_mode 2\n"
        "cl_demo_predict %1\n"
        "tv_listen_voice_indices -1\n"
        "tv_listen_voice_indices_h -1\n"
        "playdemo \"demos/swift_demo_launcher/current.dem\"\n")
        .arg(trueViewEnabled ? 1 : 0);
}

LauncherResult Cs2Manager::inspectDemoArchive(const QString &archivePath, QList<DemoArchiveEntry> *entries)
{
    if (!entries)
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to return the Demo list for this ZIP archive."));
    entries->clear();

    const QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists() || !archiveInfo.isFile()
        || archiveInfo.suffix().compare(QStringLiteral("zip"), Qt::CaseInsensitive) != 0) {
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Select a valid .zip file."));
    }

    MinizArchiveReader reader;
    QString openError;
    if (!reader.open(archiveInfo.absoluteFilePath(), &openError))
        return LauncherResult::failure(openError);

    QList<ArchiveDemoRecord> records;
    const LauncherResult result = listArchiveDemos(&reader, &records);
    if (!result.ok)
        return result;
    entries->reserve(records.size());
    for (const ArchiveDemoRecord &record : std::as_const(records))
        entries->append(record.entry);
    return LauncherResult::success(QCoreApplication::translate("Cs2Manager", "The ZIP archive is ready."));
}

LauncherResult Cs2Manager::prepareDemoSession(const Cs2Paths &paths, const QString &demoPath, const QString &archiveEntry, bool trueViewEnabled)
{
    const QFileInfo demoInfo(demoPath);
    if (!demoInfo.exists() || !demoInfo.isFile())
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Select a valid .dem or .zip file."));

    QString error;
    const bool isDemo = demoInfo.suffix().compare(QStringLiteral("dem"), Qt::CaseInsensitive) == 0;
    const bool isZip = demoInfo.suffix().compare(QStringLiteral("zip"), Qt::CaseInsensitive) == 0;
    if (isDemo) {
        if (!copyFileAtomically(demoInfo.absoluteFilePath(), stagedDemoPath(paths), &error))
            return LauncherResult::failure(error);
    } else if (isZip) {
        if (archiveEntry.isEmpty())
            return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Choose a Demo from the ZIP archive first."));
        const LauncherResult staged = stageArchiveDemo(demoInfo.absoluteFilePath(), archiveEntry, stagedDemoPath(paths));
        if (!staged.ok)
            return staged;
    } else {
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Select a valid .dem or .zip file."));
    }

    if (!QDir().mkpath(QFileInfo(cfgPath(paths)).absolutePath()))
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to create the CS2 cfg folder."));
    QSaveFile cfg(cfgPath(paths));
    if (!cfg.open(QIODevice::WriteOnly) || cfg.write(buildDemoCfg(trueViewEnabled).toUtf8()) < 0 || !cfg.commit())
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to create the Demo launch configuration: %1").arg(cfg.errorString()));

    if (!QDir().mkpath(QFileInfo(markerPath(paths)).absolutePath()))
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to create the Demo session marker."));
    QSaveFile marker(markerPath(paths));
    QJsonObject state {
        { QStringLiteral("demo"), demoInfo.absoluteFilePath() },
        { QStringLiteral("createdUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
        { QStringLiteral("trueViewEnabled"), trueViewEnabled }
    };
    if (isZip)
        state.insert(QStringLiteral("archiveEntry"), archiveEntry);
    if (!marker.open(QIODevice::WriteOnly) || marker.write(QJsonDocument(state).toJson(QJsonDocument::Compact)) < 0 || !marker.commit())
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "Unable to save the Demo session state."));

    return LauncherResult::success(QCoreApplication::translate("Cs2Manager", "The Demo and launch configuration are ready."));
}

LauncherResult Cs2Manager::prepareVoiceStatusData(const Cs2Paths &paths)
{
    if (!paths.isValid())
        return LauncherResult::failure(QCoreApplication::translate("Cs2Manager", "The CS2 installation folder is invalid."));

    const QString indexer = findBundledVoiceIndexer();
    if (indexer.isEmpty()) {
        return LauncherResult::failure(
            QCoreApplication::translate("Cs2Manager", "The Demo voice indexer was not found. Rebuild or reinstall Swift DemoUI Pro."));
    }
    const QString stagedDemo = stagedDemoPath(paths);
    if (!QFileInfo::exists(stagedDemo)) {
        return LauncherResult::failure(
            QCoreApplication::translate("Cs2Manager", "The staged Demo is missing, so parsed voice status data cannot be prepared."));
    }

    QString error;
    if (!removeOwnedDirectory(voiceSessionDirectory(paths), QStringLiteral("overrides/swift_demo_voice_session"), &error)
        || !removeOwnedDirectory(voiceContentAddonDirectory(paths), QStringLiteral("content/csgo_addons/swift_demoui_voice_session"), &error)
        || !removeOwnedDirectory(voiceGameAddonDirectory(paths), QStringLiteral("game/csgo_addons/swift_demoui_voice_session"), &error)) {
        return LauncherResult::failure(error);
    }
    LauncherResult result = runSessionTool(
        indexer,
        buildVoiceSessionArguments(stagedDemo, targetVoiceSessionVpkPath(paths)),
        QFileInfo(indexer).absolutePath(),
        QCoreApplication::translate("Cs2Manager", "Demo voice indexer"));
    if (result.ok && !QFileInfo::exists(targetVoiceSessionVpkPath(paths))) {
        result = LauncherResult::failure(
            QCoreApplication::translate("Cs2Manager", "The Demo voice packer did not create the session VPK."));
    }
    if (!result.ok)
        return result;

    return LauncherResult::success(QCoreApplication::translate("Cs2Manager", "Parsed Demo voice status data is ready."));
}

QStringList Cs2Manager::buildVoiceSessionArguments(const QString &demoPath, const QString &sessionVpkPath)
{
    return {
        QStringLiteral("build-session-vpk"),
        demoPath,
        sessionVpkPath
    };
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

    QString voiceCleanupError;
    const QString voiceVpk = targetVoiceSessionVpkPath(paths);
    if (QFileInfo::exists(voiceVpk) && !QFile::remove(voiceVpk))
        errors.append(QCoreApplication::translate("Cs2Manager", "Unable to remove the Demo voice session VPK."));
    if (!removeOwnedDirectory(voiceSessionDirectory(paths), QStringLiteral("overrides/swift_demo_voice_session"), &voiceCleanupError))
        errors.append(voiceCleanupError);
    if (!removeOwnedDirectory(voiceContentAddonDirectory(paths), QStringLiteral("content/csgo_addons/swift_demoui_voice_session"), &voiceCleanupError))
        errors.append(voiceCleanupError);
    if (!removeOwnedDirectory(voiceGameAddonDirectory(paths), QStringLiteral("game/csgo_addons/swift_demoui_voice_session"), &voiceCleanupError))
        errors.append(voiceCleanupError);

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
