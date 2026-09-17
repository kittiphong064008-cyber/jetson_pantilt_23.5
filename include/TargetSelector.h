#pragma once
#include <vector>
#include <QComboBox>
#include "TargetTracker.h"

enum class ActiveTargetCam { None, Pan, Zoom };
enum class FlowchartState { PanDetect, MoveCenterPan, ZoomTrack };

class TargetSelector {
public:
    TargetSelector();

    void reset();
    void clearLock();

    void setSelectedTrackId(int trackId) { m_selectedTrackId = trackId; }
    int selectedTrackId() const { return m_selectedTrackId; }
    int autoLockedId() const { return m_autoLockedId; }
    FlowchartState flowchartState() const { return m_flowState; }
    int zoomLostCounter() const { return m_zoomLostCounter; }

    void syncCombo(QComboBox *combo, const std::vector<TrackedTarget> &candidates);

    bool evaluate(const std::vector<TrackedTarget> &panTracks,
                  const std::vector<TrackedTarget> &zoomTracks,
                  ActiveTargetCam &outSource,
                  TrackedTarget &outTarget,
                  std::vector<TrackedTarget> &outCandidates,
                  bool &needClearZoom);

private:
    FlowchartState m_flowState = FlowchartState::PanDetect;
    int m_selectedTrackId = -1;
    int m_autoLockedId = -1;
    int m_zoomLostCounter = 9999;
    static constexpr int MAX_ZOOM_LOST = 5;
};
