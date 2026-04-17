#pragma once

#include <QObject>
#include <QString>
#include <QList>
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
    void loadGamesFromDir(const QString& dir);
    QString m_repoUrl;
    QString m_cacheDir;
    QList<GameInfo> m_games;
};
