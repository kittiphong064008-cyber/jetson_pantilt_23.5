#include "TargetSelector.h"
#include <algorithm>

TargetSelector::TargetSelector()
    : m_flowState(FlowchartState::PanDetect),
      m_selectedTrackId(-1),
      m_autoLockedId(-1),
      m_zoomLostCounter(9999)
{
}

void TargetSelector::reset() {
    m_flowState = FlowchartState::PanDetect;
    m_selectedTrackId = -1;
    m_autoLockedId = -1;
    m_zoomLostCounter = 9999;
}

void TargetSelector::clearLock() {
    m_selectedTrackId = -1;
    m_autoLockedId = -1;
}

void TargetSelector::syncCombo(QComboBox *combo, const std::vector<TrackedTarget> &candidates) {
    if (!combo) return;
    std::vector<int> candidateIds;
    for (const auto &c : candidates) candidateIds.push_back(c.trackId);
    std::sort(candidateIds.begin(), candidateIds.end());

    std::vector<int> currentIds;
    for (int i = 1; i < combo->count(); ++i) {
        currentIds.push_back(combo->itemData(i).toInt());
    }
    std::sort(currentIds.begin(), currentIds.end());

    if (candidateIds != currentIds) {
        int curData = combo->currentData().toInt();
        combo->blockSignals(true);
        combo->clear();
        combo->addItem("Auto (First Target)", -1);
        int restoredIndex = 0;
        for (const auto &c : candidates) {
            QString text = QString("Target #%1 (%2)").arg(c.trackId).arg(c.className);
            combo->addItem(text, c.trackId);
            if (c.trackId == curData) {
                restoredIndex = combo->count() - 1;
            }
        }
        combo->setCurrentIndex(restoredIndex);
        combo->blockSignals(false);
        if (restoredIndex == 0) {
            m_selectedTrackId = -1;
        } else {
            m_selectedTrackId = curData;
        }
    }
}

bool TargetSelector::evaluate(const std::vector<TrackedTarget> &panTracks,
                             const std::vector<TrackedTarget> &zoomTracks,
                             ActiveTargetCam &outSource,
                             TrackedTarget &outTarget,
                             std::vector<TrackedTarget> &outCandidates,
                             bool &needClearZoom) {
    needClearZoom = false;
    bool hasLiveZoomTrack = false;
    for (const auto &tr : zoomTracks) {
        if (tr.lostFrames == 0) {
            hasLiveZoomTrack = true;
            break;
        }
    }

    if (hasLiveZoomTrack) {
        m_flowState = FlowchartState::ZoomTrack;
        m_zoomLostCounter = 0;
    } else {
        if (m_flowState == FlowchartState::ZoomTrack) {
            m_flowState = FlowchartState::PanDetect;
            needClearZoom = true;
        }
        m_zoomLostCounter++;
    }

    ActiveTargetCam activeSource = ActiveTargetCam::None;
    TrackedTarget activeTarget;
    bool hasTarget = false;

    std::vector<TrackedTarget> activeCandidates;
    if (m_flowState == FlowchartState::ZoomTrack && hasLiveZoomTrack) {
        activeCandidates = zoomTracks;
        activeSource = ActiveTargetCam::Zoom;
    } else if (!panTracks.empty()) {
        m_flowState = FlowchartState::MoveCenterPan;
        activeCandidates = panTracks;
        activeSource = ActiveTargetCam::Pan;
    }

    if (!activeCandidates.empty()) {
        if (m_selectedTrackId > 0) {
            for (const auto &tr : activeCandidates) {
                if (tr.trackId == m_selectedTrackId) {
                    activeTarget = tr;
                    hasTarget = true;
                    break;
                }
            }
        }

        if (!hasTarget && m_selectedTrackId <= 0) {
            if (m_autoLockedId > 0) {
                for (const auto &tr : activeCandidates) {
                    if (tr.trackId == m_autoLockedId) {
                        activeTarget = tr;
                        hasTarget = true;
                        break;
                    }
                }
            }
            if (!hasTarget) {
                auto it = std::min_element(activeCandidates.begin(), activeCandidates.end(),
                    [](const TrackedTarget &a, const TrackedTarget &b) { return a.trackId < b.trackId; });
                activeTarget = *it;
                hasTarget = true;
                m_autoLockedId = activeTarget.trackId;
            }
        }
    } else {
        m_autoLockedId = -1;
    }

    outSource = activeSource;
    outTarget = activeTarget;
    outCandidates = activeCandidates;
    return hasTarget;
}
