#include "hubwindow.h"
#include "setupdialog.h"
#include "gamerprofile.h"
#include "profiledialog.h"
#include "thememanager.h"
#include "updatechecker.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QPixmap>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <QComboBox>
#include <QStyle>
#include <QPainter>
#include <QMenu>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>

HubWindow::HubWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Glue Hub");
    setMinimumSize(960, 600);
    resize(1100, 700);

    buildUi();

    connect(&m_repo, &RepoManager::refreshFinished, this, &HubWindow::onRefreshFinished);
    connect(&m_repo, &RepoManager::statusMessage, this, [this](const QString& msg) {
        m_statusLabel->setText(msg);
    });
    connect(&m_repo, &RepoManager::errorOccurred, this, [this](const QString& msg) {
        m_statusLabel->setText("Error: " + msg);
    });

    connect(&m_updateChecker, &UpdateChecker::updateAvailable,
            this, &HubWindow::onUpdateAvailable);
    connect(&m_updateChecker, &UpdateChecker::upToDate,
            this, &HubWindow::onUpdateUpToDate);

    // Install event filter on the application to catch keys globally
    qApp->installEventFilter(this);

    // Auto-refresh on start
    QMetaObject::invokeMethod(this, &HubWindow::onRefresh, Qt::QueuedConnection);
}

void HubWindow::buildUi() {
    auto* central = new QWidget;
    setCentralWidget(central);

    auto* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Sidebar ──
    auto* sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(300);
    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 0, 12);
    sideLayout->setSpacing(0);

    auto* title = new QLabel("Glue Hub");
    title->setObjectName("sidebarTitle");
    sideLayout->addWidget(title);

    auto* subtitle = new QLabel("Xbox 360 Recompilation Projects");
    subtitle->setObjectName("sidebarSubtitle");
    sideLayout->addWidget(subtitle);

    // Profile section
    m_profileSection = new QWidget;
    m_profileSection->setObjectName("profileSection");
    auto* profileLayout = new QHBoxLayout(m_profileSection);
    profileLayout->setContentsMargins(12, 8, 12, 8);
    profileLayout->setSpacing(8);

    m_profilePic = new QLabel;
    m_profilePic->setObjectName("profilePicLabel");
    m_profilePic->setFixedSize(40, 40);
    m_profilePic->setScaledContents(true);
    profileLayout->addWidget(m_profilePic);

    auto* profileText = new QVBoxLayout;
    profileText->setSpacing(0);
    m_profileTag = new QLabel;
    m_profileTag->setObjectName("profileTag");
    profileText->addWidget(m_profileTag);
    m_profileScore = new QLabel;
    m_profileScore->setObjectName("profileScore");
    profileText->addWidget(m_profileScore);
    profileLayout->addLayout(profileText, 1);

    auto* editProfileBtn = new QPushButton("...");
    editProfileBtn->setObjectName("profileEditBtn");
    editProfileBtn->setFixedSize(28, 28);
    editProfileBtn->setToolTip("Edit Profile");
    connect(editProfileBtn, &QPushButton::clicked, this, &HubWindow::onEditProfile);
    profileLayout->addWidget(editProfileBtn);

    sideLayout->addWidget(m_profileSection);

    m_refreshBtn = new QPushButton("↻  Refresh Games");
    m_refreshBtn->setObjectName("refreshBtn");
    connect(m_refreshBtn, &QPushButton::clicked, this, &HubWindow::onRefresh);
    sideLayout->addWidget(m_refreshBtn);

    m_gameList = new QListWidget;
    m_gameList->setObjectName("gameList");
    m_gameList->setIconSize(QSize(40, 40));
    connect(m_gameList, &QListWidget::currentRowChanged, this, &HubWindow::onGameSelected);
    sideLayout->addWidget(m_gameList, 1);

    // Theme picker
    auto* themeLayout = new QHBoxLayout;
    themeLayout->setContentsMargins(16, 4, 16, 0);
    auto* themeLabel = new QLabel("Theme:");
    themeLabel->setObjectName("statusLabel");
    themeLayout->addWidget(themeLabel);
    m_themeCombo = new QComboBox;
    m_themeCombo->addItems(ThemeManager::instance().availableThemes());
    m_themeCombo->setCurrentText(ThemeManager::instance().currentTheme());
    connect(m_themeCombo, &QComboBox::currentTextChanged, this, &HubWindow::onThemeChanged);
    themeLayout->addWidget(m_themeCombo, 1);
    sideLayout->addLayout(themeLayout);

    m_statusLabel = new QLabel("Starting...");
    m_statusLabel->setObjectName("statusLabel");
    sideLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(sidebar);

    // ── Detail panel ──
    auto* detailScroll = new QScrollArea;
    detailScroll->setWidgetResizable(true);
    detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_detailPanel = new QWidget;
    m_detailPanel->setObjectName("detailPanel");
    auto* detailLayout = new QVBoxLayout(m_detailPanel);
    detailLayout->setContentsMargins(32, 24, 32, 24);
    detailLayout->setSpacing(16);

    // Banner
    m_bannerLabel = new QLabel;
    m_bannerLabel->setObjectName("bannerLabel");
    m_bannerLabel->setFixedHeight(320);
    m_bannerLabel->setAlignment(Qt::AlignCenter);
    m_bannerLabel->setScaledContents(false);
    detailLayout->addWidget(m_bannerLabel);

    // Title
    m_titleLabel = new QLabel;
    m_titleLabel->setObjectName("gameTitle");
    m_titleLabel->setWordWrap(true);
    detailLayout->addWidget(m_titleLabel);

    // Platform tags container
    m_platformContainer = new QWidget;
    m_platformContainer->setLayout(new QHBoxLayout);
    m_platformContainer->layout()->setContentsMargins(0, 0, 0, 0);
    m_platformContainer->layout()->setSpacing(8);
    detailLayout->addWidget(m_platformContainer);

    // Description
    m_descLabel = new QLabel;
    m_descLabel->setObjectName("gameDescription");
    m_descLabel->setWordWrap(true);
    detailLayout->addWidget(m_descLabel);

    // Buttons
    auto* btnLayout = new QHBoxLayout;
    m_setupBtn = new QPushButton("Set Up Game");
    m_setupBtn->setObjectName("setupBtn");
    m_setupBtn->setVisible(false);
    connect(m_setupBtn, &QPushButton::clicked, this, &HubWindow::onSetupClicked);
    btnLayout->addWidget(m_setupBtn);

    m_launchBtn = new QPushButton("▶  Launch");
    m_launchBtn->setObjectName("launchBtn");
    m_launchBtn->setVisible(false);
    connect(m_launchBtn, &QPushButton::clicked, this, &HubWindow::onLaunchClicked);
    btnLayout->addWidget(m_launchBtn);

    m_updateBtn = new QPushButton("↑ Update");
    m_updateBtn->setObjectName("updateBtn");
    m_updateBtn->setVisible(false);
    connect(m_updateBtn, &QPushButton::clicked, this, &HubWindow::onUpdateClicked);
    btnLayout->addWidget(m_updateBtn);

    m_wineBtn = new QPushButton("🍷  Launch with Wine");
    m_wineBtn->setObjectName("wineBtn");
    m_wineBtn->setVisible(false);
    connect(m_wineBtn, &QPushButton::clicked, this, &HubWindow::onLaunchWithWineClicked);
    btnLayout->addWidget(m_wineBtn);

    btnLayout->addStretch();
    detailLayout->addLayout(btnLayout);

    // Wine warning
    m_wineWarning = new QLabel("⚠ Wine/Proton may result in lower performance and compatibility issues.");
    m_wineWarning->setObjectName("wineWarningLabel");
    m_wineWarning->setWordWrap(true);
    m_wineWarning->setVisible(false);
    detailLayout->addWidget(m_wineWarning);

    // Playtime label
    m_playtimeLabel = new QLabel;
    m_playtimeLabel->setObjectName("playtimeLabel");
    m_playtimeLabel->setVisible(false);
    detailLayout->addWidget(m_playtimeLabel);

    // Achievements section
    m_achievementsSection = new QWidget;
    m_achievementsSection->setObjectName("achievementsSection");
    m_achievementsSection->setVisible(false);
    auto* achSectionLayout = new QVBoxLayout(m_achievementsSection);
    achSectionLayout->setContentsMargins(0, 8, 0, 0);
    achSectionLayout->setSpacing(8);

    m_achievementsHeader = new QLabel("Achievements");
    m_achievementsHeader->setObjectName("achievementsHeader");
    achSectionLayout->addWidget(m_achievementsHeader);

    m_achievementsListLayout = new QVBoxLayout;
    m_achievementsListLayout->setSpacing(4);
    achSectionLayout->addLayout(m_achievementsListLayout);

    detailLayout->addWidget(m_achievementsSection);

    detailLayout->addStretch();

    // Empty state
    m_emptyLabel = new QLabel("Select a game from the sidebar");
    m_emptyLabel->setObjectName("emptyLabel");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    detailLayout->addWidget(m_emptyLabel);

    detailScroll->setWidget(m_detailPanel);
    mainLayout->addWidget(detailScroll, 1);

    showEmptyState();
    updateProfileDisplay();
}

void HubWindow::onRefresh() {
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText("Syncing...");
    m_gameList->clear();
    m_repo.refresh();
}

void HubWindow::onRefreshFinished() {
    m_refreshBtn->setEnabled(true);
    m_refreshBtn->setText("↻  Refresh Games");

    m_gameList->clear();
    for (const auto& game : m_repo.games()) {
        auto* item = new QListWidgetItem;
        item->setText(game.name);
        if (!game.boxart.isNull()) {
            item->setIcon(QIcon(game.boxart.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        }
        m_gameList->addItem(item);
    }

    if (m_repo.games().isEmpty()) {
        showEmptyState();
    } else if (m_gameList->count() > 0) {
        m_gameList->setCurrentRow(0);
    }
}

void HubWindow::onGameSelected(int index) {
    if (index < 0 || index >= m_repo.games().size()) {
        showEmptyState();
        return;
    }
    m_currentIndex = index;
    showGameDetail(m_repo.games().at(index));
}

void HubWindow::showEmptyState() {
    m_bannerLabel->clear();
    m_bannerLabel->setVisible(false);
    m_titleLabel->setVisible(false);
    m_descLabel->setVisible(false);
    m_platformContainer->setVisible(false);
    m_setupBtn->setVisible(false);
    m_launchBtn->setVisible(false);
    m_updateBtn->setVisible(false);
    m_wineBtn->setVisible(false);
    m_wineWarning->setVisible(false);
    m_achievementsSection->setVisible(false);
    m_playtimeLabel->setVisible(false);
    m_emptyLabel->setVisible(true);
}

void HubWindow::showGameDetail(const GameInfo& info) {
    m_emptyLabel->setVisible(false);
    m_bannerLabel->setVisible(true);
    m_titleLabel->setVisible(true);
    m_descLabel->setVisible(true);
    m_platformContainer->setVisible(true);

    // Banner - scale to fill width while keeping aspect ratio
    if (!info.banner.isNull()) {
        int w = m_bannerLabel->width();
        if (w < 200) w = 800; // initial sizing before layout
        QPixmap scaled = info.banner.scaledToWidth(w, Qt::SmoothTransformation);
        if (scaled.height() > 320)
            scaled = info.banner.scaledToHeight(320, Qt::SmoothTransformation);
        m_bannerLabel->setPixmap(scaled);
    } else {
        m_bannerLabel->clear();
        m_bannerLabel->setText("No banner");
    }

    m_titleLabel->setText(info.name);
    m_descLabel->setText(info.description);

    // Rebuild platform tags
    QLayoutItem* child;
    while ((child = m_platformContainer->layout()->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    auto* tagsLayout = static_cast<QHBoxLayout*>(m_platformContainer->layout());
    QString current = currentPlatformName();
    bool currentSupported = false;

    for (const auto& plat : info.platforms) {
        auto* tag = new QLabel(plat);
        if (plat.compare(current, Qt::CaseInsensitive) == 0) {
            tag->setObjectName("platformTagCurrent");
            currentSupported = true;
        } else {
            tag->setObjectName("platformTag");
        }
        // Force style refresh for dynamically created widgets
        tag->setStyleSheet(tag->styleSheet());
        tag->style()->unpolish(tag);
        tag->style()->polish(tag);
        tagsLayout->addWidget(tag);
    }
    tagsLayout->addStretch();

    // Show setup button only if current platform is supported
    m_setupBtn->setVisible(currentSupported);
    m_setupBtn->setEnabled(currentSupported);

    // Check if already installed: prefer sentinel file, fall back to exe/AppImage check
    QString installDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                         + "/ReXGlue/" + info.id;
    bool installed = QFile::exists(installDir + "/rexglue-setup.json");
    if (!installed) {
        QDir idir(installDir);
        if (!idir.entryList({"*.AppImage", "*.appimage"}, QDir::Files).isEmpty())
            installed = true;
        if (!installed) {
            for (const auto& sub : idir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                QDir sdir(idir.filePath(sub));
                for (const auto& f : sdir.entryList({"*.exe"}, QDir::Files)) {
                    if (!f.contains("extract-xiso", Qt::CaseInsensitive)) {
                        installed = true;
                        break;
                    }
                }
                if (installed) break;
            }
        }
    }
    m_launchBtn->setVisible(installed && currentSupported);

    // Update button — hidden until the async check resolves
    m_updateBtn->setVisible(false);
    m_pendingUpdateUrl.clear();
    if (installed && !info.githubRepo.isEmpty()) {
        // Read the installed version tag from the setup config
        QString installedTag;
        {
            QFile cfgFile(installDir + "/rexglue-setup.json");
            if (cfgFile.open(QIODevice::ReadOnly)) {
                auto doc = QJsonDocument::fromJson(cfgFile.readAll());
                installedTag = doc.object()["installed_version"].toString();
            }
        }
        m_updateChecker.check(info.githubRepo, installedTag);
    }

    // Wine/Proton: show for Windows-only games on Linux
    bool isLinux = (currentPlatformName() == "Linux");
    bool hasWindows = info.supportsPlatform("Windows");
    bool hasWine = detectWine() || detectProton();
    bool showWine = isLinux && hasWindows && !currentSupported && installed && hasWine;
    m_wineBtn->setVisible(showWine);
    m_wineWarning->setVisible(showWine);

    // Playtime
    if (info.titleId != 0) {
        auto allPlaytime = GamerProfileHub::instance().loadAllPlaytime();
        bool found = false;
        for (const auto& pt : allPlaytime) {
            if (pt.title_id == info.titleId && pt.total_seconds > 0) {
                m_playtimeLabel->setText(QString("Playtime: %1").arg(pt.formattedTime()));
                m_playtimeLabel->setVisible(true);
                found = true;
                break;
            }
        }
        if (!found) m_playtimeLabel->setVisible(false);
    } else {
        m_playtimeLabel->setVisible(false);
    }

    // Achievements
    populateAchievements(info.titleId);
}

QWidget* HubWindow::buildPlatformTags(const GameInfo& info) {
    auto* w = new QWidget;
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QString current = currentPlatformName();
    for (const auto& plat : info.platforms) {
        auto* tag = new QLabel(plat);
        if (plat.compare(current, Qt::CaseInsensitive) == 0)
            tag->setObjectName("platformTagCurrent");
        else
            tag->setObjectName("platformTag");
        layout->addWidget(tag);
    }
    layout->addStretch();
    return w;
}

void HubWindow::onSetupClicked() {
    if (m_currentIndex < 0 || m_currentIndex >= m_repo.games().size())
        return;

    const auto& info = m_repo.games().at(m_currentIndex);
    SetupDialog dlg(info, this);
    if (dlg.exec() == QDialog::Accepted) {
        // Refresh detail to show launch button
        showGameDetail(info);
    }
}

void HubWindow::onLaunchClicked() {
    if (m_currentIndex < 0 || m_currentIndex >= m_repo.games().size())
        return;

    const auto& info = m_repo.games().at(m_currentIndex);
    QString installDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                         + "/ReXGlue/" + info.id;

    // Find the executable
    QDir dir(installDir);
    QStringList execs;

    // Look for AppImage
    for (const auto& f : dir.entryList({"*.AppImage", "*.appimage"}, QDir::Files))
        execs << dir.filePath(f);

    // Look for the exe (Windows)
    for (const auto& f : dir.entryList({"*.exe"}, QDir::Files))
        execs << dir.filePath(f);

    // Look for game exe / bare executable in subdirs
    if (execs.isEmpty()) {
        for (const auto& subdir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QDir sub(dir.filePath(subdir));
            // Windows: .exe files, skip known utility binaries
            for (const auto& f : sub.entryList({"*.exe"}, QDir::Files)) {
                if (!f.contains("extract-xiso", Qt::CaseInsensitive))
                    execs << sub.filePath(f);
            }
            // Linux/macOS: executable bit set, skip DLLs and .so files
            if (execs.isEmpty()) {
                for (const auto& f : sub.entryList(QDir::Files | QDir::Executable)) {
                    if (!f.endsWith(".dll", Qt::CaseInsensitive) &&
                        !f.endsWith(".so", Qt::CaseInsensitive) &&
                        !f.contains("extract-xiso", Qt::CaseInsensitive))
                        execs << sub.filePath(f);
                }
            }
        }
    }

    if (execs.isEmpty()) {
        QMessageBox::warning(this, "Launch Failed", "No executable found in:\n" + installDir);
        return;
    }

    // Find assets — read persisted assets path from setup config
    QString assetsDir;
    {
        QFile cfgFile(installDir + "/rexglue-setup.json");
        if (cfgFile.open(QIODevice::ReadOnly)) {
            auto doc = QJsonDocument::fromJson(cfgFile.readAll());
            assetsDir = doc.object()["assets_path"].toString();
        }
    }
    // Fallback to installDir/assets for legacy installs
    if (assetsDir.isEmpty() || !QDir(assetsDir).exists())
        assetsDir = QDir(installDir + "/assets").exists() ? installDir + "/assets" : QString();

    QStringList args;
    if (!assetsDir.isEmpty())
        args << assetsDir;

    // Use the exe's own directory as working dir so renut.toml is found correctly
    QString exeDir = QFileInfo(execs.first()).absolutePath();
    QProcess::startDetached(execs.first(), args, exeDir);
}

void HubWindow::onLaunchWithWineClicked() {
    if (m_currentIndex < 0 || m_currentIndex >= m_repo.games().size())
        return;

    auto ret = QMessageBox::warning(this, "Wine/Proton Performance Warning",
        "Running Windows games through Wine/Proton may result in:\n\n"
        "• Lower performance compared to native builds\n"
        "• Graphics glitches or rendering issues\n"
        "• Potential audio desync\n"
        "• Controller mapping issues\n\n"
        "Are you sure you want to continue?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    const auto& info = m_repo.games().at(m_currentIndex);
    QString installDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                         + "/ReXGlue/" + info.id;

    QDir dir(installDir);
    QStringList execs;
    for (const auto& f : dir.entryList({"*.exe"}, QDir::Files))
        execs << dir.filePath(f);

    if (execs.isEmpty()) {
        for (const auto& subdir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QDir sub(dir.filePath(subdir));
            for (const auto& f : sub.entryList({"*.exe"}, QDir::Files))
                execs << sub.filePath(f);
        }
    }

    if (execs.isEmpty()) {
        // Also check subdirs for .exe
        for (const auto& subdir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QDir sub(dir.filePath(subdir));
            for (const auto& f : sub.entryList({"*.exe"}, QDir::Files)) {
                if (!f.contains("extract-xiso", Qt::CaseInsensitive))
                    execs << sub.filePath(f);
            }
        }
    }

    if (execs.isEmpty()) {
        QMessageBox::warning(this, "Launch Failed", "No Windows executable found in:\n" + installDir);
        return;
    }

    QString wineBin = findWineBinary();
    if (wineBin.isEmpty()) {
        QMessageBox::warning(this, "Wine Not Found",
            "Could not find Wine or Proton.\n"
            "Please install Wine: https://www.winehq.org/");
        return;
    }

    // Find assets — read persisted assets path from setup config
    QString assetsDir;
    {
        QFile cfgFile(installDir + "/rexglue-setup.json");
        if (cfgFile.open(QIODevice::ReadOnly)) {
            auto doc = QJsonDocument::fromJson(cfgFile.readAll());
            assetsDir = doc.object()["assets_path"].toString();
        }
    }
    if (assetsDir.isEmpty() || !QDir(assetsDir).exists())
        assetsDir = QDir(installDir + "/assets").exists() ? installDir + "/assets" : QString();

    QStringList wineArgs;
    wineArgs << execs.first();
    if (!assetsDir.isEmpty())
        wineArgs << assetsDir;

    QString exeDir = QFileInfo(execs.first()).absolutePath();
    QProcess::startDetached(wineBin, wineArgs, exeDir);
}

void HubWindow::onThemeChanged(const QString& theme) {
    ThemeManager::instance().applyTheme(theme);
}

void HubWindow::onEditProfile() {
    auto& gph = GamerProfileHub::instance();
    auto profiles = gph.enumerateProfiles();
    quint64 currentXuid = gph.activeXuid();

    QMenu menu(this);
    menu.setObjectName("profileSwitchMenu");

    // List all existing profiles
    for (const auto& p : profiles) {
        QString label = p.gamertag;
        if (p.xuid == currentXuid)
            label += "  \u2713";   // checkmark for active profile
        QAction* action = menu.addAction(label);
        action->setData(p.xuid);
    }

    if (!profiles.isEmpty())
        menu.addSeparator();

    QAction* createAction = menu.addAction("+ Create New Profile");

    QAction* chosen = menu.exec(QCursor::pos());
    if (!chosen)
        return;

    if (chosen == createAction) {
        ProfileDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            gph.createProfile(dlg.gamertag(), dlg.selectedGamepic());
            QSettings settings;
            settings.setValue("profile/remembered_xuid", gph.activeXuid());
        }
    } else {
        quint64 xuid = chosen->data().toULongLong();
        if (xuid != currentXuid) {
            gph.setActiveXuid(xuid);
            gph.load();
            QSettings settings;
            settings.setValue("profile/remembered_xuid", xuid);
        }
    }
    updateProfileDisplay();
}

void HubWindow::populateAchievements(uint32_t titleId) {
    // Clear previous achievement items
    while (auto* item = m_achievementsListLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    if (titleId == 0) {
        m_achievementsSection->setVisible(false);
        return;
    }

    auto achievements = GamerProfileHub::instance().loadAchievements(titleId);
    if (achievements.isEmpty()) {
        m_achievementsSection->setVisible(false);
        return;
    }

    int unlocked = 0;
    int totalGs = 0;
    int unlockedGs = 0;
    for (const auto& a : achievements) {
        totalGs += a.gamerscore;
        if (a.isUnlocked()) {
            unlocked++;
            unlockedGs += a.gamerscore;
        }
    }

    m_achievementsHeader->setText(
        QString("Achievements (%1/%2 · %3/%4 G)")
            .arg(unlocked).arg(achievements.size())
            .arg(unlockedGs).arg(totalGs));

    for (const auto& a : achievements) {
        auto* row = new QWidget;
        row->setObjectName(a.isUnlocked() ? "achRowUnlocked" : "achRowLocked");
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(8, 6, 8, 6);
        rowLayout->setSpacing(12);

        // Achievement icon
        if (!a.image_path.isEmpty() && QFile::exists(a.image_path)) {
            auto* icon = new QLabel;
            icon->setPixmap(QPixmap(a.image_path).scaled(
                32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            icon->setFixedSize(32, 32);
            rowLayout->addWidget(icon);
        }

        // Name + description
        auto* textLayout = new QVBoxLayout;
        textLayout->setSpacing(0);

        auto* nameLabel = new QLabel(a.name);
        nameLabel->setObjectName(a.isUnlocked() ? "achNameUnlocked" : "achNameLocked");
        QFont nameFont = nameLabel->font();
        nameFont.setBold(true);
        nameLabel->setFont(nameFont);
        textLayout->addWidget(nameLabel);

        auto* descLabel = new QLabel(a.description);
        descLabel->setObjectName("achDescription");
        descLabel->setWordWrap(true);
        textLayout->addWidget(descLabel);

        rowLayout->addLayout(textLayout, 1);

        // Gamerscore
        auto* gsLabel = new QLabel(QString("%1G").arg(a.gamerscore));
        gsLabel->setObjectName(a.isUnlocked() ? "achScoreUnlocked" : "achScoreLocked");
        rowLayout->addWidget(gsLabel);

        m_achievementsListLayout->addWidget(row);
    }

    m_achievementsSection->setVisible(true);
}

void HubWindow::updateProfileDisplay() {
    auto& gph = GamerProfileHub::instance();
    const auto& p = gph.profile();

    m_profileTag->setText(p.gamertag);
    m_profileScore->setText(QString("%1G").arg(p.totalGamerscore));

    if (!p.gamerpicPath.isEmpty() && QFile::exists(p.gamerpicPath)) {
        m_profilePic->setPixmap(QPixmap(p.gamerpicPath).scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        // Default avatar - just show first letter
        QPixmap px(40, 40);
        px.fill(QColor("#0f3460"));
        QPainter painter(&px);
        painter.setPen(Qt::white);
        painter.setFont(QFont("sans-serif", 18, QFont::Bold));
        painter.drawText(px.rect(), Qt::AlignCenter,
                         p.gamertag.isEmpty() ? "?" : p.gamertag.left(1).toUpper());
        m_profilePic->setPixmap(px);
    }
}

bool HubWindow::detectWine() const {
    return !findWineBinary().isEmpty();
}

bool HubWindow::detectProton() const {
    // Check common Proton locations
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QStringList protonPaths = {
        home + "/.steam/steam/steamapps/common/Proton - Experimental/proton",
        home + "/.steam/steam/steamapps/common/Proton 9.0/proton",
        home + "/.steam/steam/steamapps/common/Proton 8.0/proton",
    };
    for (const auto& p : protonPaths) {
        if (QFile::exists(p)) return true;
    }
    return false;
}

QString HubWindow::findWineBinary() const {
    // Try wine first
    QProcess proc;
    proc.start("which", {"wine"});
    proc.waitForFinished(3000);
    QString winePath = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (!winePath.isEmpty() && QFile::exists(winePath))
        return winePath;

    // Try wine64
    proc.start("which", {"wine64"});
    proc.waitForFinished(3000);
    winePath = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (!winePath.isEmpty() && QFile::exists(winePath))
        return winePath;

    return {};
}

void HubWindow::onUpdateClicked() {
    if (!m_pendingUpdateUrl.isEmpty())
        QDesktopServices::openUrl(QUrl(m_pendingUpdateUrl));
}

void HubWindow::onUpdateAvailable(const QString& latestTag, const QString& releaseUrl) {
    m_pendingUpdateUrl = releaseUrl;
    m_updateBtn->setText(QString("↑ Update (%1)").arg(latestTag));
    m_updateBtn->setVisible(true);
}

void HubWindow::onUpdateUpToDate() {
    m_updateBtn->setVisible(false);
    m_pendingUpdateUrl.clear();
}

// ── Konami Code Easter Egg ──

bool HubWindow::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        handleKonamiKey(ke->key());
    }
    return QMainWindow::eventFilter(obj, event);
}

void HubWindow::handleKonamiKey(int key) {
    static const QVector<int> konamiCode = {
        Qt::Key_Up, Qt::Key_Up, Qt::Key_Down, Qt::Key_Down,
        Qt::Key_Left, Qt::Key_Right, Qt::Key_Left, Qt::Key_Right,
        Qt::Key_B, Qt::Key_A
    };

    if (m_konamiTriggered)
        return;

    int expected = konamiCode.value(m_konamiProgress.size(), -1);
    if (key == expected) {
        m_konamiProgress.append(key);
        if (m_konamiProgress.size() == konamiCode.size()) {
            m_konamiTriggered = true;
            showAchievementToast(
                "ReXGlue OG",
                "You know the code. Welcome home.",
                0
            );
        }
    } else {
        m_konamiProgress.clear();
        if (key == konamiCode.first())
            m_konamiProgress.append(key);
    }
}

void HubWindow::showAchievementToast(const QString& title, const QString& desc, int gamerscore) {
    // Xbox 360-style achievement popup: slides up from bottom-right with the iconic sound-like feel

    auto* toast = new QWidget(this);
    toast->setObjectName("achievementToast");
    toast->setAttribute(Qt::WA_DeleteOnClose);
    toast->setFixedSize(360, 80);

    auto* layout = new QHBoxLayout(toast);
    layout->setContentsMargins(12, 8, 16, 8);
    layout->setSpacing(12);

    // Achievement icon (Xbox-style circle)
    auto* iconLabel = new QLabel;
    iconLabel->setFixedSize(48, 48);
    iconLabel->setAlignment(Qt::AlignCenter);
    QPixmap iconPm(48, 48);
    iconPm.fill(Qt::transparent);
    {
        QPainter p(&iconPm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor("#107C10")); // Xbox green
        p.setPen(Qt::NoPen);
        p.drawEllipse(2, 2, 44, 44);
        p.setPen(QColor("#ffffff"));
        QFont f = p.font();
        f.setPixelSize(20);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(2, 2, 44, 44), Qt::AlignCenter, "★");
    }
    iconLabel->setPixmap(iconPm);
    layout->addWidget(iconLabel);

    // Text
    auto* textLayout = new QVBoxLayout;
    textLayout->setSpacing(2);

    auto* headerLabel = new QLabel("Achievement Unlocked");
    headerLabel->setStyleSheet("color: #b7f400; font-size: 11px; font-weight: bold; background: transparent;");
    textLayout->addWidget(headerLabel);

    auto* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("color: #ffffff; font-size: 14px; font-weight: bold; background: transparent;");
    textLayout->addWidget(titleLabel);

    auto* descLabel = new QLabel(desc + (gamerscore > 0 ? QString(" — %1G").arg(gamerscore) : ""));
    descLabel->setStyleSheet("color: #cccccc; font-size: 11px; background: transparent;");
    textLayout->addWidget(descLabel);

    layout->addLayout(textLayout, 1);

    toast->setStyleSheet(
        "#achievementToast {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 rgba(16, 124, 16, 230), stop:0.03 rgba(16, 124, 16, 230),"
        "    stop:0.04 rgba(30, 30, 30, 240), stop:1 rgba(40, 40, 40, 240));"
        "  border: 2px solid #107C10;"
        "  border-radius: 8px;"
        "}"
    );

    // Position: bottom-right, start off-screen
    int startY = height();
    int endY = height() - 100;
    int xPos = width() - 380;
    toast->move(xPos, startY);
    toast->show();
    toast->raise();

    // Slide up animation
    auto* slideUp = new QPropertyAnimation(toast, "pos");
    slideUp->setDuration(400);
    slideUp->setStartValue(QPoint(xPos, startY));
    slideUp->setEndValue(QPoint(xPos, endY));
    slideUp->setEasingCurve(QEasingCurve::OutCubic);

    // Slide back down after a pause
    auto* slideDown = new QPropertyAnimation(toast, "pos");
    slideDown->setDuration(500);
    slideDown->setStartValue(QPoint(xPos, endY));
    slideDown->setEndValue(QPoint(xPos, startY));
    slideDown->setEasingCurve(QEasingCurve::InCubic);

    auto* group = new QSequentialAnimationGroup(toast);
    group->addAnimation(slideUp);
    group->addPause(3500);
    group->addAnimation(slideDown);

    connect(group, &QSequentialAnimationGroup::finished, toast, &QWidget::close);
    group->start(QAbstractAnimation::DeleteWhenStopped);
}
