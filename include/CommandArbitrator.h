#pragma once
#include <cmath>
#include <algorithm>
#include "Types.h"
#include "PIDController.h"
#include "TargetTracker.h"
#include "TargetSelector.h"

enum class ControlMode { Manual, Auto };

struct ManualInputState {
    float joyX = 0.0f;
    float joyY = 0.0f;
    bool keyW = false;
    bool keyA = false;
    bool keyS = false;
    bool keyD = false;
    uint16_t speed = 1000;
};

class CommandArbitrator {
public:
    CommandArbitrator() = default;

    static void computeCentroidError(const TrackedTarget &target,
                                    ActiveTargetCam source,
                                    float panCenterX, float panCenterY,
                                    float zoomCenterX, float zoomCenterY,
                                    int panCapW, int panCapH,
                                    int zoomCapW, int zoomCapH,
                                    float &outErrX, float &outErrY);

    static PanTiltState computeAutoCommand(float errX, float errY,
                                          float dt,
                                          PIDController &panPid,
                                          PIDController &tiltPid,
                                          uint16_t autoMinSpeed,
                                          uint16_t autoMaxSpeed);

    static PanTiltState computeManualCommand(const ManualInputState &input);
};
