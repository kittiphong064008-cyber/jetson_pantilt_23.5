#pragma once
#include <QString>
#include <QRectF>
#include <QPointF>
#include <deque>
#include <vector>
#include "TrtYolo.h"

struct TrackedTarget {
    int trackId = -1;
    int classId = 0;
    QString className;
    QRectF bbox;
    float conf = 0.0f;
    int lostFrames = 0;
    float vx = 0.0f;
    float vy = 0.0f;
    int direction8 = -1;
    QString dirLabel = "HOVER";
    std::deque<QPointF> history;
};

class TargetTracker {
public:
    TargetTracker();

    void updateTracks(std::vector<TrackedTarget> &tracks, const std::vector<Detection> &dets);
    void reset();

private:
    int m_nextTrackId = 1;
    static constexpr int MAX_TRACK_LOST_BUFFER = 8;
};
