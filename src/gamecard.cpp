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

    // Boxart — fill entire card, title overlaid at bottom
    if (!m_info.boxart.isNull()) {
        QPixmap scaled = m_info.boxart.scaled(width(), height(),
                                               Qt::KeepAspectRatioByExpanding,
                                               Qt::SmoothTransformation);
        int sx = (scaled.width() - width()) / 2;
        int sy = (scaled.height() - height()) / 2;
        p.drawPixmap(0, 0, scaled, sx, sy, width(), height());
    } else {
        p.fillRect(r, QColor("#0d0d1a"));
        p.setPen(QColor("#555577"));
        p.drawText(r, Qt::AlignCenter, "No Cover");
    }

    // Gradient overlay at bottom for title readability
    QLinearGradient grad(0, height() - 80, 0, height());
    grad.setColorAt(0.0, QColor(0, 0, 0, 0));
    grad.setColorAt(1.0, QColor(0, 0, 0, 200));
    p.fillRect(QRectF(0, height() - 80, width(), 80), grad);

    // Title text overlaid at bottom
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPixelSize(13);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(10, height() - 54, width() - 20, 48),
               Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap,
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
