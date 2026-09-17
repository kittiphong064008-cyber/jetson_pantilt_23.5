#include "JoystickWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <cmath>

JoystickWidget::JoystickWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(180, 180);
    setFixedSize(200, 200);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void JoystickWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const float w   = static_cast<float>(width());
    const float h   = static_cast<float>(height());
    const float cx  = w * 0.5f;
    const float cy  = h * 0.5f;
    const float outerR = std::min(cx, cy) - 2.0f;
    const float knobR  = outerR * KNOB_RATIO;

    p.setPen(QPen(QColor(100, 100, 100), 2));
    p.setBrush(QColor(40, 40, 40));
    p.drawEllipse(QPointF(cx, cy), outerR, outerR);

    p.setPen(QPen(QColor(70, 70, 70), 1, Qt::DashLine));
    p.drawLine(QPointF(cx - outerR, cy), QPointF(cx + outerR, cy));
    p.drawLine(QPointF(cx, cy - outerR), QPointF(cx, cy + outerR));

    const float dzR = outerR * DEAD_ZONE;
    p.setPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), dzR, dzR);

    const float knobCx = cx + m_knobPos.x() * (outerR - knobR);
    const float knobCy = cy + m_knobPos.y() * (outerR - knobR);

    QRadialGradient grad(QPointF(knobCx, knobCy), knobR);
    grad.setColorAt(0.0, QColor(0, 160, 255));
    grad.setColorAt(1.0, QColor(0, 100, 200));
    p.setPen(QPen(QColor(0, 200, 255), 1.5));
    p.setBrush(grad);
    p.drawEllipse(QPointF(knobCx, knobCy), knobR, knobR);
}

void JoystickWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        updateKnobFromPixel(event->pos());
    }
}

void JoystickWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging)
        updateKnobFromPixel(event->pos());
}

void JoystickWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        m_knobPos = {0.0, 0.0};
        update();
        emit axisChanged(0.0f, 0.0f);
    }
}

void JoystickWidget::updateKnobFromPixel(const QPointF &widgetPos) {
    const float cx = width()  * 0.5f;
    const float cy = height() * 0.5f;
    const float outerR = std::min(cx, cy) - 2.0f;
    const float knobR  = outerR * KNOB_RATIO;
    const float travel  = outerR - knobR;

    float dx = static_cast<float>(widgetPos.x()) - cx;
    float dy = static_cast<float>(widgetPos.y()) - cy;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist > travel) {
        dx *= travel / dist;
        dy *= travel / dist;
    }

    float nx = dx / travel;
    float ny = dy / travel;

    float nmag = std::sqrt(nx * nx + ny * ny);
    if (nmag < DEAD_ZONE) {
        nx = 0.0f;
        ny = 0.0f;
    }

    m_knobPos = {nx, ny};
    update();
    emit axisChanged(nx, ny);
}
