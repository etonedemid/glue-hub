#include "repomanager.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

// Convert a GitHub repo URL to the GitHub Contents API base.
// Supports both "https://github.com/owner/repo.git" and "https://github.com/owner/repo"
static QString apiBaseFromUrl(const QString& repoUrl)
{
    QString s = repoUrl;
    if (s.endsWith(".git"))
        s.chop(4);
    if (s.contains("github.com")) {
        QStringList parts = s.split("github.com/");
        if (parts.size() == 2)
            return "https://api.github.com/repos/" + parts[1] + "/contents";
    }
    return {};
}

RepoManager::RepoManager(QObject* parent)
    : QObject(parent)
{
    m_repoUrl  = "https://github.com/etonedemid/rexglue-hub-repo.git";
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/hub-repo";
    m_apiBase  = apiBaseFromUrl(m_repoUrl);
}

void RepoManager::setRepoUrl(const QString& url)
{
    m_repoUrl = url;
    m_apiBase = apiBaseFromUrl(url);
}

void RepoManager::setCacheDir(const QString& dir) { m_cacheDir = dir; }

// ── Public entry point ────────────────────────────────────────────────────────

void RepoManager::refresh()
{
    emit refreshStarted();

    if (m_apiBase.isEmpty()) {
        // Non-GitHub URL: fall back to whatever is cached on disk.
        loadGamesFromDir(m_cacheDir);
        emit statusMessage(QString("Found %1 games (cached)").arg(m_games.size()));
        emit refreshFinished();
        return;
    }

    fetchDirListing();
}

// ── Step 1: fetch root directory listing ─────────────────────────────────────

void RepoManager::fetchDirListing()
{
    emit statusMessage("Fetching game catalog...");

    QUrl apiUrl(m_apiBase);
    QNetworkRequest req(apiUrl);
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "glue-hub");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    auto* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit statusMessage("Network error, using cached data");
            loadGamesFromDir(m_cacheDir);
            emit statusMessage(QString("Found %1 games").arg(m_games.size()));
            emit refreshFinished();
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) {
            emit statusMessage("Unexpected catalog response, using cached data");
            loadGamesFromDir(m_cacheDir);
            emit statusMessage(QString("Found %1 games").arg(m_games.size()));
            emit refreshFinished();
            return;
        }

        QStringList dirs;
        for (const auto& v : doc.array()) {
            auto obj = v.toObject();
            if (obj["type"].toString() == "dir")
                dirs << obj["name"].toString();
        }

        if (dirs.isEmpty()) {
            emit statusMessage("No games found in catalog");
            emit refreshFinished();
            return;
        }

        m_games.clear();
        QDir(m_cacheDir).mkpath(".");
        fetchGameInfo(dirs, 0);
    });
}

// ── Step 2: fetch info.json for each game entry (sequential) ──────────────────

void RepoManager::fetchGameInfo(const QStringList& entries, int index)
{
    if (index >= entries.size()) {
        emit statusMessage(QString("Found %1 games").arg(m_games.size()));
        emit refreshFinished();
        return;
    }

    const QString entry = entries[index];
    QUrl infoUrl(m_apiBase + "/" + entry + "/info.json");
    QNetworkRequest req(infoUrl);
    req.setRawHeader("Accept", "application/vnd.github.raw+json");
    req.setRawHeader("User-Agent", "glue-hub");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    auto* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, entries, index, entry]() {
        reply->deleteLater();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();

            // Cache to disk for offline fallback.
            QString gameDir = m_cacheDir + "/" + entry;
            QDir().mkpath(gameDir);
            QFile f(gameDir + "/info.json");
            if (f.open(QIODevice::WriteOnly))
                f.write(data);

            auto info = loadGameInfo(gameDir);
            if (!info.name.isEmpty())
                m_games.append(std::move(info));
        }

        fetchGameInfo(entries, index + 1);
    });
}

// ── Disk fallback ─────────────────────────────────────────────────────────────

void RepoManager::loadGamesFromDir(const QString& dir)
{
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
