#include "MainWindow.h"
#include "JoystickWidget.h"
#include "SerialLink.h"
#include "V4L2Camera.h"

#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

MainWindow::MainWindow(const QString &serialPort, QWidget *parent)
    : QMainWindow(parent)
{
    m_settingsDialog = new SettingsDialog(this);
    setupUi();
    applySettings();

    connect(this, &MainWindow::sensorDataReceived,
            this, &MainWindow::onSensorDataReceived);
    connect(this, &MainWindow::modelEngineReady,
            this, &MainWindow::onModelEngineReady);

    m_serial = new SerialLink(serialPort.toStdString(), 150, 1000);
    m_serial->SetSensorCallback([this](const SensorData &d) {
        emit sensorDataReceived(d.heading, d.roll, d.lat, d.lon, d.alt,
        static_cast<int>(d.satellites), d.HasLocation());
    });
    m_serial->Start();

    startAiWorkers();
    setupCameras();

    m_pidTimer.start();
    m_cmdTimer = new QTimer(this);
    connect(m_cmdTimer, &QTimer::timeout, this, &MainWindow::onCommandTick);
    m_cmdTimer->start(20);

    m_renderTimer = new QTimer(this);
    connect(m_renderTimer, &QTimer::timeout, this, &MainWindow::onRenderTick);
    m_renderTimer->start(20);

    connect(m_joystick, &JoystickWidget::axisChanged, this, &MainWindow::onJoystickAxis);
    connect(m_speedSlider, &QSlider::valueChanged, this, &MainWindow::onSpeedSliderChanged);
    connect(m_panCameraSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPanCameraSelected);
    connect(m_zoomCameraSelect, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onZoomCameraSelected);
    connect(m_modelBrowseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseModel);
    connect(m_aiToggleBtn, &QPushButton::clicked, this, &MainWindow::onToggleAi);
    connect(m_predDirBtn, &QPushButton::clicked, this, &MainWindow::onTogglePredDir);
    connect(m_modeBtn, &QPushButton::clicked, this, &MainWindow::onToggleMode);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onOpenSettings);

    connect(m_targetSelectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onTargetSelectionChanged);
    connect(m_clearLockBtn, &QPushButton::clicked, this, [this]() {
        if (m_targetSelectCombo) m_targetSelectCombo->setCurrentIndex(0);
        m_targetSelector.clearLock();
    });

    setWindowTitle("Jetson Pan-Tilt Controller (v23.5)");
    resize(1380, 890);
    setFocusPolicy(Qt::StrongFocus);
}
MainWindow::~MainWindow() {
    m_cmdTimer->stop();
    m_renderTimer->stop();
    stopAiWorkers();
    for (auto *cam : m_activeCameras) { cam->stopCapture(); cam->wait(); }
    m_serial->Stop();
    delete m_serial;
}

void MainWindow::startAiWorkers() {
    m_aiWorkersRunning = true;
    m_aiThread = std::thread(&MainWindow::aiWorkerLoop, this);
}

void MainWindow::stopAiWorkers() {
    m_aiWorkersRunning = false;
    {
        std::lock_guard<std::mutex> lock(m_workMutex);
        m_hasPendingPan = true;
        m_hasPendingZoom = true;
    }
    m_workCv.notify_all();
    if (m_aiThread.joinable()) m_aiThread.join();
}

void MainWindow::aiWorkerLoop() {
    bool preferPan = true;
    while (m_aiWorkersRunning) {
        QImage frame;
        float conf = 0.25f, iou = 0.45f;
        bool isPanFrame = false;
        bool gotFrame = false;
        {
            std::unique_lock<std::mutex> lock(m_workMutex);
            m_workCv.wait(lock, [this] {
                return !m_aiWorkersRunning || m_hasPendingPan || m_hasPendingZoom;
            });
            if (!m_aiWorkersRunning) break;

            if (preferPan && m_hasPendingPan) {
                frame = std::move(m_pendingPanFrame);
                m_hasPendingPan = false;
                isPanFrame = true;
                gotFrame = true;
            } else if (!preferPan && m_hasPendingZoom) {
                frame = std::move(m_pendingZoomFrame);
                m_hasPendingZoom = false;
                isPanFrame = false;
                gotFrame = true;
            } else if (m_hasPendingPan) {
                frame = std::move(m_pendingPanFrame);
                m_hasPendingPan = false;
                isPanFrame = true;
                gotFrame = true;
            } else if (m_hasPendingZoom) {
                frame = std::move(m_pendingZoomFrame);
                m_hasPendingZoom = false;
                isPanFrame = false;
                gotFrame = true;
            }
            preferPan = !preferPan;
            conf = m_pendingConf;
            iou = m_pendingIou;
        }

        if (gotFrame && m_aiEnabled && m_sharedYolo.isLoaded() && !frame.isNull()) {
            auto rawDets = m_sharedYolo.detect(frame, conf, iou);
            std::vector<Detection> dets;
            for (const auto &d : rawDets) {
                if (m_settingsDialog && m_settingsDialog->isClassMapped(d.classId)) {
                    dets.push_back(d);
                }
            }
            std::lock_guard<std::mutex> lock(m_aiMutex);
            if (isPanFrame) {
                m_lastPanDetections = std::move(dets);
                m_panFrameProcessed = true;
            } else {
                m_lastZoomDetections = std::move(dets);
                m_zoomFrameProcessed = true;
            }
        }
    }
}

void MainWindow::setupUi() {
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainH = new QHBoxLayout(central);
    mainH->setSpacing(12);

    auto *leftCol = new QVBoxLayout;
    auto *panTitle = new QLabel("PAN CAMERA (Wide / Search)");
    panTitle->setAlignment(Qt::AlignCenter);
    panTitle->setStyleSheet("font-weight:bold; font-size:15px; color:#00d0ff;");
    leftCol->addWidget(panTitle);

    m_panView = new QLabel;
    m_panView->setMinimumSize(m_panDispW, m_panDispH);
    m_panView->setAlignment(Qt::AlignCenter);
    m_panView->setStyleSheet("background:#080808; border:2px solid #2a2a2a; border-radius:6px;");
    m_panView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_panView->setScaledContents(true);
    leftCol->addWidget(m_panView, 1);

    auto *r1 = new QHBoxLayout;
    r1->addWidget(new QLabel("Cam:"));
    m_panCameraSelect = new QComboBox; r1->addWidget(m_panCameraSelect, 1);
    m_modelBrowseBtn = new QPushButton("Browse YOLO .engine"); r1->addWidget(m_modelBrowseBtn);
    m_modelLabel = new QLabel("No Model");
    m_modelLabel->setStyleSheet("color:#888; font-size:11px;");
    m_modelLabel->setFixedWidth(140); r1->addWidget(m_modelLabel);
    m_aiToggleBtn = new QPushButton("AI OFF");
    m_aiToggleBtn->setCheckable(true);
    m_aiToggleBtn->setStyleSheet("background:#444; color:#fff; font-weight:bold; padding:5px 12px; border-radius:4px;");
    r1->addWidget(m_aiToggleBtn);
    m_settingsBtn = new QPushButton("Settings");
    m_settingsBtn->setStyleSheet("background:#2c3e50; color:#fff; font-weight:bold; padding:5px 12px; border-radius:4px;");
    r1->addWidget(m_settingsBtn);
    leftCol->addLayout(r1);

    auto *r2 = new QHBoxLayout;
    r2->addWidget(new QLabel("Conf:"));
    m_confSpinBox = new QDoubleSpinBox; m_confSpinBox->setRange(0.01, 1.0);
    m_confSpinBox->setSingleStep(0.05); m_confSpinBox->setValue(0.25); r2->addWidget(m_confSpinBox);
    r2->addWidget(new QLabel("IoU:"));
    m_iouSpinBox = new QDoubleSpinBox; m_iouSpinBox->setRange(0.01, 1.0);
    m_iouSpinBox->setSingleStep(0.05); m_iouSpinBox->setValue(0.45); r2->addWidget(m_iouSpinBox);
    r2->addWidget(new QLabel("Pan Skip:"));
    m_panSkipFrameSpinBox = new QSpinBox; m_panSkipFrameSpinBox->setRange(0, 30);
    m_panSkipFrameSpinBox->setValue(0); m_panSkipFrameSpinBox->setSuffix(" f");
    r2->addWidget(m_panSkipFrameSpinBox);
    r2->addWidget(new QLabel("Zoom Skip:"));
    m_zoomSkipFrameSpinBox = new QSpinBox; m_zoomSkipFrameSpinBox->setRange(0, 30);
    m_zoomSkipFrameSpinBox->setValue(0); m_zoomSkipFrameSpinBox->setSuffix(" f");
    r2->addWidget(m_zoomSkipFrameSpinBox);
    m_predDirBtn = new QPushButton("Pred Dir: OFF");
    m_predDirBtn->setCheckable(true);
    m_predDirBtn->setStyleSheet("background:#444; color:#fff; font-weight:bold; padding:5px 12px; border-radius:4px;");
    r2->addWidget(m_predDirBtn);
    r2->addStretch();
    leftCol->addLayout(r2);

    auto *r3 = new QHBoxLayout;
    auto *lockLbl = new QLabel("Lock Target:");
    lockLbl->setStyleSheet("font-weight:bold; color:#ffd700;");
    r3->addWidget(lockLbl);
    m_targetSelectCombo = new QComboBox;
    m_targetSelectCombo->addItem("Auto (First Target)", -1);
    m_targetSelectCombo->setMinimumWidth(220);
    r3->addWidget(m_targetSelectCombo, 1);
    m_clearLockBtn = new QPushButton("Clear Lock");
    m_clearLockBtn->setStyleSheet("background:#444; color:#fff; font-weight:bold; padding:4px 10px; border-radius:4px;");
    r3->addWidget(m_clearLockBtn);
    r3->addStretch();
    leftCol->addLayout(r3);

    auto *sensorBox = new QGroupBox("Base Station Telemetry (Filtered)");
    sensorBox->setMinimumHeight(80);
    auto *sg = new QGridLayout(sensorBox);
    sg->setColumnStretch(0, 5);
    sg->setColumnStretch(1, 2);
    sg->setColumnStretch(2, 2);
    sg->setHorizontalSpacing(16);
    m_sensorHeadingLabel = new QLabel("Heading: 0.0");
    m_sensorRollLabel = new QLabel("Roll: 0.0");
    m_sensorGpsLabel = new QLabel("GPS: No Fix (0 Sat)");
    m_sensorCoordLabel = new QLabel("Lat: 0.000000 | Lon: 0.000000");
    m_sensorAltLabel = new QLabel("Alt: 0.0 m");
    m_relocateBaseBtn = new QPushButton("Relocate Base");
    m_relocateBaseBtn->setToolTip("Reset GPS cumulative average for a new base location");
    m_relocateBaseBtn->setStyleSheet("background:#205072; color:#fff; font-weight:bold; padding:4px 8px; border-radius:4px;");
    m_sensorHeadingLabel->setMinimumWidth(110);
    m_sensorRollLabel->setMinimumWidth(110);
    m_sensorGpsLabel->setMinimumWidth(130);
    m_sensorCoordLabel->setMinimumWidth(220);
    m_sensorAltLabel->setMinimumWidth(90);
    m_sensorHeadingLabel->setStyleSheet("color:#a0e0ff; font-family:monospace; font-weight:bold;");
    m_sensorRollLabel->setStyleSheet("color:#a0e0ff; font-family:monospace; font-weight:bold;");
    m_sensorGpsLabel->setStyleSheet("color:#ffd060; font-family:monospace; font-weight:bold;");
    m_sensorCoordLabel->setStyleSheet("color:#d0d0d0; font-family:monospace;");
    m_sensorAltLabel->setStyleSheet("color:#d0d0d0; font-family:monospace;");
    sg->addWidget(m_sensorHeadingLabel, 0, 0);
    sg->addWidget(m_sensorRollLabel, 0, 1);
    sg->addWidget(m_sensorGpsLabel, 0, 2);
    sg->addWidget(m_sensorCoordLabel, 1, 0);
    sg->addWidget(m_sensorAltLabel, 1, 1);
    sg->addWidget(m_relocateBaseBtn, 1, 2);
    connect(m_relocateBaseBtn, &QPushButton::clicked, this, &MainWindow::onRelocateBaseClicked);
    leftCol->addWidget(sensorBox);

    auto *calcBox = new QGroupBox("Target Geo-Coordinates (Flowchart)");
    calcBox->setMinimumHeight(80);
    auto *cg = new QGridLayout(calcBox);
    cg->setColumnStretch(0, 4);
    cg->setColumnStretch(1, 3);
    m_calcTargetInfoLabel = new QLabel("Target: None [Searching]");
    m_calcTargetInfoLabel->setMinimumWidth(400);
    m_calcTargetInfoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_calcTargetInfoLabel->setStyleSheet("color:#00ffaa; font-family:monospace; font-weight:bold; font-size:13px;");
    m_calcDistLabel = new QLabel("Distance (d): -- m");
    m_calcDistLabel->setMinimumWidth(200);
    m_calcDistLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_calcDistLabel->setStyleSheet("color:#ffd060; font-family:monospace; font-weight:bold;");
    m_calcTargetCoordLabel = new QLabel("Target Lat: -- | Lon: --");
    m_calcTargetCoordLabel->setMinimumWidth(360);
    m_calcTargetCoordLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_calcTargetCoordLabel->setStyleSheet("color:#00e5ff; font-family:monospace; font-weight:bold;");
    m_calcTargetAltLabel = new QLabel("Target Alt: -- m");
    m_calcTargetAltLabel->setMinimumWidth(240);
    m_calcTargetAltLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_calcTargetAltLabel->setStyleSheet("color:#00e5ff; font-family:monospace; font-weight:bold;");
    cg->addWidget(m_calcTargetInfoLabel, 0, 0);
    cg->addWidget(m_calcDistLabel, 0, 1);
    cg->addWidget(m_calcTargetCoordLabel, 1, 0);
    cg->addWidget(m_calcTargetAltLabel, 1, 1);
    leftCol->addWidget(calcBox);

    auto *rightCol = new QVBoxLayout;
    auto *zoomTitle = new QLabel("ZOOM CAMERA (Identification & Distance)");
    zoomTitle->setAlignment(Qt::AlignCenter);
    zoomTitle->setStyleSheet("font-weight:bold; font-size:15px; color:#00ffaa;");
    rightCol->addWidget(zoomTitle);

    m_zoomView = new QLabel;
    m_zoomView->setMinimumSize(m_zoomDispW, m_zoomDispH);
    m_zoomView->setAlignment(Qt::AlignCenter);
    m_zoomView->setStyleSheet("background:#080808; border:2px solid #2a2a2a; border-radius:6px;");
    m_zoomView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_zoomView->setScaledContents(true);
    rightCol->addWidget(m_zoomView, 1);

    auto *zr = new QHBoxLayout;
    zr->addWidget(new QLabel("Cam:"));
    m_zoomCameraSelect = new QComboBox; zr->addWidget(m_zoomCameraSelect, 1);
    rightCol->addLayout(zr);

    auto *botRow = new QHBoxLayout;
    auto *modeBox = new QGroupBox("Control Mode"); auto *ml = new QVBoxLayout(modeBox);
    m_modeBtn = new QPushButton("MANUAL"); m_modeBtn->setMinimumSize(120, 60);
    m_modeBtn->setStyleSheet("background:#28a745; color:white; font-size:16px; font-weight:bold; border-radius:6px;");
    ml->addWidget(m_modeBtn, 0, Qt::AlignCenter); botRow->addWidget(modeBox);

    auto *joyBox = new QGroupBox("Joystick"); auto *jl = new QVBoxLayout(joyBox);
    m_joystick = new JoystickWidget; jl->addWidget(m_joystick, 0, Qt::AlignCenter);
    botRow->addWidget(joyBox);

    auto *speedBox = new QGroupBox("SPEED"); auto *sl = new QVBoxLayout(speedBox);
    auto *fl = new QLabel("FAST"); fl->setAlignment(Qt::AlignCenter);
    fl->setStyleSheet("font-size:10px; color:#0f0;"); sl->addWidget(fl);
    m_speedSlider = new QSlider(Qt::Vertical); m_speedSlider->setRange(0, 100);
    m_speedSlider->setMinimumHeight(140);
    m_speedSlider->setTickPosition(QSlider::TicksBothSides); m_speedSlider->setTickInterval(10);
    sl->addWidget(m_speedSlider, 0, Qt::AlignHCenter);
    auto *sll = new QLabel("SLOW"); sll->setAlignment(Qt::AlignCenter);
    sll->setStyleSheet("font-size:10px; color:#f00;"); sl->addWidget(sll);
    m_speedValueLabel = new QLabel; m_speedValueLabel->setAlignment(Qt::AlignCenter);
    sl->addWidget(m_speedValueLabel); botRow->addWidget(speedBox);
    rightCol->addLayout(botRow);

    m_motorLabel = new QLabel("Motor: STOP");
    m_motorLabel->setMinimumWidth(400);
    m_motorLabel->setStyleSheet("color:#00ffc8; font-family:monospace; font-weight:bold; font-size:13px; padding:4px;");
    rightCol->addWidget(m_motorLabel);

    mainH->addLayout(leftCol, 6);
    mainH->addLayout(rightCol, 5);
    syncSpeedSlider();

    central->setStyleSheet(
        "QWidget { background-color: #161616; color: #eee; }"
        "QGroupBox { border: 1px solid #3e3e3e; border-radius: 6px; margin-top: 12px; padding-top: 12px; font-weight: bold; color: #aaa; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 4px; }"
        "QComboBox { background: #262626; color: #eee; padding: 4px; border: 1px solid #444; border-radius: 4px; }"
        "QPushButton { background: #2d2d2d; color: #eee; border: 1px solid #555; border-radius: 4px; padding: 5px 10px; }"
        "QPushButton:hover { background: #3d3d3d; }"
        "QSpinBox, QDoubleSpinBox { background: #262626; color: #eee; border: 1px solid #444; border-radius: 4px; padding: 2px 4px; }"
        "QSlider::groove:vertical { background: #333; width: 8px; border-radius: 4px; }"
        "QSlider::handle:vertical { background: #00aaff; height: 16px; margin: 0 -4px; border-radius: 4px; }"
    );
}

void MainWindow::setupCameras() {
    m_cameraDevices = V4L2Camera::availableDevices();
    if (m_cameraDevices.isEmpty()) {
        m_panView->setText("No cameras found");
        m_zoomView->setText("No cameras found");
        return;
    }
    for (const auto &dev : m_cameraDevices) {
        m_panCameraSelect->addItem(dev);
        m_zoomCameraSelect->addItem(dev);
    }
    m_panCamIdx = 0; m_panCameraSelect->setCurrentIndex(0);
    m_zoomCamIdx = (m_cameraDevices.size() >= 2) ? 1 : 0;
    m_zoomCameraSelect->setCurrentIndex(m_zoomCamIdx);
    ensureCameraRunning(m_panCamIdx);
    if (m_zoomCamIdx != m_panCamIdx) ensureCameraRunning(m_zoomCamIdx);
}

void MainWindow::ensureCameraRunning(int deviceIndex) {
    if (m_activeCameras.contains(deviceIndex)) return;
    if (deviceIndex < 0 || deviceIndex >= m_cameraDevices.size()) return;
    int capW = (deviceIndex == m_zoomCamIdx) ? m_zoomCapW : m_panCapW;
    int capH = (deviceIndex == m_zoomCamIdx) ? m_zoomCapH : m_panCapH;
    auto *cam = new V4L2Camera(m_cameraDevices[deviceIndex], capW, capH, this);
    connect(cam, &V4L2Camera::errorOccurred, this, [this](const QString &msg) {
        if (m_motorLabel) m_motorLabel->setText("Camera error: " + msg);
    });
    cam->start();
    m_activeCameras[deviceIndex] = cam;
}

void MainWindow::stopUnusedCameras() {
    QList<int> toRemove;
    for (auto it = m_activeCameras.begin(); it != m_activeCameras.end(); ++it) {
        if (it.key() != m_panCamIdx && it.key() != m_zoomCamIdx) {
            it.value()->stopCapture(); it.value()->wait(); it.value()->deleteLater();
            toRemove.append(it.key());
        }
    }
    for (int idx : toRemove) m_activeCameras.remove(idx);
}


void MainWindow::onTargetSelectionChanged(int index) {
    if (index <= 0) {
        m_targetSelector.setSelectedTrackId(-1);
    } else {
        m_targetSelector.setSelectedTrackId(m_targetSelectCombo->itemData(index).toInt());
    }
}

void MainWindow::onRenderTick() {
    int panFrameW = m_panDispW;
    int zoomFrameW = m_zoomDispW;

    if (m_panCamIdx >= 0 && m_activeCameras.contains(m_panCamIdx) && m_panView) {
        QImage fullFrame;
        if (m_activeCameras[m_panCamIdx]->getLatestFrame(fullFrame)) {
            panFrameW = fullFrame.width();
            auto &t = m_fpsTrackers[m_panCamIdx];
            t.frameCount++;
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (t.lastTime == 0) { t.lastTime = now; }
            else if (now - t.lastTime >= 500) {
                t.currentFps = (t.frameCount * 1000.0f) / (now - t.lastTime);
                t.frameCount = 0; t.lastTime = now;
            }

            if (m_aiEnabled && m_sharedYolo.isLoaded() && !m_modelLoading.load()) {
                bool isZoomTracking = (m_targetSelector.flowchartState() == FlowchartState::ZoomTrack);
                if (isZoomTracking) {
                    std::lock_guard<std::mutex> lock(m_aiMutex);
                    m_lastPanDetections.clear();
                    m_panTracks.clear();
                } else {
                    int skip = m_panSkipFrameSpinBox ? m_panSkipFrameSpinBox->value() : 0;
                    bool shouldSubmitPan = true;
                    if (skip > 0) {
                        m_panFrameCounter++;
                        if (m_panFrameCounter % (skip + 1) != 0) shouldSubmitPan = false;
                    } else {
                        m_panFrameCounter = 0;
                    }

                    if (shouldSubmitPan) {
                        if (m_workMutex.try_lock()) {
                            if (!m_hasPendingPan) {
                                m_pendingPanFrame = fullFrame;
                                m_pendingConf = static_cast<float>(m_confSpinBox->value());
                                m_pendingIou = static_cast<float>(m_iouSpinBox->value());
                                m_hasPendingPan = true;
                                m_workCv.notify_one();
                            }
                            m_workMutex.unlock();
                        }
                    }
                }
            } else {
                m_panFrameCounter = 0;
                std::lock_guard<std::mutex> lock(m_aiMutex);
                m_lastPanDetections.clear();
                m_panTracks.clear();
            }

            float sx = static_cast<float>(m_panDispW) / fullFrame.width();
            float sy = static_cast<float>(m_panDispH) / fullFrame.height();
            QImage small = fullFrame.scaled(m_panDispW, m_panDispH,
                Qt::IgnoreAspectRatio, Qt::FastTransformation);
            QPainter p(&small);

            float pcx = m_panCenterX * sx;
            float pcy = m_panCenterY * sy;
            HudRenderer::drawCrosshair(p, pcx, pcy, QColor(0, 230, 255, 120));
            HudRenderer::drawTracks(p, m_panTracks, sx, sy, m_hasActiveLockedTarget, m_activeLockedTarget.trackId, m_predDirEnabled, QColor(0, 230, 255));
            HudRenderer::drawFps(p, t.currentFps);
            p.end();
            m_panView->setPixmap(QPixmap::fromImage(small));
        }
    }

    if (m_zoomCamIdx >= 0 && m_activeCameras.contains(m_zoomCamIdx) && m_zoomView) {
        QImage fullFrame;
        if (m_activeCameras[m_zoomCamIdx]->getLatestFrame(fullFrame)) {
            zoomFrameW = fullFrame.width();
            auto &t = m_fpsTrackers[m_zoomCamIdx];
            t.frameCount++;
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (t.lastTime == 0) { t.lastTime = now; }
            else if (now - t.lastTime >= 500) {
                t.currentFps = (t.frameCount * 1000.0f) / (now - t.lastTime);
                t.frameCount = 0; t.lastTime = now;
            }

            if (m_aiEnabled && m_sharedYolo.isLoaded() && !m_modelLoading.load()) {
                int skip = m_zoomSkipFrameSpinBox ? m_zoomSkipFrameSpinBox->value() : 0;
                bool shouldSubmitZoom = true;
                if (skip > 0) {
                    m_zoomFrameCounter++;
                    if (m_zoomFrameCounter % (skip + 1) != 0) shouldSubmitZoom = false;
                } else {
                    m_zoomFrameCounter = 0;
                }

                if (shouldSubmitZoom) {
                    if (m_workMutex.try_lock()) {
                        if (!m_hasPendingZoom) {
                            m_pendingZoomFrame = fullFrame;
                            m_pendingConf = static_cast<float>(m_confSpinBox->value());
                            m_pendingIou = static_cast<float>(m_iouSpinBox->value());
                            m_hasPendingZoom = true;
                            m_workCv.notify_one();
                        }
                        m_workMutex.unlock();
                    }
                }
            } else {
                m_zoomFrameCounter = 0;
                std::lock_guard<std::mutex> lock(m_aiMutex);
                m_lastZoomDetections.clear();
                m_zoomTracks.clear();
            }

            float sx = static_cast<float>(m_zoomDispW) / fullFrame.width();
            float sy = static_cast<float>(m_zoomDispH) / fullFrame.height();
            QImage small = fullFrame.scaled(m_zoomDispW, m_zoomDispH,
                Qt::IgnoreAspectRatio, Qt::FastTransformation);
            QPainter p(&small);

            float zcx = m_zoomCenterX * sx;
            float zcy = m_zoomCenterY * sy;
            HudRenderer::drawCrosshair(p, zcx, zcy, QColor(0, 255, 170, 120));
            HudRenderer::drawTracks(p, m_zoomTracks, sx, sy, m_hasActiveLockedTarget, m_activeLockedTarget.trackId, m_predDirEnabled, QColor(0, 255, 170));
            HudRenderer::drawFps(p, t.currentFps);
            p.end();
            m_zoomView->setPixmap(QPixmap::fromImage(small));
        }
    }

    bool zoomProcessed = false;
    bool panProcessed = false;
    std::vector<Detection> panDets;
    std::vector<Detection> zoomDets;
    {
        std::lock_guard<std::mutex> lock(m_aiMutex);
        if (m_zoomFrameProcessed) {
            zoomProcessed = true;
            m_zoomFrameProcessed = false;
        }
        if (m_panFrameProcessed) {
            panProcessed = true;
            m_panFrameProcessed = false;
        }
        panDets = m_lastPanDetections;
        zoomDets = m_lastZoomDetections;
    }

    if (panProcessed) {
        m_targetTracker.updateTracks(m_panTracks, panDets);
    }
    if (zoomProcessed) {
        m_targetTracker.updateTracks(m_zoomTracks, zoomDets);
    }

    ActiveTargetCam activeSource = ActiveTargetCam::None;
    TrackedTarget activeTarget;
    std::vector<TrackedTarget> activeCandidates;
    bool needClearZoom = false;
    bool hasTarget = m_targetSelector.evaluate(m_panTracks, m_zoomTracks, activeSource, activeTarget, activeCandidates, needClearZoom);
    if (needClearZoom) {
        std::lock_guard<std::mutex> lock(m_aiMutex);
        m_lastZoomDetections.clear();
        m_zoomTracks.clear();
    }

    m_targetSelector.syncCombo(m_targetSelectCombo, activeCandidates);

    {
        std::lock_guard<std::mutex> lock(m_aiMutex);
        m_activeTargetSource = hasTarget ? activeSource : ActiveTargetCam::None;
        m_activeLockedTarget = activeTarget;
        m_hasActiveLockedTarget = hasTarget;
    }

    if (hasTarget) {
        double wReal = m_settingsDialog ? m_settingsDialog->getWRealForClass(activeTarget.classId) : 0.45;
        double focal = (activeSource == ActiveTargetCam::Zoom)
            ? (m_settingsDialog ? m_settingsDialog->zoomFocalPx(zoomFrameW) : 2777.8)
            : (m_settingsDialog ? m_settingsDialog->panFocalPx(panFrameW) : 277.8);
        double d = GeoEstimator::calculateDistance(wReal, focal, activeTarget.bbox.width());

        double baseLat = 0.0, baseLon = 0.0;
        float baseAlt = 0.0f, heading = 0.0f, roll = 0.0f;
        {
            std::lock_guard<std::mutex> lock(m_sensorMutex);
            baseLat = m_geoEstimator.baseLat();
            baseLon = m_geoEstimator.baseLon();
            baseAlt = m_geoEstimator.baseAlt();
            heading = m_currentHeading;
            roll = m_currentRoll;
        }

        QString srcName = (activeSource == ActiveTargetCam::Pan) ? "PAN" : "ZOOM";
        if (m_calcTargetInfoLabel) {
            m_calcTargetInfoLabel->setText(QString("Tracking: %1 | [LOCKED #%2] %3 (W:%4m)")
                .arg(srcName).arg(activeTarget.trackId).arg(activeTarget.className).arg(wReal, 0, 'f', 2));
        }
        if (m_calcDistLabel) {
            m_calcDistLabel->setText(QString("d: %1 m").arg(d, 0, 'f', 2));
        }

        GeoTargetResult geo = GeoEstimator::estimateTargetGeo(d, baseLat, baseLon, baseAlt, heading, roll);
        if (geo.hasValidGeo) {
            if (m_calcTargetCoordLabel) {
                m_calcTargetCoordLabel->setText(QString("T.Lat: %1 | Lon: %2")
                    .arg(geo.targetLat, 0, 'f', 6).arg(geo.targetLon, 0, 'f', 6));
            }
            if (m_calcTargetAltLabel) {
                m_calcTargetAltLabel->setText(QString("T.Alt: %1 m (dZ: %2)")
                    .arg(geo.targetAlt, 0, 'f', 1).arg(geo.dZ, 0, 'f', 1));
            }
        } else {
            if (m_calcTargetCoordLabel) m_calcTargetCoordLabel->setText("T.Lat: WAIT GPS | Lon: WAIT GPS");
            if (m_calcTargetAltLabel) m_calcTargetAltLabel->setText("T.Alt: WAIT GPS");
        }
    } else {
        if (m_calcTargetInfoLabel) m_calcTargetInfoLabel->setText("Target: None [Searching]");
        if (m_calcDistLabel) m_calcDistLabel->setText("d: -- m");
        if (m_calcTargetCoordLabel) m_calcTargetCoordLabel->setText("T.Lat: -- | Lon: --");
        if (m_calcTargetAltLabel) m_calcTargetAltLabel->setText("T.Alt: -- m");
    }
}
void MainWindow::onPanCameraSelected(int index) {
    if (index < 0 || index >= m_cameraDevices.size()) return;
    m_panCamIdx = index; ensureCameraRunning(m_panCamIdx); stopUnusedCameras();
}

void MainWindow::onZoomCameraSelected(int index) {
    if (index < 0 || index >= m_cameraDevices.size()) return;
    m_zoomCamIdx = index; ensureCameraRunning(m_zoomCamIdx); stopUnusedCameras();
}

void MainWindow::onBrowseModel() {
    if (m_modelLoading.load()) return;
    QString file = QFileDialog::getOpenFileName(this, "Select YOLO Engine", "", "Engine (*.engine *.plan);;All (*)");
    if (file.isEmpty()) return;
    m_modelLoading.store(true); m_modelBrowseBtn->setEnabled(false);
    m_modelLabel->setStyleSheet("color:#ffcc00; font-size:11px;");
    m_modelLabel->setText("Loading...");
    QString path = file;
    std::thread([this, path]() {
        bool ok = m_sharedYolo.loadEngine(path); QString err = m_sharedYolo.lastError();
        emit modelEngineReady(ok, err, path);
    }).detach();
}

void MainWindow::onModelEngineReady(bool ok, QString errMsg, QString path) {
    m_modelLoading.store(false); m_modelBrowseBtn->setEnabled(true);
    if (ok) {
        m_modelLabel->setText(QFileInfo(path).fileName());
        m_modelLabel->setStyleSheet("color:#00ffaa; font-size:11px;");
    } else {
        m_modelLabel->setText("Load Failed");
        m_modelLabel->setStyleSheet("color:#ff4444; font-size:11px;");
        if (errMsg.isEmpty()) errMsg = "Failed to deserialize engine.";
        QMessageBox::warning(this, "YOLO Load Error", errMsg);
    }
}

void MainWindow::onToggleAi() {
    m_aiEnabled = m_aiToggleBtn->isChecked();
    m_aiToggleBtn->setText(m_aiEnabled ? "AI ON" : "AI OFF");
    m_aiToggleBtn->setStyleSheet(m_aiEnabled
        ? "background:#0099ff; color:white; font-weight:bold; padding:5px 12px; border-radius:4px;"
        : "background:#444; color:white; font-weight:bold; padding:5px 12px; border-radius:4px;");
}

void MainWindow::onTogglePredDir() {
    m_predDirEnabled = m_predDirBtn->isChecked();
    m_predDirBtn->setText(m_predDirEnabled ? "Pred Dir: ON" : "Pred Dir: OFF");
    m_predDirBtn->setStyleSheet(m_predDirEnabled
        ? "background:#00b4d8; color:white; font-weight:bold; padding:5px 12px; border-radius:4px;"
        : "background:#444; color:white; font-weight:bold; padding:5px 12px; border-radius:4px;");
}

void MainWindow::onOpenSettings() {
    if (m_settingsDialog && m_settingsDialog->exec() == QDialog::Accepted) {
        applySettings();
    }
}

void MainWindow::onToggleMode() {
    if (m_mode == ControlMode::Manual) {
        m_mode = ControlMode::Auto;
        m_modeBtn->setText("AUTO (PID)");
        m_modeBtn->setStyleSheet("background:#0077ff; color:white; font-size:16px; font-weight:bold; border-radius:6px;");
        m_panPid.reset(); m_tiltPid.reset(); m_pidTimer.restart();
    } else {
        m_mode = ControlMode::Manual;
        m_modeBtn->setText("MANUAL");
        m_modeBtn->setStyleSheet("background:#28a745; color:white; font-size:16px; font-weight:bold; border-radius:6px;");
    }
}

void MainWindow::onJoystickAxis(float x, float y) { m_joyX = x; m_joyY = y; }

void MainWindow::onSpeedSliderChanged(int value) {
    m_speed = static_cast<uint16_t>(MAX_SPEED - value * (MAX_SPEED - MIN_SPEED) / 100);
    if (m_speedValueLabel) m_speedValueLabel->setText(QString("%1 us").arg(m_speed));
}

void MainWindow::syncSpeedSlider() {
    int v = (MAX_SPEED - m_speed) * 100 / (MAX_SPEED - MIN_SPEED);
    if (m_speedSlider) { m_speedSlider->blockSignals(true); m_speedSlider->setValue(v); m_speedSlider->blockSignals(false); }
    if (m_speedValueLabel) m_speedValueLabel->setText(QString("%1 us").arg(m_speed));
}

void MainWindow::keyPressEvent(QKeyEvent *e) {
    if (e->isAutoRepeat()) { e->ignore(); return; }
    switch (e->key()) {
    case Qt::Key_W: m_keyW = true; break;
    case Qt::Key_A: m_keyA = true; break;
    case Qt::Key_S: m_keyS = true; break;
    case Qt::Key_D: m_keyD = true; break;
    case Qt::Key_Left: m_speed = std::max<uint16_t>(MIN_SPEED, m_speed - SPEED_STEP); syncSpeedSlider(); break;
    case Qt::Key_Right: m_speed = std::min<uint16_t>(MAX_SPEED, m_speed + SPEED_STEP); syncSpeedSlider(); break;
    case Qt::Key_Escape: close(); return;
    default: QMainWindow::keyPressEvent(e); return;
    }
    e->accept();
}

void MainWindow::keyReleaseEvent(QKeyEvent *e) {
    if (e->isAutoRepeat()) { e->ignore(); return; }
    switch (e->key()) {
    case Qt::Key_W: m_keyW = false; break;
    case Qt::Key_A: m_keyA = false; break;
    case Qt::Key_S: m_keyS = false; break;
    case Qt::Key_D: m_keyD = false; break;
    default: QMainWindow::keyReleaseEvent(e); return;
    }
    e->accept();
}

void MainWindow::applySettings() {
    if (!m_settingsDialog) return;
    float kp = static_cast<float>(m_settingsDialog->pidKp());
    float ki = static_cast<float>(m_settingsDialog->pidKi());
    float kd = static_cast<float>(m_settingsDialog->pidKd());
    float db = static_cast<float>(m_settingsDialog->pidDeadband());
    m_panPid.setGains(kp, ki, kd, db);
    m_tiltPid.setGains(kp, ki, kd, db);
    m_panCenterX = static_cast<float>(m_settingsDialog->panCenterX());
    m_panCenterY = static_cast<float>(m_settingsDialog->panCenterY());
    m_zoomCenterX = static_cast<float>(m_settingsDialog->zoomCenterX());
    m_zoomCenterY = static_cast<float>(m_settingsDialog->zoomCenterY());
    m_autoMinSpeed = m_settingsDialog->autoMinSpeed();
    m_autoMaxSpeed = m_settingsDialog->autoMaxSpeed();

    int newPanW = m_settingsDialog->panCapWidth();
    int newPanH = m_settingsDialog->panCapHeight();
    int newZoomW = m_settingsDialog->zoomCapWidth();
    int newZoomH = m_settingsDialog->zoomCapHeight();
    int newPanDispW = m_settingsDialog->panDispWidth();
    int newPanDispH = m_settingsDialog->panDispHeight();
    int newZoomDispW = m_settingsDialog->zoomDispWidth();
    int newZoomDispH = m_settingsDialog->zoomDispHeight();

    bool panCapChanged = (newPanW != m_panCapW || newPanH != m_panCapH);
    bool zoomCapChanged = (newZoomW != m_zoomCapW || newZoomH != m_zoomCapH);

    m_panCapW = newPanW; m_panCapH = newPanH;
    m_zoomCapW = newZoomW; m_zoomCapH = newZoomH;
    m_panDispW = newPanDispW; m_panDispH = newPanDispH;
    m_zoomDispW = newZoomDispW; m_zoomDispH = newZoomDispH;

    if (m_panView) m_panView->setMinimumSize(m_panDispW, m_panDispH);
    if (m_zoomView) m_zoomView->setMinimumSize(m_zoomDispW, m_zoomDispH);

    if (panCapChanged && m_panCamIdx >= 0 && m_activeCameras.contains(m_panCamIdx)) {
        auto *oldCam = m_activeCameras.take(m_panCamIdx);
        oldCam->stopCapture();
        oldCam->wait();
        oldCam->deleteLater();
        ensureCameraRunning(m_panCamIdx);
    }
    if (zoomCapChanged && m_zoomCamIdx >= 0 && m_activeCameras.contains(m_zoomCamIdx) && m_zoomCamIdx != m_panCamIdx) {
        auto *oldCam = m_activeCameras.take(m_zoomCamIdx);
        oldCam->stopCapture();
        oldCam->wait();
        oldCam->deleteLater();
        ensureCameraRunning(m_zoomCamIdx);
    }
}

PanTiltState MainWindow::computeMotorCommand() {
    if (m_mode == ControlMode::Auto) {
        bool hasTarget = false;
        TrackedTarget target;
        ActiveTargetCam source = ActiveTargetCam::None;
        {
            std::lock_guard<std::mutex> lock(m_aiMutex);
            hasTarget = m_hasActiveLockedTarget;
            target = m_activeLockedTarget;
            source = m_activeTargetSource;
        }

        if (hasTarget) {
            float errX = 0.0f, errY = 0.0f;
            CommandArbitrator::computeCentroidError(target, source,
                                                    m_panCenterX, m_panCenterY,
                                                    m_zoomCenterX, m_zoomCenterY,
                                                    m_panCapW, m_panCapH,
                                                    m_zoomCapW, m_zoomCapH,
                                                    errX, errY);
            float dt = m_pidTimer.restart() / 1000.0f;
            dt = std::max(0.005f, std::min(dt, 0.1f));
            return CommandArbitrator::computeAutoCommand(errX, errY, dt, m_panPid, m_tiltPid, m_autoMinSpeed, m_autoMaxSpeed);
        } else {
            m_panPid.reset();
            m_tiltPid.reset();
            m_pidTimer.restart();
            return PanTiltState::Stop();
        }
    } else {
        ManualInputState in;
        in.joyX = m_joyX;
        in.joyY = m_joyY;
        in.keyW = m_keyW;
        in.keyA = m_keyA;
        in.keyS = m_keyS;
        in.keyD = m_keyD;
        in.speed = m_speed;
        return CommandArbitrator::computeManualCommand(in);
    }
}
void MainWindow::onCommandTick() {
    PanTiltState cmd = computeMotorCommand();
    m_serial->SetState(cmd);
    auto lbl = [](const AxisState &a) -> QString {
        if (!a.en) return "STOP";
        return QString("%1 spd=%2").arg(a.dir ? "CW" : "CCW").arg(a.speed);
    };
    QString ms = (m_mode == ControlMode::Auto) ? "[AUTO PID]" : "[MANUAL]";
    m_motorLabel->setText(QString("%1 Pan: %2 | Tilt: %3 | Spd: %4 us")
        .arg(ms, lbl(cmd.pan), lbl(cmd.tilt)).arg(m_speed));
}

void MainWindow::onSensorDataReceived(float heading, float roll, double lat, double lon, float alt, int sat, bool hasGps) {
    {
        std::lock_guard<std::mutex> lock(m_sensorMutex);
        m_currentHeading = heading;
        m_currentRoll = roll;
        m_hasGps = hasGps;
        m_geoEstimator.updateBaseGps(lat, lon, alt, hasGps);
    }

    if (m_sensorHeadingLabel) m_sensorHeadingLabel->setText(QString("Heading: %1").arg(heading, 0, 'f', 1));
    if (m_sensorRollLabel) m_sensorRollLabel->setText(QString("Roll: %1").arg(roll, 0, 'f', 1));
    if (m_sensorGpsLabel) m_sensorGpsLabel->setText(QString("GPS: %1 (%2 Sat)").arg(hasGps ? "Fix" : "No-Fix").arg(sat));
    if (m_sensorCoordLabel) {
        m_sensorCoordLabel->setText(QString("Lat: %1 | Lon: %2")
            .arg(m_geoEstimator.baseLat(), 0, 'f', 6).arg(m_geoEstimator.baseLon(), 0, 'f', 6));
    }
    if (m_sensorAltLabel) {
        m_sensorAltLabel->setText(QString("Alt: %1 m (%2 samples)")
            .arg(m_geoEstimator.baseAlt(), 0, 'f', 1).arg(m_geoEstimator.sampleCount()));
    }
}

void MainWindow::onRelocateBaseClicked() {
    {
        std::lock_guard<std::mutex> lock(m_sensorMutex);
        m_geoEstimator.resetBaseGps();
    }
    if (m_sensorCoordLabel) {
        m_sensorCoordLabel->setText("Lat: 0.000000 | Lon: 0.000000");
    }
    if (m_sensorAltLabel) {
        m_sensorAltLabel->setText("Alt: 0.0 m");
    }
}
