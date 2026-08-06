#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

struct UpdateComponent
{
    QString version;
    QString url;
    QString sha256;

    bool isValid() const;
    bool isNewerThan(const QString &currentVersion) const;
};

struct UpdateInfo
{
    bool valid = false;
    QString error;
    QString releasePageUrl;
    UpdateComponent launcher;
    UpdateComponent menu;
};

class UpdateService final : public QObject
{
    Q_OBJECT

public:
    explicit UpdateService(QObject *parent = nullptr);

    void checkForUpdates();
    void downloadMenuUpdate(const UpdateComponent &component);

    static UpdateInfo parseLatestRelease(const QByteArray &releaseJson, const QByteArray &manifestJson = {});
    static QString bundledMenuVersion();
    static QString currentMenuVersion();
    static QString preferredMenuVpk();

signals:
    void checkFinished(const UpdateInfo &info);
    void menuDownloadProgress(qint64 received, qint64 total);
    void menuDownloadFinished(bool ok, const QString &message);

private:
    void requestLatestRelease(int attempt);
    void finishReleaseCheck(const QByteArray &releaseJson, const QByteArray &manifestJson = {});
    void failReleaseCheck(const QString &message);
    static QString normalizedSha256(const QString &value);
    static QString cacheRoot();
    static bool isCachedMenuValid(const QString &path, const QString &expectedHash);

    QNetworkAccessManager *network_ = nullptr;
    QNetworkReply *releaseReply_ = nullptr;
    QNetworkReply *manifestReply_ = nullptr;
    QNetworkReply *menuReply_ = nullptr;
    QByteArray pendingReleaseJson_;
    QByteArray menuDownloadBuffer_;
    UpdateComponent pendingMenu_;
};
