#pragma once
#include <QPainter>
#include <QRectF>
#include <QColor>
#include <vector>
#include "TargetTracker.h"

class HudRenderer {
public:
    HudRenderer() = default;

    static void drawCrosshair(QPainter &painter, float cx, float cy, const QColor &color);
    static void drawDirectionArrow(QPainter &painter, const QRectF &box, int dir8, const QColor &arrowColor);
    static void drawTracks(QPainter &painter,
                           const std::vector<TrackedTarget> &tracks,
                           float sx, float sy,
                           bool hasActiveLockedTarget,
                           int activeLockedTrackId,
                           bool predDirEnabled,
                           const QColor &baseColor);
    static void drawFps(QPainter &painter, float currentFps);
};
