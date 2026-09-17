#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QHeaderView>
#include <QTabWidget>

struct SensorPreset {
    const char *name;
    double widthMm;
};

static const SensorPreset g_sensorPresets[] = {
    {"1/2.5\" (5.76 mm)", 5.76},
    {"1/2.8\" (5.18 mm)", 5.18},
    {"1/2\" (6.40 mm)", 6.40},
    {"1/1.8\" (7.18 mm)", 7.18},
    {"1/2.3\" (6.17 mm)", 6.17},
    {"1/2.7\" (5.37 mm)", 5.37},
    {"1/3\" (4.80 mm)", 4.80},
    {"1/4\" (3.60 mm)", 3.60},
    {"Custom", 0.0}
};

static const int g_numSensorPresets = sizeof(g_sensorPresets) / sizeof(g_sensorPresets[0]);

struct ResolutionPreset {
    const char *name;
    int width;
    int height;
};

static const ResolutionPreset g_capPresets[] = {
    {"640x480 (VGA)", 640, 480},
    {"1280x720 (720p HD)", 1280, 720},
    {"1920x1080 (1080p FHD)", 1920, 1080},
    {"320x240 (QVGA)", 320, 240},
    {"Custom", 0, 0}
};
static const int g_numCapPresets = sizeof(g_capPresets) / sizeof(g_capPresets[0]);

static const ResolutionPreset g_dispPresets[] = {
    {"320x240 (Compact)", 320, 240},
    {"480x360 (Medium)", 480, 360},
    {"640x480 (Large)", 640, 480},
    {"Custom", 0, 0}
};
static const int g_numDispPresets = sizeof(g_dispPresets) / sizeof(g_dispPresets[0]);

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    loadSettings(m_settingsFilePath);
}

void SettingsDialog::setupUi() {
    setWindowTitle("Camera & System Settings");
    resize(640, 580);

    auto *mainLayout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    auto *opticsTab = new QWidget(this);
    auto *opticsLayout = new QVBoxLayout(opticsTab);

    auto *topLayout = new QHBoxLayout;
    topLayout->addWidget(new QLabel("Drone Classes & Real Width (Wreal):"));
    topLayout->addStretch();
    m_addBtn = new QPushButton("+ Add");
    m_delBtn = new QPushButton("- Remove");
    topLayout->addWidget(m_addBtn);
    topLayout->addWidget(m_delBtn);
    opticsLayout->addLayout(topLayout);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels(QStringList() << "Class ID" << "Class Name" << "Wreal (m)");
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    opticsLayout->addWidget(m_table, 1);

    auto *panBox = new QGroupBox("PAN Camera Optics");
    auto *panGrid = new QGridLayout(panBox);
    panGrid->addWidget(new QLabel("Focal (mm):"), 0, 0);
    m_panFocalMmSpin = new QDoubleSpinBox(this);
    m_panFocalMmSpin->setRange(0.1, 1000.0);
    m_panFocalMmSpin->setSingleStep(0.5);
    m_panFocalMmSpin->setValue(m_panFocalMm);
    panGrid->addWidget(m_panFocalMmSpin, 0, 1);

    panGrid->addWidget(new QLabel("Sensor Format:"), 0, 2);
    m_panSensorCombo = new QComboBox(this);
    for (int i = 0; i < g_numSensorPresets; ++i) {
        m_panSensorCombo->addItem(g_sensorPresets[i].name);
    }
    panGrid->addWidget(m_panSensorCombo, 0, 3);

    panGrid->addWidget(new QLabel("Sensor W (mm):"), 1, 0);
    m_panSensorWidthSpin = new QDoubleSpinBox(this);
    m_panSensorWidthSpin->setRange(0.1, 100.0);
    m_panSensorWidthSpin->setSingleStep(0.1);
    m_panSensorWidthSpin->setValue(m_panSensorWidthMm);
    panGrid->addWidget(m_panSensorWidthSpin, 1, 1);

    m_panCalculatedLabel = new QLabel("f = 0.0 px");
    m_panCalculatedLabel->setStyleSheet("color:#00e5ff; font-weight:bold; font-family:monospace;");
    panGrid->addWidget(m_panCalculatedLabel, 1, 2, 1, 2);
    opticsLayout->addWidget(panBox);

    auto *zoomBox = new QGroupBox("ZOOM Camera Optics");
    auto *zoomGrid = new QGridLayout(zoomBox);
    zoomGrid->addWidget(new QLabel("Focal (mm):"), 0, 0);
    m_zoomFocalMmSpin = new QDoubleSpinBox(this);
    m_zoomFocalMmSpin->setRange(0.1, 1000.0);
    m_zoomFocalMmSpin->setSingleStep(1.0);
    m_zoomFocalMmSpin->setValue(m_zoomFocalMm);
    zoomGrid->addWidget(m_zoomFocalMmSpin, 0, 1);

    zoomGrid->addWidget(new QLabel("Sensor Format:"), 0, 2);
    m_zoomSensorCombo = new QComboBox(this);
    for (int i = 0; i < g_numSensorPresets; ++i) {
        m_zoomSensorCombo->addItem(g_sensorPresets[i].name);
    }
    zoomGrid->addWidget(m_zoomSensorCombo, 0, 3);

    zoomGrid->addWidget(new QLabel("Sensor W (mm):"), 1, 0);
    m_zoomSensorWidthSpin = new QDoubleSpinBox(this);
    m_zoomSensorWidthSpin->setRange(0.1, 100.0);
    m_zoomSensorWidthSpin->setSingleStep(0.1);
    m_zoomSensorWidthSpin->setValue(m_zoomSensorWidthMm);
    zoomGrid->addWidget(m_zoomSensorWidthSpin, 1, 1);

    m_zoomCalculatedLabel = new QLabel("f = 0.0 px");
    m_zoomCalculatedLabel->setStyleSheet("color:#00ffaa; font-weight:bold; font-family:monospace;");
    zoomGrid->addWidget(m_zoomCalculatedLabel, 1, 2, 1, 2);
    opticsLayout->addWidget(zoomBox);

    m_tabs->addTab(opticsTab, "Optics & Targets");

    auto *centerTab = new QWidget(this);
    auto *centerLayout = new QVBoxLayout(centerTab);

    auto *panCenterBox = new QGroupBox("PAN Camera Optical Center");
    auto *panCenterGrid = new QGridLayout(panCenterBox);
    panCenterGrid->addWidget(new QLabel("Center X (0-3840 px):"), 0, 0);
    m_panCenterXSlider = new QSlider(Qt::Horizontal, this);
    m_panCenterXSlider->setRange(0, 3840);
    m_panCenterXSlider->setValue(m_panCenterX);
    panCenterGrid->addWidget(m_panCenterXSlider, 0, 1);
    m_panCenterXSpin = new QSpinBox(this);
    m_panCenterXSpin->setRange(0, 3840);
    m_panCenterXSpin->setValue(m_panCenterX);
    panCenterGrid->addWidget(m_panCenterXSpin, 0, 2);

    panCenterGrid->addWidget(new QLabel("Center Y (0-2160 px):"), 1, 0);
    m_panCenterYSlider = new QSlider(Qt::Horizontal, this);
    m_panCenterYSlider->setRange(0, 2160);
    m_panCenterYSlider->setValue(m_panCenterY);
    panCenterGrid->addWidget(m_panCenterYSlider, 1, 1);
    m_panCenterYSpin = new QSpinBox(this);
    m_panCenterYSpin->setRange(0, 2160);
    m_panCenterYSpin->setValue(m_panCenterY);
    panCenterGrid->addWidget(m_panCenterYSpin, 1, 2);

    m_panCenterValLabel = new QLabel(QString("(%1, %2)").arg(m_panCenterX).arg(m_panCenterY));
    m_panCenterValLabel->setStyleSheet("color:#00e5ff; font-weight:bold; font-family:monospace;");
    panCenterGrid->addWidget(m_panCenterValLabel, 2, 0, 1, 3, Qt::AlignCenter);
    centerLayout->addWidget(panCenterBox);

    auto *zoomCenterBox = new QGroupBox("ZOOM Camera Optical Center");
    auto *zoomCenterGrid = new QGridLayout(zoomCenterBox);
    zoomCenterGrid->addWidget(new QLabel("Center X (0-3840 px):"), 0, 0);
    m_zoomCenterXSlider = new QSlider(Qt::Horizontal, this);
    m_zoomCenterXSlider->setRange(0, 3840);
    m_zoomCenterXSlider->setValue(m_zoomCenterX);
    zoomCenterGrid->addWidget(m_zoomCenterXSlider, 0, 1);
    m_zoomCenterXSpin = new QSpinBox(this);
    m_zoomCenterXSpin->setRange(0, 3840);
    m_zoomCenterXSpin->setValue(m_zoomCenterX);
    zoomCenterGrid->addWidget(m_zoomCenterXSpin, 0, 2);

    zoomCenterGrid->addWidget(new QLabel("Center Y (0-2160 px):"), 1, 0);
    m_zoomCenterYSlider = new QSlider(Qt::Horizontal, this);
    m_zoomCenterYSlider->setRange(0, 2160);
    m_zoomCenterYSlider->setValue(m_zoomCenterY);
    zoomCenterGrid->addWidget(m_zoomCenterYSlider, 1, 1);
    m_zoomCenterYSpin = new QSpinBox(this);
    m_zoomCenterYSpin->setRange(0, 2160);
    m_zoomCenterYSpin->setValue(m_zoomCenterY);
    zoomCenterGrid->addWidget(m_zoomCenterYSpin, 1, 2);

    m_zoomCenterValLabel = new QLabel(QString("(%1, %2)").arg(m_zoomCenterX).arg(m_zoomCenterY));
    m_zoomCenterValLabel->setStyleSheet("color:#00ffaa; font-weight:bold; font-family:monospace;");
    zoomCenterGrid->addWidget(m_zoomCenterValLabel, 2, 0, 1, 3, Qt::AlignCenter);
    centerLayout->addWidget(zoomCenterBox);

    m_resetCenterBtn = new QPushButton("Reset Both Centers to (320, 240)", this);
    centerLayout->addWidget(m_resetCenterBtn);
    centerLayout->addStretch();
    m_tabs->addTab(centerTab, "Center Alignment");

    auto *pidTab = new QWidget(this);
    auto *pidLayout = new QVBoxLayout(pidTab);

    auto *gainsBox = new QGroupBox("PID Control Gains & Deadband");
    auto *gainsGrid = new QGridLayout(gainsBox);

    gainsGrid->addWidget(new QLabel("Kp (Proportional):"), 0, 0);
    m_kpSlider = new QSlider(Qt::Horizontal, this);
    m_kpSlider->setRange(10, 500);
    m_kpSlider->setValue(static_cast<int>(m_pidKp * 100));
    gainsGrid->addWidget(m_kpSlider, 0, 1);
    m_kpValLabel = new QLabel(QString::number(m_pidKp, 'f', 2));
    m_kpValLabel->setStyleSheet("color:#ffcc00; font-weight:bold; font-family:monospace; min-width:45px;");
    gainsGrid->addWidget(m_kpValLabel, 0, 2);

    gainsGrid->addWidget(new QLabel("Ki (Integral):"), 1, 0);
    m_kiSlider = new QSlider(Qt::Horizontal, this);
    m_kiSlider->setRange(0, 200);
    m_kiSlider->setValue(static_cast<int>(m_pidKi * 1000));
    gainsGrid->addWidget(m_kiSlider, 1, 1);
    m_kiValLabel = new QLabel(QString::number(m_pidKi, 'f', 3));
    m_kiValLabel->setStyleSheet("color:#ffcc00; font-weight:bold; font-family:monospace; min-width:45px;");
    gainsGrid->addWidget(m_kiValLabel, 1, 2);

    gainsGrid->addWidget(new QLabel("Kd (Derivative):"), 2, 0);
    m_kdSlider = new QSlider(Qt::Horizontal, this);
    m_kdSlider->setRange(0, 100);
    m_kdSlider->setValue(static_cast<int>(m_pidKd * 100));
    gainsGrid->addWidget(m_kdSlider, 2, 1);
    m_kdValLabel = new QLabel(QString::number(m_pidKd, 'f', 2));
    m_kdValLabel->setStyleSheet("color:#ffcc00; font-weight:bold; font-family:monospace; min-width:45px;");
    gainsGrid->addWidget(m_kdValLabel, 2, 2);

    gainsGrid->addWidget(new QLabel("Deadband:"), 3, 0);
    m_deadbandSlider = new QSlider(Qt::Horizontal, this);
    m_deadbandSlider->setRange(1, 20);
    m_deadbandSlider->setValue(static_cast<int>(m_pidDeadband * 100));
    gainsGrid->addWidget(m_deadbandSlider, 3, 1);
    m_deadbandValLabel = new QLabel(QString::number(m_pidDeadband, 'f', 2));
    m_deadbandValLabel->setStyleSheet("color:#ffcc00; font-weight:bold; font-family:monospace; min-width:45px;");
    gainsGrid->addWidget(m_deadbandValLabel, 3, 2);
    pidLayout->addWidget(gainsBox);

    auto *speedBox = new QGroupBox("Autonomous Speed Range (Delay us)");
    auto *speedGrid = new QGridLayout(speedBox);

    speedGrid->addWidget(new QLabel("Auto Min Delay (Max Speed):"), 0, 0);
    m_autoMinSpeedSlider = new QSlider(Qt::Horizontal, this);
    m_autoMinSpeedSlider->setRange(200, 1500);
    m_autoMinSpeedSlider->setSingleStep(50);
    m_autoMinSpeedSlider->setValue(m_autoMinSpeed);
    speedGrid->addWidget(m_autoMinSpeedSlider, 0, 1);
    m_autoMinSpeedValLabel = new QLabel(QString("%1 us").arg(m_autoMinSpeed));
    m_autoMinSpeedValLabel->setStyleSheet("color:#00ffaa; font-weight:bold; font-family:monospace; min-width:60px;");
    speedGrid->addWidget(m_autoMinSpeedValLabel, 0, 2);

    speedGrid->addWidget(new QLabel("Auto Max Delay (Min Speed):"), 1, 0);
    m_autoMaxSpeedSlider = new QSlider(Qt::Horizontal, this);
    m_autoMaxSpeedSlider->setRange(1500, 4000);
    m_autoMaxSpeedSlider->setSingleStep(50);
    m_autoMaxSpeedSlider->setValue(m_autoMaxSpeed);
    speedGrid->addWidget(m_autoMaxSpeedSlider, 1, 1);
    m_autoMaxSpeedValLabel = new QLabel(QString("%1 us").arg(m_autoMaxSpeed));
    m_autoMaxSpeedValLabel->setStyleSheet("color:#00e5ff; font-weight:bold; font-family:monospace; min-width:60px;");
    speedGrid->addWidget(m_autoMaxSpeedValLabel, 1, 2);
    pidLayout->addWidget(speedBox);

    m_resetPidBtn = new QPushButton("Reset PID to Defaults (Kp=1.0, Kd=0.35, Db=0.04, 500-3200 us)", this);
    pidLayout->addWidget(m_resetPidBtn);
    pidLayout->addStretch();
    m_tabs->addTab(pidTab, "PID Tracking");

    auto *resTab = new QWidget(this);
    auto *resLayout = new QVBoxLayout(resTab);

    auto *capBox = new QGroupBox("Camera Hardware Capture Resolution (V4L2)");
    auto *capGrid = new QGridLayout(capBox);

    capGrid->addWidget(new QLabel("PAN Capture W x H:"), 0, 0);
    auto *panCapHLayout = new QHBoxLayout;
    m_panCapWSpin = new QSpinBox(this);
    m_panCapWSpin->setRange(160, 3840);
    m_panCapWSpin->setSingleStep(16);
    m_panCapWSpin->setValue(m_panCapW);
    panCapHLayout->addWidget(m_panCapWSpin);
    panCapHLayout->addWidget(new QLabel("x"));
    m_panCapHSpin = new QSpinBox(this);
    m_panCapHSpin->setRange(120, 2160);
    m_panCapHSpin->setSingleStep(16);
    m_panCapHSpin->setValue(m_panCapH);
    panCapHLayout->addWidget(m_panCapHSpin);
    capGrid->addLayout(panCapHLayout, 0, 1);

    m_panCapPresetCombo = new QComboBox(this);
    for (int i = 0; i < g_numCapPresets; ++i) {
        m_panCapPresetCombo->addItem(g_capPresets[i].name);
    }
    capGrid->addWidget(m_panCapPresetCombo, 0, 2);

    capGrid->addWidget(new QLabel("ZOOM Capture W x H:"), 1, 0);
    auto *zoomCapHLayout = new QHBoxLayout;
    m_zoomCapWSpin = new QSpinBox(this);
    m_zoomCapWSpin->setRange(160, 3840);
    m_zoomCapWSpin->setSingleStep(16);
    m_zoomCapWSpin->setValue(m_zoomCapW);
    zoomCapHLayout->addWidget(m_zoomCapWSpin);
    zoomCapHLayout->addWidget(new QLabel("x"));
    m_zoomCapHSpin = new QSpinBox(this);
    m_zoomCapHSpin->setRange(120, 2160);
    m_zoomCapHSpin->setSingleStep(16);
    m_zoomCapHSpin->setValue(m_zoomCapH);
    zoomCapHLayout->addWidget(m_zoomCapHSpin);
    capGrid->addLayout(zoomCapHLayout, 1, 1);

    m_zoomCapPresetCombo = new QComboBox(this);
    for (int i = 0; i < g_numCapPresets; ++i) {
        m_zoomCapPresetCombo->addItem(g_capPresets[i].name);
    }
    capGrid->addWidget(m_zoomCapPresetCombo, 1, 2);
    resLayout->addWidget(capBox);

    auto *dispBox = new QGroupBox("Display Viewport Resolution (GUI Layout)");
    auto *dispGrid = new QGridLayout(dispBox);

    dispGrid->addWidget(new QLabel("PAN Display W x H:"), 0, 0);
    auto *panDispHLayout = new QHBoxLayout;
    m_panDispWSpin = new QSpinBox(this);
    m_panDispWSpin->setRange(160, 1920);
    m_panDispWSpin->setSingleStep(16);
    m_panDispWSpin->setValue(m_panDispW);
    panDispHLayout->addWidget(m_panDispWSpin);
    panDispHLayout->addWidget(new QLabel("x"));
    m_panDispHSpin = new QSpinBox(this);
    m_panDispHSpin->setRange(120, 1080);
    m_panDispHSpin->setSingleStep(16);
    m_panDispHSpin->setValue(m_panDispH);
    panDispHLayout->addWidget(m_panDispHSpin);
    dispGrid->addLayout(panDispHLayout, 0, 1);

    m_panDispPresetCombo = new QComboBox(this);
    for (int i = 0; i < g_numDispPresets; ++i) {
        m_panDispPresetCombo->addItem(g_dispPresets[i].name);
    }
    dispGrid->addWidget(m_panDispPresetCombo, 0, 2);

    dispGrid->addWidget(new QLabel("ZOOM Display W x H:"), 1, 0);
    auto *zoomDispHLayout = new QHBoxLayout;
    m_zoomDispWSpin = new QSpinBox(this);
    m_zoomDispWSpin->setRange(160, 1920);
    m_zoomDispWSpin->setSingleStep(16);
    m_zoomDispWSpin->setValue(m_zoomDispW);
    zoomDispHLayout->addWidget(m_zoomDispWSpin);
    zoomDispHLayout->addWidget(new QLabel("x"));
    m_zoomDispHSpin = new QSpinBox(this);
    m_zoomDispHSpin->setRange(120, 1080);
    m_zoomDispHSpin->setSingleStep(16);
    m_zoomDispHSpin->setValue(m_zoomDispH);
    zoomDispHLayout->addWidget(m_zoomDispHSpin);
    dispGrid->addLayout(zoomDispHLayout, 1, 1);

    m_zoomDispPresetCombo = new QComboBox(this);
    for (int i = 0; i < g_numDispPresets; ++i) {
        m_zoomDispPresetCombo->addItem(g_dispPresets[i].name);
    }
    dispGrid->addWidget(m_zoomDispPresetCombo, 1, 2);
    resLayout->addWidget(dispBox);

    m_resetResolutionsBtn = new QPushButton("Reset Resolutions to Defaults (Cap: 640x480, Disp: 320x240)", this);
    resLayout->addWidget(m_resetResolutionsBtn);
    resLayout->addStretch();
    m_tabs->addTab(resTab, "Resolution & View");

    mainLayout->addWidget(m_tabs, 1);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    m_saveBtn = new QPushButton("Save & Close", this);
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_addBtn, &QPushButton::clicked, this, &SettingsDialog::onAddRow);
    connect(m_delBtn, &QPushButton::clicked, this, &SettingsDialog::onDeleteRow);
    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);

    connect(m_panSensorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onPanSensorPresetChanged);
    connect(m_zoomSensorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onZoomSensorPresetChanged);

    connect(m_panFocalMmSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SettingsDialog::updateCalculatedFocalLabels);
    connect(m_panSensorWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SettingsDialog::updateCalculatedFocalLabels);

    connect(m_zoomFocalMmSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SettingsDialog::updateCalculatedFocalLabels);
    connect(m_zoomSensorWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SettingsDialog::updateCalculatedFocalLabels);

    connect(m_panCenterXSlider, &QSlider::valueChanged, this, &SettingsDialog::onCenterSlidersChanged);
    connect(m_panCenterYSlider, &QSlider::valueChanged, this, &SettingsDialog::onCenterSlidersChanged);
    connect(m_zoomCenterXSlider, &QSlider::valueChanged, this, &SettingsDialog::onCenterSlidersChanged);
    connect(m_zoomCenterYSlider, &QSlider::valueChanged, this, &SettingsDialog::onCenterSlidersChanged);

    connect(m_panCenterXSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_panCenterXSlider && m_panCenterXSlider->value() != v) m_panCenterXSlider->setValue(v);
    });
    connect(m_panCenterYSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_panCenterYSlider && m_panCenterYSlider->value() != v) m_panCenterYSlider->setValue(v);
    });
    connect(m_zoomCenterXSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_zoomCenterXSlider && m_zoomCenterXSlider->value() != v) m_zoomCenterXSlider->setValue(v);
    });
    connect(m_zoomCenterYSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_zoomCenterYSlider && m_zoomCenterYSlider->value() != v) m_zoomCenterYSlider->setValue(v);
    });

    connect(m_resetCenterBtn, &QPushButton::clicked, this, &SettingsDialog::onResetCenterClicked);

    connect(m_kpSlider, &QSlider::valueChanged, this, &SettingsDialog::onPidSlidersChanged);
    connect(m_kiSlider, &QSlider::valueChanged, this, &SettingsDialog::onPidSlidersChanged);
    connect(m_kdSlider, &QSlider::valueChanged, this, &SettingsDialog::onPidSlidersChanged);
    connect(m_deadbandSlider, &QSlider::valueChanged, this, &SettingsDialog::onPidSlidersChanged);
    connect(m_autoMinSpeedSlider, &QSlider::valueChanged, this, &SettingsDialog::onPidSlidersChanged);
    connect(m_autoMaxSpeedSlider, &QSlider::valueChanged, this, &SettingsDialog::onPidSlidersChanged);
    connect(m_resetPidBtn, &QPushButton::clicked, this, &SettingsDialog::onResetPidClicked);

    connect(m_panCapPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onPanCapPresetChanged);
    connect(m_zoomCapPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onZoomCapPresetChanged);
    connect(m_panDispPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onPanDispPresetChanged);
    connect(m_zoomDispPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onZoomDispPresetChanged);

    connect(m_panCapWSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        m_panCapPresetCombo->blockSignals(true);
        m_panCapPresetCombo->setCurrentIndex(g_numCapPresets - 1);
        m_panCapPresetCombo->blockSignals(false);
    });
    connect(m_panCapHSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        m_panCapPresetCombo->blockSignals(true);
        m_panCapPresetCombo->setCurrentIndex(g_numCapPresets - 1);
        m_panCapPresetCombo->blockSignals(false);
    });
    connect(m_zoomCapWSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        m_zoomCapPresetCombo->blockSignals(true);
        m_zoomCapPresetCombo->setCurrentIndex(g_numCapPresets - 1);
        m_zoomCapPresetCombo->blockSignals(false);
    });
    connect(m_zoomCapHSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        m_zoomCapPresetCombo->blockSignals(true);
        m_zoomCapPresetCombo->setCurrentIndex(g_numCapPresets - 1);
        m_zoomCapPresetCombo->blockSignals(false);
    });

    connect(m_panDispWSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        m_panDispPresetCombo->blockSignals(true);
        m_panDispPresetCombo->setCurrentIndex(g_numDispPresets - 1);
        m_panDispPresetCombo->blockSignals(false);
    });
    connect(m_panDispHSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        m_panDispPresetCombo->blockSignals(true);
        m_panDispPresetCombo->setCurrentIndex(g_numDispPresets - 1);
        m_panDispPresetCombo->blockSignals(false);
    });
    connect(m_zoomDispWSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        m_zoomDispPresetCombo->blockSignals(true);
        m_zoomDispPresetCombo->setCurrentIndex(g_numDispPresets - 1);
        m_zoomDispPresetCombo->blockSignals(false);
    });
    connect(m_zoomDispHSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        m_zoomDispPresetCombo->blockSignals(true);
        m_zoomDispPresetCombo->setCurrentIndex(g_numDispPresets - 1);
        m_zoomDispPresetCombo->blockSignals(false);
    });

    connect(m_resetResolutionsBtn, &QPushButton::clicked, this, &SettingsDialog::onResetResolutionsClicked);

    updateCalculatedFocalLabels();

    setStyleSheet(
        "QDialog { background-color: #1e1e1e; color: #eee; }"
        "QLabel { color: #eee; font-weight: bold; }"
        "QTabWidget::pane { border: 1px solid #3e3e3e; background: #222; border-radius: 4px; }"
        "QTabBar::tab { background: #2b2b2b; color: #ccc; padding: 6px 14px; border: 1px solid #444; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background: #3a3a3a; color: #fff; font-weight: bold; }"
        "QGroupBox { border: 1px solid #3e3e3e; border-radius: 6px; margin-top: 12px; padding-top: 12px; font-weight: bold; color: #aaa; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 4px; }"
        "QTableWidget { background-color: #262626; color: #eee; gridline-color: #3e3e3e; border: 1px solid #444; }"
        "QHeaderView::section { background-color: #333; color: #eee; padding: 4px; border: 1px solid #444; }"
        "QPushButton { background: #2d2d2d; color: #eee; border: 1px solid #555; border-radius: 4px; padding: 5px 12px; }"
        "QPushButton:hover { background: #3d3d3d; }"
        "QComboBox { background: #262626; color: #eee; padding: 4px; border: 1px solid #444; border-radius: 4px; }"
        "QDoubleSpinBox, QSpinBox { background: #262626; color: #eee; border: 1px solid #444; border-radius: 4px; padding: 2px 4px; }"
        "QSlider::groove:horizontal { height: 6px; background: #333; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #0077ff; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #eee; border: 1px solid #777; width: 14px; margin-top: -4px; margin-bottom: -4px; border-radius: 7px; }"
    );
}

double SettingsDialog::panFocalPx(int imageWidth) const {
    double sw = std::max(0.01, m_panSensorWidthMm);
    return (m_panFocalMm * imageWidth) / sw;
}

double SettingsDialog::zoomFocalPx(int imageWidth) const {
    double sw = std::max(0.01, m_zoomSensorWidthMm);
    return (m_zoomFocalMm * imageWidth) / sw;
}

void SettingsDialog::updateCalculatedFocalLabels() {
    m_panFocalMm = m_panFocalMmSpin->value();
    m_panSensorWidthMm = m_panSensorWidthSpin->value();
    m_zoomFocalMm = m_zoomFocalMmSpin->value();
    m_zoomSensorWidthMm = m_zoomSensorWidthSpin->value();

    double panPx = panFocalPx(m_panDispW);
    double zoomPx = zoomFocalPx(m_zoomDispW);

    if (m_panCalculatedLabel) {
        m_panCalculatedLabel->setText(QString("Calculated f: %1 px").arg(panPx, 0, 'f', 1));
    }
    if (m_zoomCalculatedLabel) {
        m_zoomCalculatedLabel->setText(QString("Calculated f: %1 px").arg(zoomPx, 0, 'f', 1));
    }
}

void SettingsDialog::onPanSensorPresetChanged(int index) {
    if (index >= 0 && index < g_numSensorPresets - 1) {
        m_panSensorWidthSpin->blockSignals(true);
        m_panSensorWidthSpin->setValue(g_sensorPresets[index].widthMm);
        m_panSensorWidthSpin->blockSignals(false);
        updateCalculatedFocalLabels();
    }
}

void SettingsDialog::onZoomSensorPresetChanged(int index) {
    if (index >= 0 && index < g_numSensorPresets - 1) {
        m_zoomSensorWidthSpin->blockSignals(true);
        m_zoomSensorWidthSpin->setValue(g_sensorPresets[index].widthMm);
        m_zoomSensorWidthSpin->blockSignals(false);
        updateCalculatedFocalLabels();
    }
}

void SettingsDialog::onCenterSlidersChanged() {
    if (m_panCenterXSlider && m_panCenterXSpin) {
        m_panCenterX = m_panCenterXSlider->value();
        m_panCenterXSpin->blockSignals(true);
        m_panCenterXSpin->setValue(m_panCenterX);
        m_panCenterXSpin->blockSignals(false);
    }
    if (m_panCenterYSlider && m_panCenterYSpin) {
        m_panCenterY = m_panCenterYSlider->value();
        m_panCenterYSpin->blockSignals(true);
        m_panCenterYSpin->setValue(m_panCenterY);
        m_panCenterYSpin->blockSignals(false);
    }
    if (m_panCenterValLabel) {
        m_panCenterValLabel->setText(QString("(%1, %2)").arg(m_panCenterX).arg(m_panCenterY));
    }

    if (m_zoomCenterXSlider && m_zoomCenterXSpin) {
        m_zoomCenterX = m_zoomCenterXSlider->value();
        m_zoomCenterXSpin->blockSignals(true);
        m_zoomCenterXSpin->setValue(m_zoomCenterX);
        m_zoomCenterXSpin->blockSignals(false);
    }
    if (m_zoomCenterYSlider && m_zoomCenterYSpin) {
        m_zoomCenterY = m_zoomCenterYSlider->value();
        m_zoomCenterYSpin->blockSignals(true);
        m_zoomCenterYSpin->setValue(m_zoomCenterY);
        m_zoomCenterYSpin->blockSignals(false);
    }
    if (m_zoomCenterValLabel) {
        m_zoomCenterValLabel->setText(QString("(%1, %2)").arg(m_zoomCenterX).arg(m_zoomCenterY));
    }
}

void SettingsDialog::onResetCenterClicked() {
    m_panCenterX = 320;
    m_panCenterY = 240;
    m_zoomCenterX = 320;
    m_zoomCenterY = 240;
    if (m_panCenterXSlider) m_panCenterXSlider->setValue(320);
    if (m_panCenterYSlider) m_panCenterYSlider->setValue(240);
    if (m_zoomCenterXSlider) m_zoomCenterXSlider->setValue(320);
    if (m_zoomCenterYSlider) m_zoomCenterYSlider->setValue(240);
    onCenterSlidersChanged();
}

void SettingsDialog::onPidSlidersChanged() {
    if (m_kpSlider) {
        m_pidKp = m_kpSlider->value() / 100.0;
        if (m_kpValLabel) m_kpValLabel->setText(QString::number(m_pidKp, 'f', 2));
    }
    if (m_kiSlider) {
        m_pidKi = m_kiSlider->value() / 1000.0;
        if (m_kiValLabel) m_kiValLabel->setText(QString::number(m_pidKi, 'f', 3));
    }
    if (m_kdSlider) {
        m_pidKd = m_kdSlider->value() / 100.0;
        if (m_kdValLabel) m_kdValLabel->setText(QString::number(m_pidKd, 'f', 2));
    }
    if (m_deadbandSlider) {
        m_pidDeadband = m_deadbandSlider->value() / 100.0;
        if (m_deadbandValLabel) m_deadbandValLabel->setText(QString::number(m_pidDeadband, 'f', 2));
    }
    if (m_autoMinSpeedSlider) {
        m_autoMinSpeed = static_cast<uint16_t>(m_autoMinSpeedSlider->value());
        if (m_autoMinSpeedValLabel) m_autoMinSpeedValLabel->setText(QString("%1 us").arg(m_autoMinSpeed));
    }
    if (m_autoMaxSpeedSlider) {
        m_autoMaxSpeed = static_cast<uint16_t>(m_autoMaxSpeedSlider->value());
        if (m_autoMaxSpeedValLabel) m_autoMaxSpeedValLabel->setText(QString("%1 us").arg(m_autoMaxSpeed));
    }
}

void SettingsDialog::onResetPidClicked() {
    m_pidKp = 1.0;
    m_pidKi = 0.02;
    m_pidKd = 0.35;
    m_pidDeadband = 0.04;
    m_autoMinSpeed = 500;
    m_autoMaxSpeed = 3200;

    if (m_kpSlider) m_kpSlider->setValue(100);
    if (m_kiSlider) m_kiSlider->setValue(20);
    if (m_kdSlider) m_kdSlider->setValue(35);
    if (m_deadbandSlider) m_deadbandSlider->setValue(4);
    if (m_autoMinSpeedSlider) m_autoMinSpeedSlider->setValue(500);
    if (m_autoMaxSpeedSlider) m_autoMaxSpeedSlider->setValue(3200);
    onPidSlidersChanged();
}

void SettingsDialog::onPanCapPresetChanged(int index) {
    if (index >= 0 && index < g_numCapPresets - 1) {
        m_panCapWSpin->blockSignals(true);
        m_panCapHSpin->blockSignals(true);
        m_panCapWSpin->setValue(g_capPresets[index].width);
        m_panCapHSpin->setValue(g_capPresets[index].height);
        m_panCapWSpin->blockSignals(false);
        m_panCapHSpin->blockSignals(false);
    }
}

void SettingsDialog::onZoomCapPresetChanged(int index) {
    if (index >= 0 && index < g_numCapPresets - 1) {
        m_zoomCapWSpin->blockSignals(true);
        m_zoomCapHSpin->blockSignals(true);
        m_zoomCapWSpin->setValue(g_capPresets[index].width);
        m_zoomCapHSpin->setValue(g_capPresets[index].height);
        m_zoomCapWSpin->blockSignals(false);
        m_zoomCapHSpin->blockSignals(false);
    }
}

void SettingsDialog::onPanDispPresetChanged(int index) {
    if (index >= 0 && index < g_numDispPresets - 1) {
        m_panDispWSpin->blockSignals(true);
        m_panDispHSpin->blockSignals(true);
        m_panDispWSpin->setValue(g_dispPresets[index].width);
        m_panDispHSpin->setValue(g_dispPresets[index].height);
        m_panDispWSpin->blockSignals(false);
        m_panDispHSpin->blockSignals(false);
        updateCalculatedFocalLabels();
    }
}

void SettingsDialog::onZoomDispPresetChanged(int index) {
    if (index >= 0 && index < g_numDispPresets - 1) {
        m_zoomDispWSpin->blockSignals(true);
        m_zoomDispHSpin->blockSignals(true);
        m_zoomDispWSpin->setValue(g_dispPresets[index].width);
        m_zoomDispHSpin->setValue(g_dispPresets[index].height);
        m_zoomDispWSpin->blockSignals(false);
        m_zoomDispHSpin->blockSignals(false);
        updateCalculatedFocalLabels();
    }
}

void SettingsDialog::onResetResolutionsClicked() {
    m_panCapW = 640;
    m_panCapH = 480;
    m_zoomCapW = 640;
    m_zoomCapH = 480;
    m_panDispW = 320;
    m_panDispH = 240;
    m_zoomDispW = 320;
    m_zoomDispH = 240;

    if (m_panCapWSpin) m_panCapWSpin->setValue(640);
    if (m_panCapHSpin) m_panCapHSpin->setValue(480);
    if (m_zoomCapWSpin) m_zoomCapWSpin->setValue(640);
    if (m_zoomCapHSpin) m_zoomCapHSpin->setValue(480);

    if (m_panDispWSpin) m_panDispWSpin->setValue(320);
    if (m_panDispHSpin) m_panDispHSpin->setValue(240);
    if (m_zoomDispWSpin) m_zoomDispWSpin->setValue(320);
    if (m_zoomDispHSpin) m_zoomDispHSpin->setValue(240);

    if (m_panCapPresetCombo) m_panCapPresetCombo->setCurrentIndex(0);
    if (m_zoomCapPresetCombo) m_zoomCapPresetCombo->setCurrentIndex(0);
    if (m_panDispPresetCombo) m_panDispPresetCombo->setCurrentIndex(0);
    if (m_zoomDispPresetCombo) m_zoomDispPresetCombo->setCurrentIndex(0);
    updateCalculatedFocalLabels();
}

void SettingsDialog::populateTable() {
    m_table->setRowCount(0);
    for (int i = 0; i < m_droneClasses.size(); ++i) {
        int r = m_table->rowCount();
        m_table->insertRow(r);
        auto *idItem = new QTableWidgetItem(QString::number(m_droneClasses[i].classId));
        auto *nameItem = new QTableWidgetItem(m_droneClasses[i].className);
        auto *wItem = new QTableWidgetItem(QString::number(m_droneClasses[i].wReal, 'f', 3));
        m_table->setItem(r, 0, idItem);
        m_table->setItem(r, 1, nameItem);
        m_table->setItem(r, 2, wItem);
    }
}

void SettingsDialog::onAddRow() {
    int r = m_table->rowCount();
    m_table->insertRow(r);
    m_table->setItem(r, 0, new QTableWidgetItem(QString::number(r)));
    m_table->setItem(r, 1, new QTableWidgetItem(QString("drone_%1").arg(r)));
    m_table->setItem(r, 2, new QTableWidgetItem("0.350"));
}

void SettingsDialog::onDeleteRow() {
    int r = m_table->currentRow();
    if (r >= 0) {
        m_table->removeRow(r);
    }
}

void SettingsDialog::onSaveClicked() {
    {
        std::lock_guard<std::mutex> lock(m_classMutex);
        m_droneClasses.clear();
        for (int r = 0; r < m_table->rowCount(); ++r) {
            DroneClassInfo info;
            auto *idItem = m_table->item(r, 0);
            auto *nameItem = m_table->item(r, 1);
            auto *wItem = m_table->item(r, 2);
            if (idItem) info.classId = idItem->text().toInt();
            if (nameItem) info.className = nameItem->text();
            if (wItem) info.wReal = wItem->text().toDouble();
            if (info.wReal <= 0.001) info.wReal = 0.35;
            m_droneClasses.append(info);
        }
    }
    m_panFocalMm = m_panFocalMmSpin->value();
    m_panSensorWidthMm = m_panSensorWidthSpin->value();
    m_zoomFocalMm = m_zoomFocalMmSpin->value();
    m_zoomSensorWidthMm = m_zoomSensorWidthSpin->value();

    if (m_panCenterXSlider) m_panCenterX = m_panCenterXSlider->value();
    if (m_panCenterYSlider) m_panCenterY = m_panCenterYSlider->value();
    if (m_zoomCenterXSlider) m_zoomCenterX = m_zoomCenterXSlider->value();
    if (m_zoomCenterYSlider) m_zoomCenterYSlider->value();

    if (m_kpSlider) m_pidKp = m_kpSlider->value() / 100.0;
    if (m_kiSlider) m_pidKi = m_kiSlider->value() / 1000.0;
    if (m_kdSlider) m_pidKd = m_kdSlider->value() / 100.0;
    if (m_deadbandSlider) m_pidDeadband = m_deadbandSlider->value() / 100.0;
    if (m_autoMinSpeedSlider) m_autoMinSpeed = static_cast<uint16_t>(m_autoMinSpeedSlider->value());
    if (m_autoMaxSpeedSlider) m_autoMaxSpeed = static_cast<uint16_t>(m_autoMaxSpeedSlider->value());

    if (m_panCapWSpin) m_panCapW = m_panCapWSpin->value();
    if (m_panCapHSpin) m_panCapH = m_panCapHSpin->value();
    if (m_zoomCapWSpin) m_zoomCapW = m_zoomCapWSpin->value();
    if (m_zoomCapHSpin) m_zoomCapH = m_zoomCapHSpin->value();

    if (m_panDispWSpin) m_panDispW = m_panDispWSpin->value();
    if (m_panDispHSpin) m_panDispH = m_panDispHSpin->value();
    if (m_zoomDispWSpin) m_zoomDispW = m_zoomDispWSpin->value();
    if (m_zoomDispHSpin) m_zoomDispH = m_zoomDispHSpin->value();

    saveSettings(m_settingsFilePath);
    accept();
}

double SettingsDialog::getWRealForClass(int classId, const QString &className) const {
    std::lock_guard<std::mutex> lock(m_classMutex);
    for (const auto &c : m_droneClasses) {
        if (c.classId == classId) return c.wReal;
        if (!className.isEmpty() && c.className.compare(className, Qt::CaseInsensitive) == 0) return c.wReal;
    }
    if (!m_droneClasses.isEmpty()) return m_droneClasses.first().wReal;
    return 0.35;
}

bool SettingsDialog::isClassMapped(int classId) const {
    std::lock_guard<std::mutex> lock(m_classMutex);
    for (const auto &c : m_droneClasses) {
        if (c.classId == classId) return true;
    }
    return false;
}

void SettingsDialog::loadSettings(const QString &filePath) {
    m_settingsFilePath = filePath;
    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        {
            std::lock_guard<std::mutex> lock(m_classMutex);
            m_droneClasses.clear();
            m_droneClasses.append(DroneClassInfo{0, "phantom", 0.350});
            m_droneClasses.append(DroneClassInfo{1, "mavic", 0.183});
        }
        m_panFocalMm = 5.0;
        m_panSensorWidthMm = 5.76;
        m_zoomFocalMm = 50.0;
        m_zoomSensorWidthMm = 5.76;

        m_panCenterX = 320;
        m_panCenterY = 240;
        m_zoomCenterX = 320;
        m_zoomCenterY = 240;

        m_pidKp = 1.0;
        m_pidKi = 0.02;
        m_pidKd = 0.35;
        m_pidDeadband = 0.04;
        m_autoMinSpeed = 500;
        m_autoMaxSpeed = 3200;

        m_panCapW = 640;
        m_panCapH = 480;
        m_zoomCapW = 640;
        m_zoomCapH = 480;

        m_panDispW = 320;
        m_panDispH = 240;
        m_zoomDispW = 320;
        m_zoomDispH = 240;

        if (m_panFocalMmSpin) m_panFocalMmSpin->setValue(m_panFocalMm);
        if (m_panSensorWidthSpin) m_panSensorWidthSpin->setValue(m_panSensorWidthMm);
        if (m_zoomFocalMmSpin) m_zoomFocalMmSpin->setValue(m_zoomFocalMm);
        if (m_zoomSensorWidthSpin) m_zoomSensorWidthSpin->setValue(m_zoomSensorWidthMm);

        if (m_panCenterXSlider) m_panCenterXSlider->setValue(m_panCenterX);
        if (m_panCenterYSlider) m_panCenterYSlider->setValue(m_panCenterY);
        if (m_zoomCenterXSlider) m_zoomCenterXSlider->setValue(m_zoomCenterX);
        if (m_zoomCenterYSlider) m_zoomCenterYSlider->setValue(m_zoomCenterY);

        if (m_kpSlider) m_kpSlider->setValue(static_cast<int>(m_pidKp * 100));
        if (m_kiSlider) m_kiSlider->setValue(static_cast<int>(m_pidKi * 1000));
        if (m_kdSlider) m_kdSlider->setValue(static_cast<int>(m_pidKd * 100));
        if (m_deadbandSlider) m_deadbandSlider->setValue(static_cast<int>(m_pidDeadband * 100));
        if (m_autoMinSpeedSlider) m_autoMinSpeedSlider->setValue(m_autoMinSpeed);
        if (m_autoMaxSpeedSlider) m_autoMaxSpeedSlider->setValue(m_autoMaxSpeed);

        if (m_panCapWSpin) m_panCapWSpin->setValue(m_panCapW);
        if (m_panCapHSpin) m_panCapHSpin->setValue(m_panCapH);
        if (m_zoomCapWSpin) m_zoomCapWSpin->setValue(m_zoomCapW);
        if (m_zoomCapHSpin) m_zoomCapHSpin->setValue(m_zoomCapH);

        if (m_panDispWSpin) m_panDispWSpin->setValue(m_panDispW);
        if (m_panDispHSpin) m_panDispHSpin->setValue(m_panDispH);
        if (m_zoomDispWSpin) m_zoomDispWSpin->setValue(m_zoomDispW);
        if (m_zoomDispHSpin) m_zoomDispHSpin->setValue(m_zoomDispH);

        updateCalculatedFocalLabels();
        onCenterSlidersChanged();
        onPidSlidersChanged();
        populateTable();
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) return;

    QJsonObject obj = doc.object();
    if (obj.contains("pan_focal_mm")) m_panFocalMm = obj["pan_focal_mm"].toDouble(5.0);
    if (obj.contains("pan_sensor_w_mm")) m_panSensorWidthMm = obj["pan_sensor_w_mm"].toDouble(5.76);
    if (obj.contains("zoom_focal_mm")) m_zoomFocalMm = obj["zoom_focal_mm"].toDouble(50.0);
    if (obj.contains("zoom_sensor_w_mm")) m_zoomSensorWidthMm = obj["zoom_sensor_w_mm"].toDouble(5.76);

    if (obj.contains("pan_center_x")) m_panCenterX = obj["pan_center_x"].toInt(320);
    if (obj.contains("pan_center_y")) m_panCenterY = obj["pan_center_y"].toInt(240);
    if (obj.contains("zoom_center_x")) m_zoomCenterX = obj["zoom_center_x"].toInt(320);
    if (obj.contains("zoom_center_y")) m_zoomCenterY = obj["zoom_center_y"].toInt(240);

    if (obj.contains("pid_kp")) m_pidKp = obj["pid_kp"].toDouble(1.0);
    if (obj.contains("pid_ki")) m_pidKi = obj["pid_ki"].toDouble(0.02);
    if (obj.contains("pid_kd")) m_pidKd = obj["pid_kd"].toDouble(0.35);
    if (obj.contains("pid_deadband")) m_pidDeadband = obj["pid_deadband"].toDouble(0.04);
    if (obj.contains("auto_min_speed")) m_autoMinSpeed = static_cast<uint16_t>(obj["auto_min_speed"].toInt(500));
    if (obj.contains("auto_max_speed")) m_autoMaxSpeed = static_cast<uint16_t>(obj["auto_max_speed"].toInt(3200));

    if (obj.contains("pan_cap_w")) m_panCapW = obj["pan_cap_w"].toInt(640);
    if (obj.contains("pan_cap_h")) m_panCapH = obj["pan_cap_h"].toInt(480);
    if (obj.contains("zoom_cap_w")) m_zoomCapW = obj["zoom_cap_w"].toInt(640);
    if (obj.contains("zoom_cap_h")) m_zoomCapH = obj["zoom_cap_h"].toInt(480);

    if (obj.contains("pan_disp_w")) m_panDispW = obj["pan_disp_w"].toInt(320);
    if (obj.contains("pan_disp_h")) m_panDispH = obj["pan_disp_h"].toInt(240);
    if (obj.contains("zoom_disp_w")) m_zoomDispW = obj["zoom_disp_w"].toInt(320);
    if (obj.contains("zoom_disp_h")) m_zoomDispH = obj["zoom_disp_h"].toInt(240);

    if (m_panFocalMmSpin) m_panFocalMmSpin->setValue(m_panFocalMm);
    if (m_panSensorWidthSpin) m_panSensorWidthSpin->setValue(m_panSensorWidthMm);
    if (m_zoomFocalMmSpin) m_zoomFocalMmSpin->setValue(m_zoomFocalMm);
    if (m_zoomSensorWidthSpin) m_zoomSensorWidthSpin->setValue(m_zoomSensorWidthMm);

    if (m_panCenterXSlider) m_panCenterXSlider->setValue(m_panCenterX);
    if (m_panCenterYSlider) m_panCenterYSlider->setValue(m_panCenterY);
    if (m_zoomCenterXSlider) m_zoomCenterXSlider->setValue(m_zoomCenterX);
    if (m_zoomCenterYSlider) m_zoomCenterYSlider->setValue(m_zoomCenterY);

    if (m_kpSlider) m_kpSlider->setValue(static_cast<int>(m_pidKp * 100));
    if (m_kiSlider) m_kiSlider->setValue(static_cast<int>(m_pidKi * 1000));
    if (m_kdSlider) m_kdSlider->setValue(static_cast<int>(m_pidKd * 100));
    if (m_deadbandSlider) m_deadbandSlider->setValue(static_cast<int>(m_pidDeadband * 100));
    if (m_autoMinSpeedSlider) m_autoMinSpeedSlider->setValue(m_autoMinSpeed);
    if (m_autoMaxSpeedSlider) m_autoMaxSpeedSlider->setValue(m_autoMaxSpeed);

    if (m_panCapWSpin) m_panCapWSpin->setValue(m_panCapW);
    if (m_panCapHSpin) m_panCapHSpin->setValue(m_panCapH);
    if (m_zoomCapWSpin) m_zoomCapWSpin->setValue(m_zoomCapW);
    if (m_zoomCapHSpin) m_zoomCapHSpin->setValue(m_zoomCapH);

    if (m_panDispWSpin) m_panDispWSpin->setValue(m_panDispW);
    if (m_panDispHSpin) m_panDispHSpin->setValue(m_panDispH);
    if (m_zoomDispWSpin) m_zoomDispWSpin->setValue(m_zoomDispW);
    if (m_zoomDispHSpin) m_zoomDispHSpin->setValue(m_zoomDispH);

    auto matchPresetIndex = [](double w) -> int {
        for (int i = 0; i < g_numSensorPresets - 1; ++i) {
            if (std::abs(g_sensorPresets[i].widthMm - w) < 0.05) return i;
        }
        return g_numSensorPresets - 1;
    };

    if (m_panSensorCombo) m_panSensorCombo->setCurrentIndex(matchPresetIndex(m_panSensorWidthMm));
    if (m_zoomSensorCombo) m_zoomSensorCombo->setCurrentIndex(matchPresetIndex(m_zoomSensorWidthMm));

    auto matchCapPresetIndex = [](int w, int h) -> int {
        for (int i = 0; i < g_numCapPresets - 1; ++i) {
            if (g_capPresets[i].width == w && g_capPresets[i].height == h) return i;
        }
        return g_numCapPresets - 1;
    };
    if (m_panCapPresetCombo) m_panCapPresetCombo->setCurrentIndex(matchCapPresetIndex(m_panCapW, m_panCapH));
    if (m_zoomCapPresetCombo) m_zoomCapPresetCombo->setCurrentIndex(matchCapPresetIndex(m_zoomCapW, m_zoomCapH));

    auto matchDispPresetIndex = [](int w, int h) -> int {
        for (int i = 0; i < g_numDispPresets - 1; ++i) {
            if (g_dispPresets[i].width == w && g_dispPresets[i].height == h) return i;
        }
        return g_numDispPresets - 1;
    };
    if (m_panDispPresetCombo) m_panDispPresetCombo->setCurrentIndex(matchDispPresetIndex(m_panDispW, m_panDispH));
    if (m_zoomDispPresetCombo) m_zoomDispPresetCombo->setCurrentIndex(matchDispPresetIndex(m_zoomDispW, m_zoomDispH));

    {
        std::lock_guard<std::mutex> lock(m_classMutex);
        m_droneClasses.clear();
        if (obj.contains("classes") && obj["classes"].isArray()) {
            QJsonArray arr = obj["classes"].toArray();
            for (const auto &val : arr) {
                if (val.isObject()) {
                    QJsonObject cObj = val.toObject();
                    DroneClassInfo info;
                    info.classId = cObj["id"].toInt();
                    info.className = cObj["name"].toString();
                    info.wReal = cObj["w_real"].toDouble(0.35);
                    m_droneClasses.append(info);
                }
            }
        }
        if (m_droneClasses.isEmpty()) {
            m_droneClasses.append(DroneClassInfo{0, "phantom", 0.350});
            m_droneClasses.append(DroneClassInfo{1, "mavic", 0.183});
        }
    }
    updateCalculatedFocalLabels();
    onCenterSlidersChanged();
    onPidSlidersChanged();
    populateTable();
}

void SettingsDialog::saveSettings(const QString &filePath) {
    QJsonObject obj;
    obj["pan_focal_mm"] = m_panFocalMm;
    obj["pan_sensor_w_mm"] = m_panSensorWidthMm;
    obj["zoom_focal_mm"] = m_zoomFocalMm;
    obj["zoom_sensor_w_mm"] = m_zoomSensorWidthMm;

    obj["pan_center_x"] = m_panCenterX;
    obj["pan_center_y"] = m_panCenterY;
    obj["zoom_center_x"] = m_zoomCenterX;
    obj["zoom_center_y"] = m_zoomCenterY;

    obj["pid_kp"] = m_pidKp;
    obj["pid_ki"] = m_pidKi;
    obj["pid_kd"] = m_pidKd;
    obj["pid_deadband"] = m_pidDeadband;
    obj["auto_min_speed"] = static_cast<int>(m_autoMinSpeed);
    obj["auto_max_speed"] = static_cast<int>(m_autoMaxSpeed);

    obj["pan_cap_w"] = m_panCapW;
    obj["pan_cap_h"] = m_panCapH;
    obj["zoom_cap_w"] = m_zoomCapW;
    obj["zoom_cap_h"] = m_zoomCapH;

    obj["pan_disp_w"] = m_panDispW;
    obj["pan_disp_h"] = m_panDispH;
    obj["zoom_disp_w"] = m_zoomDispW;
    obj["zoom_disp_h"] = m_zoomDispH;

    QJsonArray arr;
    for (const auto &c : m_droneClasses) {
        QJsonObject cObj;
        cObj["id"] = c.classId;
        cObj["name"] = c.className;
        cObj["w_real"] = c.wReal;
        arr.append(cObj);
    }
    obj["classes"] = arr;

    QJsonDocument doc(obj);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}
