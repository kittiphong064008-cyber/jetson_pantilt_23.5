#include "SerialLink.h"

#include <chrono>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

SerialLink::SerialLink(std::string portPath, int heartbeatMs, int reconnectRetryMs)
    : _portPath(std::move(portPath)),
      _heartbeatMs(heartbeatMs),
      _reconnectRetryMs(reconnectRetryMs) {}

SerialLink::~SerialLink() {
    Stop();
}

void SerialLink::SetSensorCallback(SensorCallback cb) {
    _sensorCallback = std::move(cb);
}

void SerialLink::SetState(const PanTiltState &state) {
    std::lock_guard<std::mutex> lock(_stateMutex);
    if (state != _state) {
        _state = state;
        _stateDirty = true;
    }
}

void SerialLink::Start() {
    if (_running) return;
    _running = true;
    _thread = std::thread(&SerialLink::ThreadLoop, this);
}

void SerialLink::Stop() {
    if (!_running) return;
    _running = false;
    if (_thread.joinable()) _thread.join();
}

bool SerialLink::OpenPort() {
    _fd = open(_portPath.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (_fd < 0) return false;

    termios tty{};
    if (tcgetattr(_fd, &tty) != 0) { ClosePort(); return false; }

    cfmakeraw(&tty);
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(_fd, TCSANOW, &tty) != 0) { ClosePort(); return false; }

    tcflush(_fd, TCIOFLUSH);
    _rxCount = 0;
    return true;
}

void SerialLink::ClosePort() {
    if (_fd >= 0) {
        close(_fd);
        _fd = -1;
    }
}

bool SerialLink::SendState(const PanTiltState &state) {
    if (_fd < 0) return false;
    auto data = _packet.Build(state);
    ssize_t n = write(_fd, data.data(), data.size());
    return n == static_cast<ssize_t>(data.size());
}

void SerialLink::FeedReceiveBuffer(const uint8_t *data, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) {
        uint8_t b = data[i];
        if (_rxCount == 0 && b != SENSOR_START) continue;
        _rxBuf[_rxCount++] = b;

        if (_rxCount == SENSOR_PKT_SIZE) {
            SensorData sd;
            if (TryDecodeSensorPacket(sd)) {
                if (_sensorCallback) {
                    _sensorCallback(sd);
                }
                _rxCount = 0;
            } else {
                std::memmove(_rxBuf, _rxBuf + 1, SENSOR_PKT_SIZE - 1);
                _rxCount = SENSOR_PKT_SIZE - 1;
                std::size_t nextStart = 0;
                while (nextStart < _rxCount && _rxBuf[nextStart] != SENSOR_START) {
                    nextStart++;
                }
                if (nextStart > 0) {
                    std::memmove(_rxBuf, _rxBuf + nextStart, _rxCount - nextStart);
                    _rxCount -= nextStart;
                }
            }
        }
    }
}

bool SerialLink::TryDecodeSensorPacket(SensorData &out) const {
    uint8_t chk = 0;
    for (std::size_t i = 1; i < SENSOR_PKT_SIZE - 1; ++i) chk ^= _rxBuf[i];
    if (chk != _rxBuf[SENSOR_PKT_SIZE - 1]) return false;

    std::memcpy(&out.heading, &_rxBuf[1],  sizeof(float));
    std::memcpy(&out.roll,    &_rxBuf[5],  sizeof(float));
    std::memcpy(&out.lat,     &_rxBuf[9],  sizeof(double));
    std::memcpy(&out.lon,     &_rxBuf[17], sizeof(double));
    std::memcpy(&out.alt,     &_rxBuf[25], sizeof(float));
    out.satellites = _rxBuf[29];
    out.gpsValid   = _rxBuf[30];
    return true;
}

void SerialLink::ThreadLoop() {
    auto lastHeartbeat = std::chrono::steady_clock::now();

    while (_running) {
        if (_fd < 0) {
            if (!OpenPort()) {
                std::fprintf(stderr, "[SerialLink] cannot open %s, retrying...\n",
                             _portPath.c_str());
                std::this_thread::sleep_for(std::chrono::milliseconds(_reconnectRetryMs));
                continue;
            }
            std::fprintf(stderr, "[SerialLink] connected to %s\n", _portPath.c_str());
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - lastHeartbeat).count();

        bool dueToHeartbeat = elapsed >= _heartbeatMs;
        bool dueToChange    = _stateDirty.exchange(false);

        if (dueToHeartbeat || dueToChange) {
            PanTiltState snapshot;
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                snapshot = _state;
            }
            if (!SendState(snapshot)) {
                std::fprintf(stderr, "[SerialLink] write failed, reconnecting...\n");
                ClosePort();
                continue;
            }
            lastHeartbeat = now;
        }

        uint8_t buf[256];
        ssize_t n = read(_fd, buf, sizeof(buf));
        if (n > 0) {
            FeedReceiveBuffer(buf, static_cast<std::size_t>(n));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (_fd >= 0) {
        SendState(PanTiltState::Stop());
        ClosePort();
    }
}
