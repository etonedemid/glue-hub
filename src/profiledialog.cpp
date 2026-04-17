#include "profiledialog.h"
#include "gamerprofile.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QDir>

ProfileDialog::ProfileDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Create Gamer Profile");
    setMinimumSize(500, 420);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(16);

    auto* header = new QLabel("Create Your Profile");
    header->setObjectName("profileDialogHeader");
    layout->addWidget(header);

    // Gamertag
    auto* form = new QFormLayout;
    m_gamertagEdit = new QLineEdit;
    m_gamertagEdit->setPlaceholderText("Enter your gamertag...");
    m_gamertagEdit->setMaxLength(15);
    form->addRow("Gamertag:", m_gamertagEdit);
    layout->addLayout(form);

    // Gamerpic picker
    auto* picLabel = new QLabel("Choose a Gamerpic:");
    layout->addWidget(picLabel);

    auto* picArea = new QHBoxLayout;

    m_picList = new QListWidget;
    m_picList->setViewMode(QListWidget::IconMode);
    m_picList->setIconSize(QSize(64, 64));
    m_picList->setGridSize(QSize(76, 76));
    m_picList->setFlow(QListWidget::LeftToRight);
    m_picList->setWrapping(true);
    m_picList->setResizeMode(QListWidget::Adjust);
    m_picList->setMinimumHeight(160);
    connect(m_picList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (!item) return;
        m_selectedPic = item->data(Qt::UserRole).toString();
        m_picPreview->setPixmap(QPixmap(m_selectedPic).scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });
    picArea->addWidget(m_picList, 1);

    auto* previewLayout = new QVBoxLayout;
    m_picPreview = new QLabel;
    m_picPreview->setFixedSize(96, 96);
    m_picPreview->setAlignment(Qt::AlignCenter);
    m_picPreview->setObjectName("picPreview");
    previewLayout->addWidget(m_picPreview);

    auto* browseBtn = new QPushButton("Browse...");
    connect(browseBtn, &QPushButton::clicked, this, &ProfileDialog::onBrowseGamepic);
    previewLayout->addWidget(browseBtn);
    previewLayout->addStretch();

    picArea->addLayout(previewLayout);
    layout->addLayout(picArea);

    // Buttons
    auto* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    auto* confirmBtn = new QPushButton("Create Profile");
    confirmBtn->setObjectName("dialogSetupBtn");
    connect(confirmBtn, &QPushButton::clicked, this, &ProfileDialog::onConfirm);
    btnLayout->addWidget(confirmBtn);

    layout->addLayout(btnLayout);

    populateGamepics();
}

QString ProfileDialog::gamertag() const {
    return m_gamertagEdit->text().trimmed();
}

QString ProfileDialog::selectedGamepic() const {
    return m_selectedPic;
}

void ProfileDialog::populateGamepics() {
    auto pics = GamerProfileHub::instance().availableGamepics();
    for (const auto& pic : pics) {
        auto* item = new QListWidgetItem;
        QPixmap px(pic);
        if (px.isNull()) continue;
        item->setIcon(QIcon(px.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        item->setData(Qt::UserRole, pic);
        item->setToolTip(QFileInfo(pic).baseName());
        m_picList->addItem(item);
    }
}

void ProfileDialog::onBrowseGamepic() {
    QString file = QFileDialog::getOpenFileName(this, "Select Gamerpic",
                                                 QString(),
                                                 "Images (*.png *.jpg *.webp *.bmp)");
    if (file.isEmpty()) return;

    // Copy to gamerpics dir
    auto& gph = GamerProfileHub::instance();
    QDir().mkpath(gph.gamerpicsDir());
    QString dest = gph.gamerpicsDir() + "/" + QFileInfo(file).fileName();
    if (file != dest)
        QFile::copy(file, dest);

    m_selectedPic = dest;
    m_picPreview->setPixmap(QPixmap(dest).scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // Add to list if not there
    auto* item = new QListWidgetItem;
    item->setIcon(QIcon(QPixmap(dest).scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    item->setData(Qt::UserRole, dest);
    m_picList->addItem(item);
    m_picList->setCurrentItem(item);
}

void ProfileDialog::onConfirm() {
    if (gamertag().isEmpty()) {
        m_gamertagEdit->setFocus();
        return;
    }
    accept();
}
