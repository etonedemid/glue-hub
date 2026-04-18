#pragma once

#include <QDialog>
#include <QListWidget>
#include <QCheckBox>
#include <QPushButton>

#include "gamerprofile.h"

class ProfileChooser : public QDialog {
    Q_OBJECT
public:
    explicit ProfileChooser(const QList<HubProfileEntry>& profiles, QWidget* parent = nullptr);

    quint64 selectedXuid() const { return m_selectedXuid; }
    bool rememberForever() const;

private slots:
    void onSelect();
    void onCreateNew();

private:
    QListWidget* m_list;
    QCheckBox* m_rememberCheck;
    quint64 m_selectedXuid = 0;
    bool m_createNew = false;
};
