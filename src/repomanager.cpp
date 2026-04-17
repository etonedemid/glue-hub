#include "repomanager.h"

#include <QDir>
#include <QProcess>
#include <QStandardPaths>

RepoManager::RepoManager(QObject* parent)
    : QObject(parent)
{
    m_repoUrl = "https://github.com/etonedemid/rexglue-hub-repo.git";
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/hub-repo";
}

void RepoManager::setRepoUrl(const QString& url) { m_repoUrl = url; }
void RepoManager::setCacheDir(const QString& dir) { m_cacheDir = dir; }

void RepoManager::refresh() {
    emit refreshStarted();
    emit statusMessage("Syncing game catalog...");

    QDir cacheDir(m_cacheDir);

    if (cacheDir.exists(".git")) {
        // Pull latest
        QProcess git;
        git.setWorkingDirectory(m_cacheDir);
        git.start("git", {"pull", "--ff-only"});
        git.waitForFinished(30000);
        if (git.exitCode() != 0) {
            emit statusMessage("Pull failed, using cached data");
        }
    } else {
        // Clone fresh
        cacheDir.mkpath(".");
        QProcess git;
        git.start("git", {"clone", "--depth", "1", m_repoUrl, m_cacheDir});
        git.waitForFinished(60000);
        if (git.exitCode() != 0) {
            QString err = QString::fromUtf8(git.readAllStandardError());
            emit errorOccurred("Failed to clone hub repo: " + err);
            emit refreshFinished();
            return;
        }
    }

    loadGamesFromDir(m_cacheDir);
    emit statusMessage(QString("Found %1 games").arg(m_games.size()));
    emit refreshFinished();
}

void RepoManager::loadGamesFromDir(const QString& dir) {
    m_games.clear();
    QDir root(dir);
    for (const auto& entry : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString gamePath = root.filePath(entry);
        if (QFile::exists(gamePath + "/info.json")) {
            auto info = loadGameInfo(gamePath);
            if (!info.name.isEmpty())
                m_games.append(std::move(info));
        }
    }
}
