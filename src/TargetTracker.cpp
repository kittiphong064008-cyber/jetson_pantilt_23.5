#include "TargetTracker.h"
#include <cmath>
#include <algorithm>

TargetTracker::TargetTracker()
    : m_nextTrackId(1)
{
}

void TargetTracker::reset() {
    m_nextTrackId = 1;
}

void TargetTracker::updateTracks(std::vector<TrackedTarget> &tracks, const std::vector<Detection> &dets) {
    std::vector<bool> matchedDets(dets.size(), false);

    for (size_t t = 0; t < tracks.size(); ++t) {
        float bestIoU = 0.0f;
        int bestDetIdx = -1;
        float bestDist = 999999.0f;

        float tcX = tracks[t].bbox.x() + tracks[t].bbox.width() * 0.5f;
        float tcY = tracks[t].bbox.y() + tracks[t].bbox.height() * 0.5f;

        for (size_t d = 0; d < dets.size(); ++d) {
            if (matchedDets[d]) continue;
            if (tracks[t].classId != dets[d].classId) continue;

            QRectF inter = tracks[t].bbox.intersected(dets[d].bbox);
            float interArea = inter.isEmpty() ? 0.0f : inter.width() * inter.height();
            float unionArea = tracks[t].bbox.width() * tracks[t].bbox.height() +
                              dets[d].bbox.width() * dets[d].bbox.height() - interArea;
            float iou = (unionArea > 0.0f) ? (interArea / unionArea) : 0.0f;

            float dcX = dets[d].bbox.x() + dets[d].bbox.width() * 0.5f;
            float dcY = dets[d].bbox.y() + dets[d].bbox.height() * 0.5f;
            float dist = std::sqrt((tcX - dcX)*(tcX - dcX) + (tcY - dcY)*(tcY - dcY));

            if (iou > 0.2f && iou > bestIoU) {
                bestIoU = iou;
                bestDetIdx = static_cast<int>(d);
            } else if (bestIoU <= 0.0f && dist < 100.0f && dist < bestDist) {
                bestDist = dist;
                bestDetIdx = static_cast<int>(d);
            }
        }

        if (bestDetIdx >= 0) {
            float oldCx = tracks[t].bbox.x() + tracks[t].bbox.width() * 0.5f;
            float oldCy = tracks[t].bbox.y() + tracks[t].bbox.height() * 0.5f;
            float newCx = dets[bestDetIdx].bbox.x() + dets[bestDetIdx].bbox.width() * 0.5f;
            float newCy = dets[bestDetIdx].bbox.y() + dets[bestDetIdx].bbox.height() * 0.5f;
            float dx = newCx - oldCx;
            float dy = newCy - oldCy;

            tracks[t].history.push_back(QPointF(newCx, newCy));
            if (tracks[t].history.size() > 10) tracks[t].history.pop_front();

            tracks[t].vx = 0.65f * tracks[t].vx + 0.35f * dx;
            tracks[t].vy = 0.65f * tracks[t].vy + 0.35f * dy;

            float speed = std::sqrt(tracks[t].vx * tracks[t].vx + tracks[t].vy * tracks[t].vy);
            if (speed < 2.5f) {
                tracks[t].direction8 = -1;
                tracks[t].dirLabel = "HOVER";
            } else {
                float angle = std::atan2(-tracks[t].vy, tracks[t].vx) * 180.0f / 3.14159265358979323846f;
                if (angle < 0.0f) angle += 360.0f;
                int sec = static_cast<int>((angle + 22.5f) / 45.0f) % 8;
                tracks[t].direction8 = sec;
                static const char *kSecNames[8] = {
                    "E", "NE", "N", "NW", "W", "SW", "S", "SE"
                };
                tracks[t].dirLabel = QString::fromUtf8(kSecNames[sec]);
            }

            tracks[t].bbox = dets[bestDetIdx].bbox;
            tracks[t].conf = dets[bestDetIdx].conf;
            tracks[t].className = dets[bestDetIdx].className;
            tracks[t].lostFrames = 0;
            matchedDets[bestDetIdx] = true;
        } else {
            tracks[t].lostFrames++;
            tracks[t].vx *= 0.5f;
            tracks[t].vy *= 0.5f;
        }
    }

    for (size_t d = 0; d < dets.size(); ++d) {
        if (!matchedDets[d]) {
            TrackedTarget nt;
            nt.trackId = m_nextTrackId++;
            if (m_nextTrackId > 9999) m_nextTrackId = 1;
            nt.classId = dets[d].classId;
            nt.className = dets[d].className;
            nt.bbox = dets[d].bbox;
            nt.conf = dets[d].conf;
            nt.lostFrames = 0;
            nt.vx = 0.0f;
            nt.vy = 0.0f;
            nt.direction8 = -1;
            nt.dirLabel = "HOVER";
            float cx = dets[d].bbox.x() + dets[d].bbox.width() * 0.5f;
            float cy = dets[d].bbox.y() + dets[d].bbox.height() * 0.5f;
            nt.history.push_back(QPointF(cx, cy));
            tracks.push_back(nt);
        }
    }

    tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [](const TrackedTarget &tr) {
        return tr.lostFrames > MAX_TRACK_LOST_BUFFER;
    }), tracks.end());
}
