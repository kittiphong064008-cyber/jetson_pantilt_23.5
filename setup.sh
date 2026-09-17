#!/bin/bash
set -e

echo "=========================================================="
echo "    Jetson Pan-Tilt: Automated System Setup Script       "
echo "=========================================================="

echo "[1/4] Updating package lists..."
sudo apt-get update

echo "[2/4] Installing compilation tools and libraries..."
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    qtbase5-dev \
    qtchooser \
    qt5-qmake \
    qtbase5-dev-tools \
    libv4l-dev \
    v4l-utils \
    libx11-dev

echo "[3/4] Configuring hardware permissions (Serial and Camera)..."
sudo usermod -aG dialout $USER
sudo usermod -aG video $USER

echo "[4/4] Setup complete!"
echo "=========================================================="
echo " System will reboot in 5 seconds to apply permissions..."
echo " Press Ctrl+C now if you want to cancel reboot."
echo "=========================================================="
sleep 5
sudo reboot
