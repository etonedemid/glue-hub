#pragma once

#include <QObject>
#include <QNetworkAccessManager>

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    // Check if a newer release exists for the given GitHub repo.
    // Emits updateAvailable(latestTag) if installedTag < latestTag,
    // or upToDate() if already on the latest version.
    void check(const QString& githubRepo, const QString& installedTag);

signals:
    void updateAvailable(const QString& latestTag, const QString& releaseUrl);
    void upToDate();
    void errorOccurred(const QString& msg);

private:
    // Returns -1 / 0 / +1 like strcmp: a < b / a == b / a > b
    // Handles tags like "v1.0.2", "1.0.2", "v1.2.3-rc1"
    static int compareTags(const QString& a, const QString& b);

    QNetworkAccessManager m_nam;
};
