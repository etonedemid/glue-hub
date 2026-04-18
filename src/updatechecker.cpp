#include "updatechecker.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
{}

void UpdateChecker::check(const QString& githubRepo, const QString& installedTag) {
    if (githubRepo.isEmpty()) {
        emit upToDate();
        return;
    }

    QString apiUrl = githubRepo;
    apiUrl.replace("https://github.com/", "https://api.github.com/repos/");
    if (apiUrl.endsWith('/')) apiUrl.chop(1);
    apiUrl += "/releases/latest";

    QNetworkRequest req{QUrl(apiUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "ReXGlue-Hub/1.0");

    auto* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, installedTag]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred("Update check failed: " + reply->errorString());
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit upToDate();
            return;
        }

        auto obj = doc.object();
        QString latestTag = obj["tag_name"].toString();
        QString releaseUrl = obj["html_url"].toString();

        if (latestTag.isEmpty()) {
            emit upToDate();
            return;
        }

        if (installedTag.isEmpty() || compareTags(installedTag, latestTag) < 0) {
            emit updateAvailable(latestTag, releaseUrl);
        } else {
            emit upToDate();
        }
    });
}

// Compare two version tags, stripping a leading 'v' before comparing.
// Each component is compared numerically so "v1.10.0" > "v1.9.0".
// Returns negative / zero / positive.
int UpdateChecker::compareTags(const QString& a, const QString& b) {
    auto strip = [](const QString& s) {
        return (s.startsWith('v') || s.startsWith('V')) ? s.mid(1) : s;
    };

    // Only compare the numeric version prefix (stop at '-', '+', etc.)
    auto numericPrefix = [](const QString& s) {
        int i = 0;
        while (i < s.size() && (s[i].isDigit() || s[i] == '.')) ++i;
        return s.left(i);
    };

    QStringList partsA = numericPrefix(strip(a)).split('.', Qt::SkipEmptyParts);
    QStringList partsB = numericPrefix(strip(b)).split('.', Qt::SkipEmptyParts);

    int maxLen = qMax(partsA.size(), partsB.size());
    for (int i = 0; i < maxLen; ++i) {
        int va = (i < partsA.size()) ? partsA[i].toInt() : 0;
        int vb = (i < partsB.size()) ? partsB[i].toInt() : 0;
        if (va != vb)
            return va - vb;
    }
    return 0;
}
