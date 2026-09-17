#include "HudRenderer.h"
#include <cmath>
#include <algorithm>

void HudRenderer::drawCrosshair(QPainter &painter, float cx, float cy, const QColor &color) {
    painter.setPen(QPen(color, 1, Qt::DashLine));
    painter.drawLine(QPointF(cx - 10, cy), QPointF(cx + 10, cy));
    painter.drawLine(QPointF(cx, cy - 10), QPointF(cx, cy + 10));
}

void HudRenderer::drawDirectionArrow(QPainter &painter, const QRectF &b, int dir8, const QColor &arrowColor) {
    if (dir8 < 0 || dir8 > 7) return;
    float bcx = b.x() + b.width() * 0.5f;
    float bcy = b.y() + b.height() * 0.5f;
    QPointF startPt, endPt;
    float arrowLen = 38.0f;
    switch (dir8) {
    case 0:
        startPt = QPointF(b.right(), bcy);
        endPt = QPointF(b.right() + arrowLen, bcy);
        break;
    case 1:
        startPt = QPointF(b.right(), b.top());
        endPt = QPointF(b.right() + arrowLen * 0.7071f, b.top() - arrowLen * 0.7071f);
        break;
    case 2:
        startPt = QPointF(bcx, b.top());
        endPt = QPointF(bcx, b.top() - arrowLen);
        break;
    case 3:
        startPt = QPointF(b.left(), b.top());
        endPt = QPointF(b.left() - arrowLen * 0.7071f, b.top() - arrowLen * 0.7071f);
        break;
    case 4:
        startPt = QPointF(b.left(), bcy);
        endPt = QPointF(b.left() - arrowLen, bcy);
        break;
    case 5:
        startPt = QPointF(b.left(), b.bottom());
        endPt = QPointF(b.left() - arrowLen * 0.7071f, b.bottom() + arrowLen * 0.7071f);
        break;
    case 6:
        startPt = QPointF(bcx, b.bottom());
        endPt = QPointF(bcx, b.bottom() + arrowLen);
        break;
    case 7:
        startPt = QPointF(b.right(), b.bottom());
        endPt = QPointF(b.right() + arrowLen * 0.7071f, b.bottom() + arrowLen * 0.7071f);
        break;
    default:
        return;
    }

    painter.setPen(QPen(arrowColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(startPt, endPt);

    float dx = endPt.x() - startPt.x();
    float dy = endPt.y() - startPt.y();
    float len = std::sqrt(dx * dx + dy * dy);
    if (len > 1.0f) {
        float ux = dx / len;
        float uy = dy / len;
        float headLen = 11.0f;
        float headAngle = 0.52f;
        float lx = endPt.x() - headLen * (ux * std::cos(headAngle) - uy * std::sin(headAngle));
        float ly = endPt.y() - headLen * (ux * std::sin(headAngle) + uy * std::cos(headAngle));
        float rx = endPt.x() - headLen * (ux * std::cos(-headAngle) - uy * std::sin(-headAngle));
        float ry = endPt.y() - headLen * (ux * std::sin(-headAngle) + uy * std::cos(-headAngle));
        QPolygonF poly;
        poly << endPt << QPointF(lx, ly) << QPointF(rx, ry);
        painter.setBrush(arrowColor);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(poly);
    }
}

void HudRenderer::drawTracks(QPainter &p,
                             const std::vector<TrackedTarget> &tracks,
                             float sx, float sy,
                             bool hasActiveLockedTarget,
                             int activeLockedTrackId,
                             bool predDirEnabled,
                             const QColor &baseColor) {
    for (const auto &tr : tracks) {
        QRectF box(tr.bbox.x() * sx, tr.bbox.y() * sy,
                   tr.bbox.width() * sx, tr.bbox.height() * sy);
        bool isLocked = (hasActiveLockedTarget && tr.trackId == activeLockedTrackId);

        QColor color = isLocked ? QColor(255, 204, 0) : baseColor;
        int penW = isLocked ? 3 : 2;
        p.setPen(QPen(color, penW));
        p.setBrush(QColor(color.red(), color.green(), color.blue(), isLocked ? 50 : 25));
        p.drawRect(box);

        if (isLocked) {
            float cx = box.x() + box.width() * 0.5f;
            float cy = box.y() + box.height() * 0.5f;
            p.setPen(QPen(QColor(255, 204, 0), 1, Qt::DashLine));
            p.drawLine(QPointF(cx - 12, cy), QPointF(cx + 12, cy));
            p.drawLine(QPointF(cx, cy - 12), QPointF(cx, cy + 12));
        }

        if (predDirEnabled) {
            drawDirectionArrow(p, box, tr.direction8, isLocked ? QColor(255, 204, 0) : baseColor);
        }

        QString prefix = isLocked ? QString("[LOCKED #%1]").arg(tr.trackId) : QString("#%1").arg(tr.trackId);
        QString label = QString("%1 %2 %3%").arg(prefix, tr.className).arg(static_cast<int>(tr.conf * 100));
        if (predDirEnabled) {
            label += QString(" | DIR: %1").arg(tr.dirLabel);
        }
        QFont f = p.font(); f.setPointSize(8); f.setBold(true); p.setFont(f);
        QFontMetrics fm(f);
        QRect trRect = fm.boundingRect(label);
        QRect bg(static_cast<int>(box.x()),
                 static_cast<int>(std::max(0.0, box.y() - trRect.height() - 3)),
                 trRect.width() + 6, trRect.height() + 3);
        p.setPen(Qt::NoPen); p.setBrush(QColor(color.red(), color.green(), color.blue(), 210)); p.drawRect(bg);
        p.setPen(QColor(0, 0, 0)); p.drawText(bg, Qt::AlignCenter, label);
    }
}

void HudRenderer::drawFps(QPainter &p, float currentFps) {
    QFont ff = p.font(); ff.setPointSize(9); ff.setBold(true); p.setFont(ff);
    QString fps = QString("FPS: %1").arg(currentFps, 0, 'f', 1);
    QFontMetrics fm(ff); QRect tr = fm.boundingRect(fps);
    QRect bg(6, 6, tr.width() + 10, tr.height() + 6);
    p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 160)); p.drawRect(bg);
    p.setPen(QColor(0, 255, 128)); p.drawText(bg, Qt::AlignCenter, fps);
}
