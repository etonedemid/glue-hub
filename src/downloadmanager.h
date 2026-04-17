#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <functional>

class DownloadManager : public QObject {
    Q_OBJECT
public:
    explicit DownloadManager(QObject* parent = nullptr);

    void downloadRelease(const QString& repoUrl, const QString& assetName,
                         const QString& destPath);

signals:
    void progress(qint64 received, qint64 total);
    void finished(const QString& filePath);
    void errorOccurred(const QString& msg);

private:
    void startDownload(const QUrl& url, const QString& destPath);
    QNetworkAccessManager m_nam;
};
