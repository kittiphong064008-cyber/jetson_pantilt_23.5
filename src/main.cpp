#include <atomic>
#include <chrono>
#include <cstdio>
#include <csignal>
#include <thread>

#include "Keyboard.h"
#include "SerialLink.h"
#include "Types.h"

namespace {

constexpr const char *DEFAULT_PORT = "/dev/ttyUSB0";
constexpr int HEARTBEAT_MS       = 150;
constexpr int RECONNECT_RETRY_MS = 1000;
constexpr int KB_POLL_MS         = 20;

std::atomic<bool> g_running{true};

void OnSignal(int) { g_running = false; }

std::string StateLabel(const AxisState &a) {
    if (!a.en) return "STOP";
    return (a.dir == 1 ? "CW  spd=" : "CCW spd=") + std::to_string(a.speed);
}

std::string SensorLabel(const SensorData &d) {
    char buf[160];
    std::snprintf(buf, sizeof(buf),
        "H:%.2f R:%.2f %s Sat:%u",
        d.heading, d.roll,
        d.HasLocation() ? "GPS:fix" : "GPS:no-fix",
        d.satellites);
    return buf;
}

}

int main(int argc, char **argv) {
    const char *port = (argc > 1) ? argv[1] : DEFAULT_PORT;

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    std::printf("Controls: A/D pan (hold), W/S tilt (hold), arrows = speed trim,\n"
                "          Esc = quit\n");
    std::printf("Port: %s\n", port);

    SerialLink serial(port, HEARTBEAT_MS, RECONNECT_RETRY_MS);
    serial.SetSensorCallback([](const SensorData &d) {
        std::printf("\r[ESP32] %s          ", SensorLabel(d).c_str());
        std::fflush(stdout);
    });
    serial.Start();

    Keyboard keyboard(1000, 1000);
    if (!keyboard.IsConnected()) {
        std::fprintf(stderr,
            "\n[main] keyboard not available — see the [Keyboard] message above.\n"
            "       Sensor data will still print, but no motor commands can be sent.\n");
    }

    PanTiltState prev = PanTiltState::Stop();
    uint16_t prevPanSpd  = keyboard.PanSpeed();
    uint16_t prevTiltSpd = keyboard.TiltSpeed();

    while (g_running && !keyboard.QuitRequested()) {
        PanTiltState current = keyboard.GetCurrentCommand();

        if (keyboard.PanSpeed() != prevPanSpd || keyboard.TiltSpeed() != prevTiltSpd) {
            std::printf("\n[SPD] Pan=%u  Tilt=%u\n", keyboard.PanSpeed(), keyboard.TiltSpeed());
            prevPanSpd  = keyboard.PanSpeed();
            prevTiltSpd = keyboard.TiltSpeed();
        }

        if (current != prev) {
            std::printf("\n[KB] Pan:%s  Tilt:%s\n",
                        StateLabel(current.pan).c_str(),
                        StateLabel(current.tilt).c_str());
            prev = current;
        }

        serial.SetState(current);

        std::this_thread::sleep_for(std::chrono::milliseconds(KB_POLL_MS));
    }

    serial.Stop();

    std::printf("\nStopped, motors set to zero, exiting.\n");
    return 0;
}
