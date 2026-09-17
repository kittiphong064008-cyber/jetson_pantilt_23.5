#include "CommandArbitrator.h"

void CommandArbitrator::computeCentroidError(const TrackedTarget &target,
                                            ActiveTargetCam source,
                                            float panCenterX, float panCenterY,
                                            float zoomCenterX, float zoomCenterY,
                                            int panCapW, int panCapH,
                                            int zoomCapW, int zoomCapH,
                                            float &outErrX, float &outErrY) {
    float cx = target.bbox.x() + target.bbox.width() * 0.5f;
    float cy = target.bbox.y() + target.bbox.height() * 0.5f;
    float targetCenterX = (source == ActiveTargetCam::Zoom) ? zoomCenterX : panCenterX;
    float targetCenterY = (source == ActiveTargetCam::Zoom) ? zoomCenterY : panCenterY;
    float capHalfW = (source == ActiveTargetCam::Zoom) ? (zoomCapW * 0.5f) : (panCapW * 0.5f);
    float capHalfH = (source == ActiveTargetCam::Zoom) ? (zoomCapH * 0.5f) : (panCapH * 0.5f);
    if (capHalfW <= 1.0f) capHalfW = 320.0f;
    if (capHalfH <= 1.0f) capHalfH = 240.0f;
    outErrX = (cx - targetCenterX) / capHalfW;
    outErrY = (cy - targetCenterY) / capHalfH;
}

PanTiltState CommandArbitrator::computeAutoCommand(float errX, float errY,
                                                  float dt,
                                                  PIDController &panPid,
                                                  PIDController &tiltPid,
                                                  uint16_t autoMinSpeed,
                                                  uint16_t autoMaxSpeed) {
    PanTiltState st = PanTiltState::Stop();
    float panOut = panPid.update(errX, dt);
    float tiltOut = tiltPid.update(errY, dt);
    if (std::fabs(panOut) > 0.02f) {
        st.pan.en = 1; st.pan.dir = (panOut > 0) ? 1 : 0;
        float pMag = std::min(1.0f, std::fabs(panOut) * 1.5f);
        st.pan.speed = static_cast<uint16_t>(autoMaxSpeed - pMag * (autoMaxSpeed - autoMinSpeed));
    }
    if (std::fabs(tiltOut) > 0.02f) {
        st.tilt.en = 1; st.tilt.dir = (tiltOut < 0) ? 1 : 0;
        float tMag = std::min(1.0f, std::fabs(tiltOut) * 1.5f);
        st.tilt.speed = static_cast<uint16_t>(autoMaxSpeed - tMag * (autoMaxSpeed - autoMinSpeed));
    }
    return st;
}

PanTiltState CommandArbitrator::computeManualCommand(const ManualInputState &in) {
    PanTiltState st = PanTiltState::Stop();
    const float dz = 0.10f;
    if (std::fabs(in.joyX) > dz || std::fabs(in.joyY) > dz) {
        if (std::fabs(in.joyX) > dz) { st.pan.en = 1; st.pan.dir = (in.joyX > 0) ? 1 : 0; st.pan.speed = in.speed; }
        if (std::fabs(in.joyY) > dz) { st.tilt.en = 1; st.tilt.dir = (in.joyY < 0) ? 1 : 0; st.tilt.speed = in.speed; }
    } else {
        if (in.keyA && !in.keyD) { st.pan.en = 1; st.pan.dir = 0; st.pan.speed = in.speed; }
        else if (in.keyD && !in.keyA) { st.pan.en = 1; st.pan.dir = 1; st.pan.speed = in.speed; }
        if (in.keyW && !in.keyS) { st.tilt.en = 1; st.tilt.dir = 1; st.tilt.speed = in.speed; }
        else if (in.keyS && !in.keyW) { st.tilt.en = 1; st.tilt.dir = 0; st.tilt.speed = in.speed; }
    }
    return st;
}
