#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QKeyEvent>

#include "repomanager.h"
#include "updatechecker.h"
#include "downloadmanager.h"
#include "gameinfo.h"
#include <QProgressBar>

class HubWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit HubWindow(QWidget* parent = nullptr);

private slots:
    void onRefresh();
    void onRefreshFinished();
    void onGameSelected(int index);
    void onSetupClicked();
    void onLaunchClicked();
    void onLaunchWithWineClicked();
    void onThemeChanged(const QString& theme);
    void onEditProfile();
    void onUpdateClicked();
    void onUpdateAvailable(const QString& latestTag, const QString& releaseUrl);
    void onUpdateUpToDate();
    void onUpdateDownloadFinished(const QString& filePath);

private:
    void buildUi();
    void showGameDetail(const GameInfo& info);
    void showEmptyState();
    void updateProfileDisplay();
    void populateAchievements(uint32_t titleId);
    QWidget* buildPlatformTags(const GameInfo& info);
    bool detectWine() const;
    bool detectProton() const;
    QString findWineBinary() const;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void handleKonamiKey(int key);
    void showAchievementToast(const QString& title, const QString& desc, int gamerscore);

    RepoManager m_repo;
    UpdateChecker m_updateChecker;

    // Sidebar
    QListWidget* m_gameList;
    QPushButton* m_refreshBtn;
    QComboBox* m_themeCombo;

    // Profile widgets
    QWidget* m_profileSection;
    QLabel* m_profilePic;
    QLabel* m_profileTag;
    QLabel* m_profileScore;

    // Detail pane
    QWidget* m_detailPanel;
    QLabel* m_bannerLabel;
    QLabel* m_titleLabel;
    QLabel* m_descLabel;
    QWidget* m_platformContainer;
    QPushButton* m_setupBtn;
    QPushButton* m_launchBtn;
    QPushButton* m_wineBtn;
    QPushButton* m_updateBtn;
    QProgressBar* m_updateProgress;
    QLabel* m_updateStatus;
    QLabel* m_wineWarning;
    QLabel* m_statusLabel;
    QLabel* m_emptyLabel;

    // Update state for currently displayed game
    QString m_pendingUpdateUrl;
    QString m_updateInstallDir;
    QString m_updateAssetName;
    QString m_updateGithubRepo;
    QString m_latestTag;
    DownloadManager m_updateDownloader;

    // Achievements
    QWidget* m_achievementsSection;
    QLabel* m_achievementsHeader;
    QVBoxLayout* m_achievementsListLayout;

    // Playtime
    QLabel* m_playtimeLabel;

    // State
    int m_currentIndex = -1;

    // Konami Code easter egg
    QVector<int> m_konamiProgress;
    bool m_konamiTriggered = false;
};
