#pragma once

#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include "gameinfo.h"

class GameCard : public QWidget {
    Q_OBJECT
public:
    explicit GameCard(const GameInfo& info, QWidget* parent = nullptr);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    GameInfo m_info;
    bool m_hovered = false;
};
