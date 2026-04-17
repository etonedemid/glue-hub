#include "gamecard.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

GameCard::GameCard(const GameInfo& info, QWidget* parent)
    : QWidget(parent), m_info(info)
{
    setFixedSize(180, 260);
    setCursor(Qt::PointingHandCursor);
    setToolTip(info.name);
}

void GameCard::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    QRectF r(0, 0, width(), height());
    qreal radius = 12.0;

    // Clipping path for rounded corners
    QPainterPath clip;
    clip.addRoundedRect(r, radius, radius);
    p.setClipPath(clip);

    // Background
    p.fillRect(r, QColor("#16213e"));

    // Boxart
    if (!m_info.boxart.isNull()) {
        QPixmap scaled = m_info.boxart.scaled(width(), 200,
                                               Qt::KeepAspectRatioByExpanding,
                                               Qt::SmoothTransformation);
        int sx = (scaled.width() - width()) / 2;
        int sy = (scaled.height() - 200) / 2;
        p.drawPixmap(0, 0, scaled, sx, sy, width(), 200);
    } else {
        p.fillRect(QRectF(0, 0, width(), 200), QColor("#0d0d1a"));
        p.setPen(QColor("#555577"));
        p.drawText(QRectF(0, 0, width(), 200), Qt::AlignCenter, "No Cover");
    }

    // Title strip
    p.fillRect(QRectF(0, 200, width(), 60), QColor(22, 33, 62, 240));
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPixelSize(13);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(10, 206, width() - 20, 48),
               Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               m_info.name);

    // Hover glow
    if (m_hovered) {
        p.setClipPath(clip);
        QPen pen(QColor("#e94560"), 2);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(1, 1, -1, -1), radius, radius);
    }
}

void GameCard::enterEvent(QEnterEvent*) {
    m_hovered = true;
    update();
}

void GameCard::leaveEvent(QEvent*) {
    m_hovered = false;
    update();
}

void GameCard::mousePressEvent(QMouseEvent*) {
    emit clicked();
}
