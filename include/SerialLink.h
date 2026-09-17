#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "Packet.h"
#include "Types.h"

class SerialLink {
public:
    using SensorCallback = std::function<void(const SensorData &)>;

    explicit SerialLink(std::string portPath,
                         int heartbeatMs = 150,
                         int reconnectRetryMs = 1000);
    ~SerialLink();

    SerialLink(const SerialLink &)            = delete;
    SerialLink &operator=(const SerialLink &) = delete;

    void Start();
    void Stop();
    void SetState(const PanTiltState &state);
    void SetSensorCallback(SensorCallback cb);

private:
    static constexpr uint8_t     SENSOR_START    = 0xBB;
    static constexpr std::size_t SENSOR_PKT_SIZE = 32;

    void ThreadLoop();
    bool OpenPort();
    void ClosePort();
    bool SendState(const PanTiltState &state);
    void FeedReceiveBuffer(const uint8_t *data, std::size_t len);
    bool TryDecodeSensorPacket(SensorData &out) const;

    std::string _portPath;
    int         _heartbeatMs;
    int         _reconnectRetryMs;

    int  _fd = -1;
    std::atomic<bool> _running{false};
    std::thread        _thread;

    std::mutex        _stateMutex;
    PanTiltState      _state{};
    std::atomic<bool> _stateDirty{false};

    SensorCallback _sensorCallback;
    Packet         _packet;

    uint8_t     _rxBuf[SENSOR_PKT_SIZE]{};
    std::size_t _rxCount = 0;
};
