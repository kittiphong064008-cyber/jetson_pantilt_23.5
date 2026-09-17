#pragma once
#include <cstdint>

struct GeoTargetResult {
    double distance = 0.0;
    double targetLat = 0.0;
    double targetLon = 0.0;
    double targetAlt = 0.0;
    double dZ = 0.0;
    bool hasValidGeo = false;
};

class GeoEstimator {
public:
    GeoEstimator();

    void resetBaseGps();
    void updateBaseGps(double lat, double lon, float alt, bool hasGps);

    double baseLat() const { return m_filteredLat; }
    double baseLon() const { return m_filteredLon; }
    float  baseAlt() const { return m_filteredAlt; }
    uint64_t sampleCount() const { return m_gpsSampleCount; }

    static double calculateDistance(double wReal, double focalPx, double bboxWidth);

    static GeoTargetResult estimateTargetGeo(double distance,
                                            double baseLat, double baseLon, float baseAlt,
                                            float headingDeg, float rollDeg);

private:
    uint64_t m_gpsSampleCount = 0;
    double m_filteredLat = 0.0;
    double m_filteredLon = 0.0;
    float  m_filteredAlt = 0.0f;
};
