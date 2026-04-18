#include "profilechooser.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

ProfileChooser::ProfileChooser(const QList<HubProfileEntry>& profiles, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Choose Profile");
    setMinimumSize(400, 320);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* header = new QLabel("Who's playing?");
    header->setObjectName("profileDialogHeader");
    layout->addWidget(header);

    m_list = new QListWidget;
    m_list->setIconSize(QSize(48, 48));
    for (const auto& p : profiles) {
        auto* item = new QListWidgetItem(
            QString("%1  (%2)").arg(p.gamertag,
                                     QStringLiteral("0x") + QString::number(p.xuid, 16).toUpper().rightJustified(16, '0')));
        item->setData(Qt::UserRole, p.xuid);
        m_list->addItem(item);
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &ProfileChooser::onSelect);
    layout->addWidget(m_list, 1);

    m_rememberCheck = new QCheckBox("Remember my choice (don't ask again)");
    layout->addWidget(m_rememberCheck);

    auto* btnLayout = new QHBoxLayout;

    auto* newBtn = new QPushButton("Create New Profile");
    connect(newBtn, &QPushButton::clicked, this, &ProfileChooser::onCreateNew);
    btnLayout->addWidget(newBtn);

    btnLayout->addStretch();

    auto* selectBtn = new QPushButton("Select");
    selectBtn->setObjectName("dialogSetupBtn");
    selectBtn->setDefault(true);
    connect(selectBtn, &QPushButton::clicked, this, &ProfileChooser::onSelect);
    btnLayout->addWidget(selectBtn);

    layout->addLayout(btnLayout);
}

bool ProfileChooser::rememberForever() const {
    return m_rememberCheck->isChecked();
}

void ProfileChooser::onSelect() {
    auto* item = m_list->currentItem();
    if (!item) return;
    m_selectedXuid = item->data(Qt::UserRole).toULongLong();
    accept();
}

void ProfileChooser::onCreateNew() {
    m_selectedXuid = 0;
    m_createNew = true;
    accept();
}
