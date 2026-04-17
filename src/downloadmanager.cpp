#include "downloadmanager.h"

#include <QFile>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>

DownloadManager::DownloadManager(QObject* parent)
    : QObject(parent)
{}

void DownloadManager::downloadRelease(const QString& repoUrl, const QString& assetName,
                                       const QString& destPath) {
    // Convert github repo URL to API releases endpoint
    // e.g. https://github.com/user/repo -> https://api.github.com/repos/user/repo/releases/latest
    QString apiUrl = repoUrl;
    apiUrl.replace("https://github.com/", "https://api.github.com/repos/");
    if (apiUrl.endsWith("/")) apiUrl.chop(1);
    apiUrl += "/releases/latest";

    QNetworkRequest req{QUrl(apiUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "ReXGlue-Hub/1.0");

    auto* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, assetName, destPath]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred("API request failed: " + reply->errorString());
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto assets = doc.object()["assets"].toArray();

        QUrl downloadUrl;
        for (const auto& asset : assets) {
            auto obj = asset.toObject();
            if (obj["name"].toString() == assetName) {
                downloadUrl = QUrl(obj["browser_download_url"].toString());
                break;
            }
        }

        if (downloadUrl.isEmpty()) {
            emit errorOccurred("Release asset '" + assetName + "' not found");
            return;
        }

        startDownload(downloadUrl, destPath);
    });
}

void DownloadManager::startDownload(const QUrl& url, const QString& destPath) {
    QDir().mkpath(QFileInfo(destPath).absolutePath());

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "ReXGlue-Hub/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    auto* reply = m_nam.get(req);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 recv, qint64 total) {
                emit progress(recv, total);
            });

    connect(reply, &QNetworkReply::finished, this, [this, reply, destPath]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred("Download failed: " + reply->errorString());
            return;
        }

        QFile f(destPath);
        if (!f.open(QIODevice::WriteOnly)) {
            emit errorOccurred("Cannot write to: " + destPath);
            return;
        }
        f.write(reply->readAll());
        f.close();

        emit finished(destPath);
    });
}
