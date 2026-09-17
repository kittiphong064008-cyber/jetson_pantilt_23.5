#pragma once
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QLabel>
#include <QTabWidget>
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <mutex>
#include <cstdint>

struct DroneClassInfo {
    int classId = 0;
    QString className;
    double wReal = 0.35;
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    double panFocalMm() const { return m_panFocalMm; }
    double zoomFocalMm() const { return m_zoomFocalMm; }
    double panSensorWidthMm() const { return m_panSensorWidthMm; }
    double zoomSensorWidthMm() const { return m_zoomSensorWidthMm; }

    double panFocalPx(int imageWidth = 320) const;
    double zoomFocalPx(int imageWidth = 320) const;

    int panCenterX() const { return m_panCenterX; }
    int panCenterY() const { return m_panCenterY; }
    int zoomCenterX() const { return m_zoomCenterX; }
    int zoomCenterY() const { return m_zoomCenterY; }

    double pidKp() const { return m_pidKp; }
    double pidKi() const { return m_pidKi; }
    double pidKd() const { return m_pidKd; }
    double pidDeadband() const { return m_pidDeadband; }
    uint16_t autoMinSpeed() const { return m_autoMinSpeed; }
    uint16_t autoMaxSpeed() const { return m_autoMaxSpeed; }

    int panCapWidth() const { return m_panCapW; }
    int panCapHeight() const { return m_panCapH; }
    int zoomCapWidth() const { return m_zoomCapW; }
    int zoomCapHeight() const { return m_zoomCapH; }

    int panDispWidth() const { return m_panDispW; }
    int panDispHeight() const { return m_panDispH; }
    int zoomDispWidth() const { return m_zoomDispW; }
    int zoomDispHeight() const { return m_zoomDispH; }

    const QVector<DroneClassInfo>& droneClasses() const { return m_droneClasses; }
    double getWRealForClass(int classId, const QString &className = QString()) const;
    bool isClassMapped(int classId) const;

    void loadSettings(const QString &filePath = "settings.json");
    void saveSettings(const QString &filePath = "settings.json");

private slots:
    void onAddRow();
    void onDeleteRow();
    void onSaveClicked();
    void onPanSensorPresetChanged(int index);
    void onZoomSensorPresetChanged(int index);
    void updateCalculatedFocalLabels();
    void onPidSlidersChanged();
    void onCenterSlidersChanged();
    void onResetCenterClicked();
    void onResetPidClicked();
    void onResetResolutionsClicked();
    void onPanCapPresetChanged(int index);
    void onZoomCapPresetChanged(int index);
    void onPanDispPresetChanged(int index);
    void onZoomDispPresetChanged(int index);

private:
    void setupUi();
    void populateTable();

    QTabWidget *m_tabs = nullptr;

    QTableWidget *m_table = nullptr;
    QDoubleSpinBox *m_panFocalMmSpin = nullptr;
    QComboBox *m_panSensorCombo = nullptr;
    QDoubleSpinBox *m_panSensorWidthSpin = nullptr;
    QLabel *m_panCalculatedLabel = nullptr;

    QDoubleSpinBox *m_zoomFocalMmSpin = nullptr;
    QComboBox *m_zoomSensorCombo = nullptr;
    QDoubleSpinBox *m_zoomSensorWidthSpin = nullptr;
    QLabel *m_zoomCalculatedLabel = nullptr;

    QPushButton *m_addBtn = nullptr;
    QPushButton *m_delBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;

    QSlider *m_panCenterXSlider = nullptr;
    QSpinBox *m_panCenterXSpin = nullptr;
    QSlider *m_panCenterYSlider = nullptr;
    QSpinBox *m_panCenterYSpin = nullptr;
    QLabel *m_panCenterValLabel = nullptr;

    QSlider *m_zoomCenterXSlider = nullptr;
    QSpinBox *m_zoomCenterXSpin = nullptr;
    QSlider *m_zoomCenterYSlider = nullptr;
    QSpinBox *m_zoomCenterYSpin = nullptr;
    QLabel *m_zoomCenterValLabel = nullptr;
    QPushButton *m_resetCenterBtn = nullptr;

    QSlider *m_kpSlider = nullptr;
    QLabel *m_kpValLabel = nullptr;
    QSlider *m_kiSlider = nullptr;
    QLabel *m_kiValLabel = nullptr;
    QSlider *m_kdSlider = nullptr;
    QLabel *m_kdValLabel = nullptr;
    QSlider *m_deadbandSlider = nullptr;
    QLabel *m_deadbandValLabel = nullptr;
    QSlider *m_autoMinSpeedSlider = nullptr;
    QLabel *m_autoMinSpeedValLabel = nullptr;
    QSlider *m_autoMaxSpeedSlider = nullptr;
    QLabel *m_autoMaxSpeedValLabel = nullptr;
    QPushButton *m_resetPidBtn = nullptr;

    QSpinBox *m_panCapWSpin = nullptr;
    QSpinBox *m_panCapHSpin = nullptr;
    QComboBox *m_panCapPresetCombo = nullptr;

    QSpinBox *m_zoomCapWSpin = nullptr;
    QSpinBox *m_zoomCapHSpin = nullptr;
    QComboBox *m_zoomCapPresetCombo = nullptr;

    QSpinBox *m_panDispWSpin = nullptr;
    QSpinBox *m_panDispHSpin = nullptr;
    QComboBox *m_panDispPresetCombo = nullptr;

    QSpinBox *m_zoomDispWSpin = nullptr;
    QSpinBox *m_zoomDispHSpin = nullptr;
    QComboBox *m_zoomDispPresetCombo = nullptr;

    QPushButton *m_resetResolutionsBtn = nullptr;

    double m_panFocalMm = 5.0;
    double m_panSensorWidthMm = 5.76;
    double m_zoomFocalMm = 50.0;
    double m_zoomSensorWidthMm = 5.76;

    int m_panCenterX = 320;
    int m_panCenterY = 240;
    int m_zoomCenterX = 320;
    int m_zoomCenterY = 240;

    double m_pidKp = 1.0;
    double m_pidKi = 0.02;
    double m_pidKd = 0.35;
    double m_pidDeadband = 0.04;
    uint16_t m_autoMinSpeed = 500;
    uint16_t m_autoMaxSpeed = 3200;

    int m_panCapW = 640;
    int m_panCapH = 480;
    int m_zoomCapW = 640;
    int m_zoomCapH = 480;

    int m_panDispW = 320;
    int m_panDispH = 240;
    int m_zoomDispW = 320;
    int m_zoomDispH = 240;

    QVector<DroneClassInfo> m_droneClasses;
    QString m_settingsFilePath = "settings.json";
    mutable std::mutex m_classMutex;
};
