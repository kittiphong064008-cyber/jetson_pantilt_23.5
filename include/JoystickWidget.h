#pragma once
#include <QWidget>
#include <QPointF>

class JoystickWidget : public QWidget {
    Q_OBJECT
public:
    explicit JoystickWidget(QWidget *parent = nullptr);

    float axisX() const { return m_knobPos.x(); }
    float axisY() const { return m_knobPos.y(); }

signals:
    void axisChanged(float x, float y);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void updateKnobFromPixel(const QPointF &widgetPos);

    QPointF m_knobPos{0.0, 0.0};
    bool    m_dragging = false;

    static constexpr float KNOB_RATIO = 0.25f;
    static constexpr float DEAD_ZONE  = 0.10f;
};
