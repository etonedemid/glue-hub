#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>

class ProfileDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProfileDialog(QWidget* parent = nullptr);

    QString gamertag() const;
    QString selectedGamepic() const;

private slots:
    void onBrowseGamepic();
    void onConfirm();

private:
    void populateGamepics();

    QLineEdit* m_gamertagEdit;
    QListWidget* m_picList;
    QLabel* m_picPreview;
    QString m_selectedPic;
};
