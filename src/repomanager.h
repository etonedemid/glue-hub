#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "gameinfo.h"

class RepoManager : public QObject {
    Q_OBJECT
public:
    explicit RepoManager(QObject* parent = nullptr);

    void setRepoUrl(const QString& url);
    void setCacheDir(const QString& dir);

    void refresh();
    const QList<GameInfo>& games() const { return m_games; }

signals:
    void refreshStarted();
    void refreshFinished();
    void statusMessage(const QString& msg);
    void errorOccurred(const QString& msg);

private:
    void fetchDirListing();
    void fetchGameInfo(const QStringList& entries, int index);
    void loadGamesFromDir(const QString& dir);

    QNetworkAccessManager m_nam;
    QString m_repoUrl;
    QString m_cacheDir;
    QString m_apiBase;   // "https://api.github.com/repos/owner/repo/contents"
    QString m_rawBase;   // "https://raw.githubusercontent.com/owner/repo/main"
    QList<GameInfo> m_games;
};
