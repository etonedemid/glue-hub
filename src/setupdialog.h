#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QComboBox>
#include "gameinfo.h"
#include "downloadmanager.h"

class SetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit SetupDialog(const GameInfo& info, QWidget* parent = nullptr);

private slots:
    void onBrowseIso();
    void onBrowseAssets();
    void onSetup();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(const QString& filePath);
    void onDownloadError(const QString& msg);

private:
    void extractAndSetup(const QString& archivePath);

    GameInfo m_info;
    QLineEdit* m_isoPath;
    QLineEdit* m_assetsPath;
    QLineEdit* m_installPath;
    QProgressBar* m_progress;
    QPushButton* m_setupBtn;
    QLabel* m_statusLabel;
    DownloadManager m_downloader;
};
