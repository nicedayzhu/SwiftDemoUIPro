#include "UpdateService.h"

#include "Cs2Manager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>

namespace
{
constexpr qint64 kMaximumMenuDownloadSize = 128LL * 1024LL * 1024LL;
const auto kLatestReleaseUrl = "https://api.github.com/repos/nicedayzhu/SwiftDemoUIPro/releases/latest";
const auto kManifestName = "update-manifest.json";

QNetworkRequest githubRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("Accept-Encoding", "identity");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("X-GitHub-Api-Version", "2026-03-10");
    request.setRawHeader(
        "User-Agent",
        QStringLiteral("SwiftDemoUIPro/%1").arg(QCoreApplication::applicationVersion()).toUtf8());
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

bool isSemanticVersion(const QString &version)
{
    const QVersionNumber parsed = QVersionNumber::fromString(version);
    return !parsed.isNull() && parsed.segmentCount() == 3 && parsed.toString() == version;
}

UpdateComponent componentFromJson(const QJsonObject &object)
{
    UpdateComponent component;
    component.version = object.value(QStringLiteral("version")).toString().trimmed();
    component.url = object.value(QStringLiteral("url")).toString().trimmed();
    component.sha256 = object.value(QStringLiteral("sha256")).toString().trimmed();
    if (!component.sha256.isEmpty() && component.sha256.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive))
        component.sha256.remove(0, 7);
    component.sha256 = component.sha256.toLower();
    return component;
}

UpdateComponent componentFromAsset(
    const QJsonObject &asset,
    const QRegularExpression &pattern)
{
    const QString name = asset.value(QStringLiteral("name")).toString();
    const QRegularExpressionMatch match = pattern.match(name);
    if (!match.hasMatch())
        return {};

    UpdateComponent component;
    component.version = match.captured(1);
    component.url = asset.value(QStringLiteral("browser_download_url")).toString();
    component.sha256 = asset.value(QStringLiteral("digest")).toString();
    if (component.sha256.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive))
        component.sha256.remove(0, 7);
    component.sha256 = component.sha256.toLower();
    return component;
}
}

bool UpdateComponent::isValid() const
{
    return isSemanticVersion(version) && QUrl(url).isValid() && QUrl(url).scheme() == QStringLiteral("https");
}

bool UpdateComponent::isNewerThan(const QString &currentVersion) const
{
    if (!isValid() || !isSemanticVersion(currentVersion))
        return false;
    return QVersionNumber::compare(
               QVersionNumber::fromString(version),
               QVersionNumber::fromString(currentVersion)) > 0;
}

UpdateService::UpdateService(QObject *parent)
    : QObject(parent)
    , network_(new QNetworkAccessManager(this))
{
}

void UpdateService::checkForUpdates()
{
    if (releaseReply_ || manifestReply_)
        return;

    requestLatestRelease(0);
}

void UpdateService::requestLatestRelease(int attempt)
{
    releaseReply_ = network_->get(githubRequest(QUrl(QString::fromLatin1(kLatestReleaseUrl))));
    connect(releaseReply_, &QNetworkReply::finished, this, [this, attempt]() {
        QNetworkReply *reply = releaseReply_;
        releaseReply_ = nullptr;
        const QByteArray responseBody = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            QString message = reply->errorString();
            const QJsonDocument errorDocument = QJsonDocument::fromJson(responseBody);
            const QString githubMessage = errorDocument.object().value(QStringLiteral("message")).toString().trimmed();
            if (!githubMessage.isEmpty())
                message = githubMessage;
            if (reply->rawHeader("X-RateLimit-Remaining") == QByteArrayLiteral("0"))
                message = tr("GitHub API rate limit reached. Please try again later.");
            reply->deleteLater();
            failReleaseCheck(message);
            return;
        }

        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString contentType = QString::fromLatin1(reply->header(QNetworkRequest::ContentTypeHeader).toByteArray());
        reply->deleteLater();
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseBody, &parseError);
        if (httpStatus != 200 || parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (attempt == 0) {
                QTimer::singleShot(500, this, [this]() { requestLatestRelease(1); });
                return;
            }
            failReleaseCheck(
                tr("GitHub returned an invalid JSON response (HTTP %1, %2, %3 bytes).")
                    .arg(httpStatus)
                    .arg(contentType.isEmpty() ? tr("unknown content type") : contentType)
                    .arg(responseBody.size()));
            return;
        }

        pendingReleaseJson_ = responseBody;
        const QJsonArray assets = document.object().value(QStringLiteral("assets")).toArray();
        QUrl manifestUrl;
        for (const QJsonValue &value : assets) {
            const QJsonObject asset = value.toObject();
            if (asset.value(QStringLiteral("name")).toString() == QString::fromLatin1(kManifestName)) {
                manifestUrl = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
                break;
            }
        }

        if (!manifestUrl.isValid()) {
            finishReleaseCheck(pendingReleaseJson_);
            return;
        }

        manifestReply_ = network_->get(githubRequest(manifestUrl));
        connect(manifestReply_, &QNetworkReply::finished, this, [this]() {
            QNetworkReply *manifestReply = manifestReply_;
            manifestReply_ = nullptr;
            if (manifestReply->error() != QNetworkReply::NoError) {
                const QString message = manifestReply->errorString();
                manifestReply->deleteLater();
                failReleaseCheck(message);
                return;
            }
            const QByteArray manifestJson = manifestReply->readAll();
            manifestReply->deleteLater();
            finishReleaseCheck(pendingReleaseJson_, manifestJson);
        });
    });
}

void UpdateService::downloadMenuUpdate(const UpdateComponent &component)
{
    if (menuReply_)
        return;
    if (!component.isValid()) {
        emit menuDownloadFinished(false, tr("The DemoUI update information is invalid."));
        return;
    }
    const QString expectedHash = normalizedSha256(component.sha256);
    if (expectedHash.isEmpty()) {
        emit menuDownloadFinished(false, tr("The DemoUI update has no valid SHA-256 digest and cannot be installed safely."));
        return;
    }

    pendingMenu_ = component;
    menuDownloadBuffer_.clear();
    menuReply_ = network_->get(githubRequest(QUrl(component.url)));
    connect(menuReply_, &QNetworkReply::downloadProgress, this, &UpdateService::menuDownloadProgress);
    connect(menuReply_, &QIODevice::readyRead, this, [this]() {
        if (!menuReply_)
            return;
        menuDownloadBuffer_.append(menuReply_->readAll());
        if (menuDownloadBuffer_.size() > kMaximumMenuDownloadSize)
            menuReply_->abort();
    });
    connect(menuReply_, &QNetworkReply::finished, this, [this, expectedHash]() {
        QNetworkReply *reply = menuReply_;
        menuReply_ = nullptr;
        menuDownloadBuffer_.append(reply->readAll());
        const bool tooLarge = menuDownloadBuffer_.size() > kMaximumMenuDownloadSize;
        if (reply->error() != QNetworkReply::NoError || tooLarge) {
            const QString message = tooLarge ? tr("The DemoUI update is unexpectedly large.") : reply->errorString();
            reply->deleteLater();
            menuDownloadBuffer_.clear();
            emit menuDownloadFinished(false, message);
            return;
        }
        reply->deleteLater();

        const QString actualHash = QString::fromLatin1(
            QCryptographicHash::hash(menuDownloadBuffer_, QCryptographicHash::Sha256).toHex());
        if (actualHash != expectedHash) {
            menuDownloadBuffer_.clear();
            emit menuDownloadFinished(false, tr("The downloaded DemoUI update failed SHA-256 verification."));
            return;
        }

        const QString directory = QDir(cacheRoot()).filePath(QStringLiteral("menu-%1").arg(pendingMenu_.version));
        if (!QDir().mkpath(directory)) {
            menuDownloadBuffer_.clear();
            emit menuDownloadFinished(false, tr("Unable to create the local update directory."));
            return;
        }
        const QString destination = QDir(directory).filePath(QString::fromLatin1(Cs2Manager::kVpkName));
        QSaveFile output(destination);
        if (!output.open(QIODevice::WriteOnly)
            || output.write(menuDownloadBuffer_) != menuDownloadBuffer_.size()
            || !output.commit()) {
            menuDownloadBuffer_.clear();
            emit menuDownloadFinished(false, tr("Unable to save the verified DemoUI update."));
            return;
        }
        menuDownloadBuffer_.clear();

        QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
        settings.setValue(QStringLiteral("cachedMenuVersion"), pendingMenu_.version);
        settings.setValue(QStringLiteral("cachedMenuPath"), destination);
        settings.setValue(QStringLiteral("cachedMenuSha256"), actualHash);
        emit menuDownloadFinished(
            true,
            tr("DemoUI %1 is ready and will be used the next time playback starts.").arg(pendingMenu_.version));
    });
}

UpdateInfo UpdateService::parseLatestRelease(const QByteArray &releaseJson, const QByteArray &manifestJson)
{
    UpdateInfo info;
    QJsonParseError releaseError;
    const QJsonDocument releaseDocument = QJsonDocument::fromJson(releaseJson, &releaseError);
    if (releaseError.error != QJsonParseError::NoError || !releaseDocument.isObject()) {
        info.error = tr("GitHub returned invalid release data.");
        return info;
    }

    const QJsonObject release = releaseDocument.object();
    info.releasePageUrl = release.value(QStringLiteral("html_url")).toString();
    if (!manifestJson.isEmpty()) {
        QJsonParseError manifestError;
        const QJsonDocument manifestDocument = QJsonDocument::fromJson(manifestJson, &manifestError);
        if (manifestError.error != QJsonParseError::NoError || !manifestDocument.isObject()) {
            info.error = tr("The release update manifest is invalid.");
            return info;
        }
        const QJsonObject manifest = manifestDocument.object();
        if (manifest.value(QStringLiteral("schemaVersion")).toInt() != 1) {
            info.error = tr("The release update manifest uses an unsupported format.");
            return info;
        }
        info.launcher = componentFromJson(manifest.value(QStringLiteral("launcher")).toObject());
        info.menu = componentFromJson(manifest.value(QStringLiteral("menu")).toObject());
    } else {
        const QRegularExpression launcherPattern(
            QStringLiteral(R"(^SwiftDemoUIPro-v([0-9]+\.[0-9]+\.[0-9]+)-win64\.zip$)"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpression menuPattern(
            QStringLiteral(R"(^swift_demo_menu_override-v([0-9]+\.[0-9]+\.[0-9]+)\.vpk$)"),
            QRegularExpression::CaseInsensitiveOption);
        for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = value.toObject();
            if (!info.launcher.isValid())
                info.launcher = componentFromAsset(asset, launcherPattern);
            if (!info.menu.isValid())
                info.menu = componentFromAsset(asset, menuPattern);
        }
    }

    if (!info.launcher.isValid() && !info.menu.isValid()) {
        info.error = tr("The latest release does not contain a supported update asset.");
        return info;
    }
    info.valid = true;
    return info;
}

QString UpdateService::bundledMenuVersion()
{
    if (!QCoreApplication::instance())
        return {};
    return QCoreApplication::instance()->property("menuVersion").toString();
}

QString UpdateService::currentMenuVersion()
{
    const QString bundled = bundledMenuVersion();
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    const QString cachedVersion = settings.value(QStringLiteral("cachedMenuVersion")).toString();
    const QString cachedPath = settings.value(QStringLiteral("cachedMenuPath")).toString();
    const QString cachedHash = settings.value(QStringLiteral("cachedMenuSha256")).toString();
    if (isCachedMenuValid(cachedPath, cachedHash)
        && isSemanticVersion(cachedVersion)
        && (!isSemanticVersion(bundled)
            || QVersionNumber::compare(
                   QVersionNumber::fromString(cachedVersion),
                   QVersionNumber::fromString(bundled)) > 0)) {
        return cachedVersion;
    }
    return bundled;
}

QString UpdateService::preferredMenuVpk()
{
    QSettings settings(QStringLiteral("SwiftTools"), QStringLiteral("SwiftDemoLauncher"));
    const QString cachedVersion = settings.value(QStringLiteral("cachedMenuVersion")).toString();
    const QString cachedPath = settings.value(QStringLiteral("cachedMenuPath")).toString();
    const QString cachedHash = settings.value(QStringLiteral("cachedMenuSha256")).toString();
    const QString bundled = bundledMenuVersion();
    if (isCachedMenuValid(cachedPath, cachedHash)
        && isSemanticVersion(cachedVersion)
        && (!isSemanticVersion(bundled)
            || QVersionNumber::compare(
                   QVersionNumber::fromString(cachedVersion),
                   QVersionNumber::fromString(bundled)) > 0)) {
        return QDir::cleanPath(cachedPath);
    }
    return Cs2Manager::findBundledVpk();
}

void UpdateService::finishReleaseCheck(const QByteArray &releaseJson, const QByteArray &manifestJson)
{
    const UpdateInfo info = parseLatestRelease(releaseJson, manifestJson);
    pendingReleaseJson_.clear();
    emit checkFinished(info);
}

void UpdateService::failReleaseCheck(const QString &message)
{
    pendingReleaseJson_.clear();
    UpdateInfo info;
    info.error = message;
    emit checkFinished(info);
}

QString UpdateService::normalizedSha256(const QString &value)
{
    QString hash = value.trimmed().toLower();
    if (hash.startsWith(QStringLiteral("sha256:")))
        hash.remove(0, 7);
    static const QRegularExpression pattern(QStringLiteral(R"(^[0-9a-f]{64}$)"));
    return pattern.match(hash).hasMatch() ? hash : QString();
}

QString UpdateService::cacheRoot()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("updates"));
}

bool UpdateService::isCachedMenuValid(const QString &path, const QString &expectedHash)
{
    const QString hash = normalizedSha256(expectedHash);
    const QFileInfo info(path);
    if (hash.isEmpty() || !info.exists() || !info.isFile()
        || info.size() <= 0 || info.size() > kMaximumMenuDownloadSize) {
        return false;
    }

    const QDir root(cacheRoot());
    const QString relative = QDir::cleanPath(root.relativeFilePath(info.absoluteFilePath()));
    if (relative == QStringLiteral("..")
        || relative.startsWith(QStringLiteral("../"))
        || QDir::isAbsolutePath(relative)) {
        return false;
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QCryptographicHash digest(QCryptographicHash::Sha256);
    if (!digest.addData(&file))
        return false;
    return QString::fromLatin1(digest.result().toHex()) == hash;
}
