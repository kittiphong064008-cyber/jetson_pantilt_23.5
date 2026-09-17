# jetson_pantilt_21

Jetson Pan-Tilt Controller with Continuous Cumulative GPS Averaging (Stationary Survey-In), Base Relocation Recalibration, Configurable Hardware Capture & UI Viewport Resolutions, Flowchart State Machine (Dedicated Zoom Loop with Zero-Latency Handover to Pan), TensorRT YOLO object detection, PID Auto-Tracking, Optical Center Alignment, and ESP32 Sensor Telemetry.

## Key Features in v21
- **Continuous GPS Cumulative Averaging**: Welford running mean accumulating all valid GPS points while the base station remains stationary.
- **Relocate Base Station Button**: Interactive UI control to reset accumulated GPS coordinates and restart calibration at a new location.
- **Dynamic Capture & Display Resolution**: Dedicated "Resolution & View" settings tab for hardware V4L2 capture and GUI viewport display dimensions for both Pan and Zoom cameras, with presets and automatic JSON persistence.
- **Strict Zero-Comment Architecture**: Entire C++ codebase is maintained with zero comments.

## Build & Run on Jetson

### Using CMake
```bash
cd ~/Documents/GitHub/Jetson/jetson_pantilt_21
mkdir -p build && cd build
cmake ..
make -j$(nproc)
./jetson_pantilt_ui /dev/ttyUSB0
```

### Using qmake (Qt Creator)
```bash
cd ~/Documents/GitHub/Jetson/jetson_pantilt_21
qmake jetson_pantilt.pro
make -j$(nproc)
./jetson_pantilt_ui /dev/ttyUSB0
```
