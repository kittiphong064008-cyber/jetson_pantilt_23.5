#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTimer>
#include <QMap>
#include <QElapsedTimer>
#include <atomic>
#include <cstdint>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <deque>

#include "Types.h"
#include "PIDController.h"
#include "TrtYolo.h"
#include "SettingsDialog.h"
#include "TargetTracker.h"
#include "HudRenderer.h"
#include "TargetSelector.h"
#include "GeoEstimator.h"
#include "CommandArbitrator.h"

class V4L2Camera;
class JoystickWidget;
class SerialLink;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &serialPort, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

signals:
    void sensorDataReceived(float heading, float roll, double lat, double lon, float alt, int sat, bool hasGps);
    void modelEngineReady(bool ok, QString errMsg, QString path);

private slots:
    void onJoystickAxis(float x, float y);
    void onSpeedSliderChanged(int value);
    void onPanCameraSelected(int index);
    void onZoomCameraSelected(int index);
    void onCommandTick();
    void onRenderTick();
    void onSensorDataReceived(float heading, float roll, double lat, double lon, float alt, int sat, bool hasGps);
    void onBrowseModel();
    void onToggleAi();
    void onToggleMode();
    void onOpenSettings();
    void onModelEngineReady(bool ok, QString errMsg, QString path);
    void onTargetSelectionChanged(int index);
    void onRelocateBaseClicked();
    void onTogglePredDir();

private:
    struct FpsTracker {
        int frameCount = 0;
        float currentFps = 0.0f;
        qint64 lastTime = 0;
    };

    void setupUi();
    void setupCameras();
    void ensureCameraRunning(int deviceIndex);
    void stopUnusedCameras();
    void syncSpeedSlider();
    PanTiltState computeMotorCommand();
    void startAiWorkers();
    void stopAiWorkers();
    void aiWorkerLoop();
    void applySettings();

    QLabel *m_panView = nullptr;
    QLabel *m_zoomView = nullptr;
    QComboBox *m_panCameraSelect = nullptr;
    QComboBox *m_zoomCameraSelect = nullptr;
    QPushButton *m_modelBrowseBtn = nullptr;
    QLabel *m_modelLabel = nullptr;
    QPushButton *m_aiToggleBtn = nullptr;
    QPushButton *m_predDirBtn = nullptr;
    bool m_predDirEnabled = false;
    QDoubleSpinBox *m_confSpinBox = nullptr;
    QDoubleSpinBox *m_iouSpinBox = nullptr;
    QSpinBox *m_panSkipFrameSpinBox = nullptr;
    QSpinBox *m_zoomSkipFrameSpinBox = nullptr;
    QPushButton *m_settingsBtn = nullptr;

    QComboBox *m_targetSelectCombo = nullptr;
    QPushButton *m_clearLockBtn = nullptr;

    QLabel *m_sensorHeadingLabel = nullptr;
    QLabel *m_sensorRollLabel = nullptr;
    QLabel *m_sensorGpsLabel = nullptr;
    QLabel *m_sensorCoordLabel = nullptr;
    QLabel *m_sensorAltLabel = nullptr;
    QPushButton *m_relocateBaseBtn = nullptr;

    QLabel *m_calcDistLabel = nullptr;
    QLabel *m_calcTargetCoordLabel = nullptr;
    QLabel *m_calcTargetAltLabel = nullptr;
    QLabel *m_calcTargetInfoLabel = nullptr;

    QPushButton *m_modeBtn = nullptr;
    JoystickWidget *m_joystick = nullptr;
    QSlider *m_speedSlider = nullptr;
    QLabel *m_speedValueLabel = nullptr;
    QLabel *m_motorLabel = nullptr;

    QStringList m_cameraDevices;
    QMap<int, V4L2Camera *> m_activeCameras;
    QMap<int, FpsTracker> m_fpsTrackers;
    int m_panCamIdx = -1;
    int m_zoomCamIdx = -1;

    SerialLink *m_serial = nullptr;
    QTimer *m_cmdTimer = nullptr;
    QTimer *m_renderTimer = nullptr;

    ControlMode m_mode = ControlMode::Manual;
    bool m_aiEnabled = false;
    std::atomic<bool> m_modelLoading{false};

    TrtYolo m_sharedYolo;
    SettingsDialog *m_settingsDialog = nullptr;

    std::thread m_aiThread;
    std::atomic<bool> m_aiWorkersRunning{false};

    std::mutex m_workMutex;
    std::condition_variable m_workCv;
    QImage m_pendingPanFrame;
    QImage m_pendingZoomFrame;
    float m_pendingConf = 0.25f;
    float m_pendingIou = 0.45f;
    bool m_hasPendingPan = false;
    bool m_hasPendingZoom = false;
    int m_panFrameCounter = 0;
    int m_zoomFrameCounter = 0;

    PIDController m_panPid{1.0f, 0.02f, 0.35f, 0.4f, 0.04f};
    PIDController m_tiltPid{1.0f, 0.02f, 0.35f, 0.4f, 0.04f};
    QElapsedTimer m_pidTimer;

    TargetTracker m_targetTracker;
    HudRenderer m_hudRenderer;
    TargetSelector m_targetSelector;
    GeoEstimator m_geoEstimator;
    CommandArbitrator m_cmdArbitrator;

    std::vector<Detection> m_lastPanDetections;
    std::vector<Detection> m_lastZoomDetections;
    ActiveTargetCam m_activeTargetSource = ActiveTargetCam::None;
    std::mutex m_aiMutex;
    bool m_panFrameProcessed = false;
    bool m_zoomFrameProcessed = false;

    std::vector<TrackedTarget> m_panTracks;
    std::vector<TrackedTarget> m_zoomTracks;
    TrackedTarget m_activeLockedTarget;
    bool m_hasActiveLockedTarget = false;

    float  m_currentHeading = 0.0f;
    float  m_currentRoll = 0.0f;
    bool   m_hasGps = false;
    std::mutex m_sensorMutex;

    bool m_keyW = false, m_keyA = false;
    bool m_keyS = false, m_keyD = false;
    float m_joyX = 0.0f, m_joyY = 0.0f;
    uint16_t m_speed = 1000;

    float m_panCenterX = 320.0f;
    float m_panCenterY = 240.0f;
    float m_zoomCenterX = 320.0f;
    float m_zoomCenterY = 240.0f;
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

    static constexpr uint16_t SPEED_STEP = 50;
    static constexpr uint16_t MIN_SPEED = 200;
    static constexpr uint16_t MAX_SPEED = 3200;
};
