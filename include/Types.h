#pragma once
#include <cstdint>

struct AxisState {
    uint8_t  en    = 0;
    uint8_t  dir   = 0;
    uint16_t speed = 0;

    bool operator==(const AxisState &o) const {
        return en == o.en && dir == o.dir && speed == o.speed;
    }
    bool operator!=(const AxisState &o) const { return !(*this == o); }

    static AxisState Stop() { return AxisState{}; }
};

struct PanTiltState {
    AxisState pan;
    AxisState tilt;

    bool operator==(const PanTiltState &o) const {
        return pan == o.pan && tilt == o.tilt;
    }
    bool operator!=(const PanTiltState &o) const { return !(*this == o); }

    static PanTiltState Stop() { return PanTiltState{}; }
};

struct SensorData {
    float    heading    = 0.0f;
    float    roll       = 0.0f;
    double   lat        = 0.0;
    double   lon        = 0.0;
    float    alt        = 0.0f;
    uint8_t  satellites = 0;
    uint8_t  gpsValid   = 0;

    bool HasLocation() const { return (gpsValid & 0x01) != 0; }
    bool HasAltitude() const { return (gpsValid & 0x02) != 0; }
};
