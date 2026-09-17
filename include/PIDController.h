#pragma once
#include <algorithm>
#include <cmath>

class PIDController {
public:
    PIDController(float kp = 1.0f, float ki = 0.02f, float kd = 0.35f,
                  float integralLimit = 0.4f, float deadband = 0.04f)
        : m_kp(kp), m_ki(ki), m_kd(kd),
          m_integralLimit(integralLimit), m_deadband(deadband) {}

    void setGains(float kp, float ki, float kd, float deadband = 0.04f) {
        m_kp = kp;
        m_ki = ki;
        m_kd = kd;
        m_deadband = deadband;
    }

    void reset() {
        m_integral = 0.0f;
        m_prevError = 0.0f;
        m_filteredDerivative = 0.0f;
        m_firstRun = true;
    }

    float update(float error, float dt) {
        if (std::fabs(error) < m_deadband) {
            m_prevError = error;
            m_filteredDerivative = 0.0f;
            m_integral = 0.0f;
            return 0.0f;
        }

        if (m_firstRun || dt <= 0.0f) {
            m_prevError = error;
            m_firstRun = false;
            return m_kp * error;
        }

        float pTerm = m_kp * error;

        m_integral += error * dt;
        m_integral = std::max(-m_integralLimit, std::min(m_integral, m_integralLimit));
        float iTerm = m_ki * m_integral;

        float rawDerivative = (error - m_prevError) / dt;
        float alpha = 0.7f;
        m_filteredDerivative = alpha * m_filteredDerivative + (1.0f - alpha) * rawDerivative;
        float dTerm = m_kd * m_filteredDerivative;

        m_prevError = error;

        float output = pTerm + iTerm + dTerm;
        return std::max(-1.0f, std::min(output, 1.0f));
    }

private:
    float m_kp;
    float m_ki;
    float m_kd;
    float m_integralLimit;
    float m_deadband;

    float m_integral = 0.0f;
    float m_prevError = 0.0f;
    float m_filteredDerivative = 0.0f;
    bool  m_firstRun = true;
};
