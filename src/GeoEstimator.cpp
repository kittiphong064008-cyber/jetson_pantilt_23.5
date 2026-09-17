#include "GeoEstimator.h"
#include <cmath>
#include <algorithm>

GeoEstimator::GeoEstimator()
    : m_gpsSampleCount(0),
      m_filteredLat(0.0),
      m_filteredLon(0.0),
      m_filteredAlt(0.0f)
{
}

void GeoEstimator::resetBaseGps() {
    m_gpsSampleCount = 0;
    m_filteredLat = 0.0;
    m_filteredLon = 0.0;
    m_filteredAlt = 0.0f;
}

void GeoEstimator::updateBaseGps(double lat, double lon, float alt, bool hasGps) {
    if (hasGps && lat != 0.0 && lon != 0.0) {
        m_gpsSampleCount++;
        m_filteredLat += (lat - m_filteredLat) / static_cast<double>(m_gpsSampleCount);
        m_filteredLon += (lon - m_filteredLon) / static_cast<double>(m_gpsSampleCount);
        m_filteredAlt += (alt - m_filteredAlt) / static_cast<float>(m_gpsSampleCount);
    }
}

double GeoEstimator::calculateDistance(double wReal, double focalPx, double bboxWidth) {
    double wPixel = std::max(1.0, bboxWidth);
    return (wReal * focalPx) / wPixel;
}

GeoTargetResult GeoEstimator::estimateTargetGeo(double distance,
                                                double baseLat, double baseLon, float baseAlt,
                                                float headingDeg, float rollDeg) {
    GeoTargetResult res;
    res.distance = distance;

    if (baseLat == 0.0 && baseLon == 0.0) {
        res.hasValidGeo = false;
        return res;
    }

    const double PI = 3.14159265358979323846;
    double psi = headingDeg * (PI / 180.0);
    double theta = rollDeg * (PI / 180.0);
    double X = distance * std::cos(theta) * std::sin(psi);
    double Y = distance * std::cos(theta) * std::cos(psi);
    double Z = distance * std::sin(theta);

    const double R_EARTH = 6371000.0;
    double targetLat = baseLat + (Y / R_EARTH) * (180.0 / PI);
    double latRad = baseLat * (PI / 180.0);
    double cosLat = std::cos(latRad);
    if (std::abs(cosLat) < 1e-6) cosLat = 1e-6;
    double targetLon = baseLon + (X / (R_EARTH * cosLat)) * (180.0 / PI);
    double targetAlt = baseAlt + Z;

    res.targetLat = targetLat;
    res.targetLon = targetLon;
    res.targetAlt = targetAlt;
    res.dZ = Z;
    res.hasValidGeo = true;
    return res;
}
