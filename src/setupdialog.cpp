#include "setupdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QProcess>
#include <QMessageBox>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

SetupDialog::SetupDialog(const GameInfo& info, QWidget* parent)
    : QDialog(parent), m_info(info)
{
    setWindowTitle("Set Up: " + info.name);
    setMinimumWidth(520);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    // Header
    auto* header = new QLabel(info.name);
    header->setObjectName("setupHeader");
    layout->addWidget(header);

    auto* plat = new QLabel("Platform: " + currentPlatformName());
    plat->setObjectName("setupPlatformLabel");
    layout->addWidget(plat);

    // Game assets section
    auto* assetsGroup = new QGroupBox("Game Assets");
    assetsGroup->setObjectName("setupGroupBox");

    auto* assetsLayout = new QVBoxLayout(assetsGroup);

    auto* isoLabel = new QLabel("Xbox 360 ISO file:");
    auto* isoRow = new QHBoxLayout;
    m_isoPath = new QLineEdit;
    m_isoPath->setPlaceholderText("Path to .iso file (optional if assets provided)");
    auto* isoBrowse = new QPushButton("Browse...");
    connect(isoBrowse, &QPushButton::clicked, this, &SetupDialog::onBrowseIso);
    isoRow->addWidget(m_isoPath, 1);
    isoRow->addWidget(isoBrowse);
    assetsLayout->addWidget(isoLabel);
    assetsLayout->addLayout(isoRow);

    auto* orLabel = new QLabel("— OR —");
    orLabel->setAlignment(Qt::AlignCenter);
    orLabel->setObjectName("setupOrLabel");
    assetsLayout->addWidget(orLabel);

    auto* assetsLabel = new QLabel("Extracted assets folder:");
    auto* assetsRow = new QHBoxLayout;
    m_assetsPath = new QLineEdit;
    m_assetsPath->setPlaceholderText("Path to extracted game assets folder");
    auto* assetsBrowse = new QPushButton("Browse...");
    connect(assetsBrowse, &QPushButton::clicked, this, &SetupDialog::onBrowseAssets);
    assetsRow->addWidget(m_assetsPath, 1);
    assetsRow->addWidget(assetsBrowse);
    assetsLayout->addWidget(assetsLabel);
    assetsLayout->addLayout(assetsRow);

    layout->addWidget(assetsGroup);

    // Install location
    auto* installGroup = new QGroupBox("Install Location");
    installGroup->setObjectName("setupGroupBox");
    auto* installLayout = new QHBoxLayout(installGroup);
    m_installPath = new QLineEdit;
    QString defaultInstall = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                             + "/ReXGlue/" + info.id;
    m_installPath->setText(defaultInstall);
    auto* installBrowse = new QPushButton("Browse...");
    connect(installBrowse, &QPushButton::clicked, this, [this]() {
        auto dir = QFileDialog::getExistingDirectory(this, "Install Location", m_installPath->text());
        if (!dir.isEmpty()) m_installPath->setText(dir);
    });
    installLayout->addWidget(m_installPath, 1);
    installLayout->addWidget(installBrowse);
    layout->addWidget(installGroup);

    // Progress
    m_progress = new QProgressBar;
    m_progress->setVisible(false);
    m_progress->setRange(0, 100);
    layout->addWidget(m_progress);

    m_statusLabel = new QLabel;
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setVisible(false);
    layout->addWidget(m_statusLabel);

    // Buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto* cancelBtn = new QPushButton("Cancel");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    m_setupBtn = new QPushButton("Download && Set Up");
    m_setupBtn->setObjectName("dialogSetupBtn");
    connect(m_setupBtn, &QPushButton::clicked, this, &SetupDialog::onSetup);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(m_setupBtn);
    layout->addLayout(btnRow);

    // Download signals
    connect(&m_downloader, &DownloadManager::progress, this, &SetupDialog::onDownloadProgress);
    connect(&m_downloader, &DownloadManager::finished, this, &SetupDialog::onDownloadFinished);
    connect(&m_downloader, &DownloadManager::errorOccurred, this, &SetupDialog::onDownloadError);
    connect(&m_downloader, &DownloadManager::releaseTagFetched, this, &SetupDialog::onReleaseTagFetched);
}

void SetupDialog::onBrowseIso() {
    auto file = QFileDialog::getOpenFileName(this, "Select Xbox 360 ISO",
                                              QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                              "ISO Images (*.iso);;All Files (*)");
    if (!file.isEmpty())
        m_isoPath->setText(file);
}

void SetupDialog::onBrowseAssets() {
    auto dir = QFileDialog::getExistingDirectory(this, "Select Game Assets Folder",
                                                  QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    if (!dir.isEmpty())
        m_assetsPath->setText(dir);
}

void SetupDialog::onSetup() {
    if (m_isoPath->text().isEmpty() && m_assetsPath->text().isEmpty()) {
        QMessageBox::warning(this, "Missing Assets",
                             "Please provide either an ISO file or an extracted assets folder.");
        return;
    }

    QString installDir = m_installPath->text();
    QDir().mkpath(installDir);

    m_setupBtn->setEnabled(false);
    m_progress->setVisible(true);
    m_statusLabel->setVisible(true);

    // If ISO provided, extract first
    if (!m_isoPath->text().isEmpty() && m_assetsPath->text().isEmpty()) {
        m_statusLabel->setText("Extracting ISO...");
        m_progress->setRange(0, 0); // indeterminate

        QProcess* proc = new QProcess(this);
        QString assetsDir = installDir + "/assets";
        QDir().mkpath(assetsDir);

        // Use extract-xiso bundled next to this executable, falling back to PATH
        QString extractor;
        QString exeDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
        QString bundled = exeDir + "/extract-xiso.exe";
#else
        QString bundled = exeDir + "/extract-xiso";
#endif
        if (QFile::exists(bundled))
            extractor = bundled;
        else
            extractor = "extract-xiso";

        proc->start(extractor, {"-x", "-d", assetsDir, m_isoPath->text()});
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, assetsDir, installDir](int code, QProcess::ExitStatus) {
            proc->deleteLater();
            if (code != 0) {
                // extract-xiso not available or failed; just note the ISO path
                m_statusLabel->setText("ISO extraction failed, game will use ISO directly");
            }
            m_assetsPath->setText(assetsDir);
            m_progress->setRange(0, 100);

            // Now download the release
            m_statusLabel->setText("Downloading release build...");
            QString packet = m_info.releasePacketForPlatform(currentPlatformName());
            if (packet.isEmpty()) {
                m_statusLabel->setText("No release found for " + currentPlatformName());
                m_setupBtn->setEnabled(true);
                return;
            }
            QString destFile = installDir + "/" + packet;
            m_downloader.downloadRelease(m_info.githubRepo, packet, destFile);
        });
        return;
    }

    // Assets folder provided — go straight to download
    m_statusLabel->setText("Downloading release build...");
    QString packet = m_info.releasePacketForPlatform(currentPlatformName());
    if (packet.isEmpty()) {
        m_statusLabel->setText("No release found for " + currentPlatformName());
        m_setupBtn->setEnabled(true);
        return;
    }
    QString destFile = installDir + "/" + packet;
    m_downloader.downloadRelease(m_info.githubRepo, packet, destFile);
}

void SetupDialog::onDownloadProgress(qint64 received, qint64 total) {
    if (total > 0) {
        m_progress->setRange(0, 100);
        m_progress->setValue(static_cast<int>(received * 100 / total));
        m_statusLabel->setText(QString("Downloading... %1 / %2 MB")
                                   .arg(received / 1048576.0, 0, 'f', 1)
                                   .arg(total / 1048576.0, 0, 'f', 1));
    }
}

void SetupDialog::onDownloadFinished(const QString& filePath) {
    m_statusLabel->setText("Download complete. Setting up...");
    extractAndSetup(filePath);
}

void SetupDialog::onDownloadError(const QString& msg) {
    m_statusLabel->setText("Error: " + msg);
    m_setupBtn->setEnabled(true);
    m_progress->setVisible(false);
}

void SetupDialog::extractAndSetup(const QString& archivePath) {
    QString installDir = m_installPath->text();

    // Extract archive based on type
    if (archivePath.endsWith(".zip")) {
        QProcess* proc = new QProcess(this);
#ifdef Q_OS_WIN
        // PowerShell's Expand-Archive works on all modern Windows (no unzip needed)
        QString psCmd = QString("Expand-Archive -LiteralPath \"%1\" -DestinationPath \"%2\" -Force")
                            .arg(archivePath, installDir);
        proc->start("powershell", {"-NoProfile", "-NonInteractive", "-Command", psCmd});
#else
        proc->start("unzip", {"-o", archivePath, "-d", installDir});
#endif
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, installDir](int code, QProcess::ExitStatus) {
            proc->deleteLater();
            if (code != 0) {
                m_statusLabel->setText("Extraction failed (exit code " + QString::number(code) + ")");
                m_setupBtn->setEnabled(true);
                return;
            }
            m_progress->setValue(100);
            m_statusLabel->setText("Setup complete!");
            QMessageBox::information(this, "Setup Complete",
                                     "Game installed to:\n" + installDir +
                                     "\n\nAssets path:\n" + m_assetsPath->text());
            {
                QJsonObject cfg;
                cfg["assets_path"] = m_assetsPath->text();
                if (!m_installedTag.isEmpty())
                    cfg["installed_version"] = m_installedTag;
                QFile cfgFile(installDir + "/rexglue-setup.json");
                if (cfgFile.open(QIODevice::WriteOnly))
                    cfgFile.write(QJsonDocument(cfg).toJson());
            } // cfgFile closed/flushed before accept()
            accept();
        });
    } else if (archivePath.endsWith(".tar.gz") || archivePath.endsWith(".tgz")) {
        QProcess* proc = new QProcess(this);
        proc->start("tar", {"xzf", archivePath, "-C", installDir});
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, installDir](int code, QProcess::ExitStatus) {
            proc->deleteLater();
            if (code != 0) {
                m_statusLabel->setText("Extraction failed (exit code " + QString::number(code) + ")");
                m_setupBtn->setEnabled(true);
                return;
            }
            m_progress->setValue(100);
            m_statusLabel->setText("Setup complete!");
            QMessageBox::information(this, "Setup Complete",
                                     "Game installed to:\n" + installDir +
                                     "\n\nAssets path:\n" + m_assetsPath->text());
            {
                QJsonObject cfg;
                cfg["assets_path"] = m_assetsPath->text();
                if (!m_installedTag.isEmpty())
                    cfg["installed_version"] = m_installedTag;
                QFile cfgFile(installDir + "/rexglue-setup.json");
                if (cfgFile.open(QIODevice::WriteOnly))
                    cfgFile.write(QJsonDocument(cfg).toJson());
            } // cfgFile closed/flushed before accept()
            accept();
        });
    } else if (archivePath.endsWith(".AppImage", Qt::CaseInsensitive) || archivePath.endsWith(".appimage", Qt::CaseInsensitive)) {
        // Make executable
        QFile::setPermissions(archivePath,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                              QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                              QFileDevice::ReadOther | QFileDevice::ExeOther);
        m_progress->setValue(100);
        m_statusLabel->setText("Setup complete!");
        QMessageBox::information(this, "Setup Complete",
                                 "AppImage ready at:\n" + archivePath +
                                 "\n\nRun it with:\n" + archivePath + " " + m_assetsPath->text());
        {
            QJsonObject cfg;
            cfg["assets_path"] = m_assetsPath->text();
            if (!m_installedTag.isEmpty())
                cfg["installed_version"] = m_installedTag;
            QFile cfgFile(installDir + "/rexglue-setup.json");
            if (cfgFile.open(QIODevice::WriteOnly))
                cfgFile.write(QJsonDocument(cfg).toJson());
        }
        accept();
    } else if (archivePath.endsWith(".apk")) {
        m_progress->setValue(100);
        m_statusLabel->setText("APK downloaded!");
        QMessageBox::information(this, "Setup Complete",
                                 "APK downloaded to:\n" + archivePath +
                                 "\n\nTransfer to your Android device and install.");
        {
            QJsonObject cfg;
            cfg["assets_path"] = m_assetsPath->text();
            if (!m_installedTag.isEmpty())
                cfg["installed_version"] = m_installedTag;
            QFile cfgFile(installDir + "/rexglue-setup.json");
            if (cfgFile.open(QIODevice::WriteOnly))
                cfgFile.write(QJsonDocument(cfg).toJson());
        }
        accept();
    } else {
        m_progress->setValue(100);
        m_statusLabel->setText("Downloaded: " + archivePath);
        {
            QJsonObject cfg;
            cfg["assets_path"] = m_assetsPath->text();
            if (!m_installedTag.isEmpty())
                cfg["installed_version"] = m_installedTag;
            QFile cfgFile(installDir + "/rexglue-setup.json");
            if (cfgFile.open(QIODevice::WriteOnly))
                cfgFile.write(QJsonDocument(cfg).toJson());
        }
        accept();
    }
}

void SetupDialog::onReleaseTagFetched(const QString& tag) {
    m_installedTag = tag;
}
