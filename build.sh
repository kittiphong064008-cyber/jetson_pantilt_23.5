#!/bin/bash
set -e

echo "=========================================================="
echo "    Jetson Pan-Tilt: Automated Build Script               "
echo "=========================================================="

mkdir -p build
cd build
cmake ..
make -j$(nproc)

echo "=========================================================="
echo " Build Succeeded!"
echo " To run with USB Serial (ESP32):"
echo "   ./jetson_pantilt_ui /dev/ttyUSB0"
echo " Or with UART pins:"
echo "   ./jetson_pantilt_ui /dev/ttyTHS1"
echo "=========================================================="
