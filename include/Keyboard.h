#pragma once
#include <cstdint>
#include <X11/Xlib.h>
#ifdef None
#undef None
#endif
#ifdef CursorShape
#undef CursorShape
#endif
#ifdef Status
#undef Status
#endif
#ifdef KeyPress
#undef KeyPress
#endif
#ifdef KeyRelease
#undef KeyRelease
#endif
#include "Types.h"

class Keyboard {
public:
    explicit Keyboard(uint16_t panSpeed = 1000, uint16_t tiltSpeed = 1000);
    ~Keyboard();

    Keyboard(const Keyboard &)            = delete;
    Keyboard &operator=(const Keyboard &) = delete;

    PanTiltState GetCurrentCommand();

    uint16_t PanSpeed()  const { return _panSpeed; }
    uint16_t TiltSpeed() const { return _tiltSpeed; }

    bool QuitRequested() const { return _quit; }
    bool IsConnected() const { return _display != nullptr; }

private:
    static constexpr uint16_t STEP      = 50;
    static constexpr uint16_t MIN_SPEED = 200;
    static constexpr uint16_t MAX_SPEED = 3200;

    bool IsKeyPressed(KeyCode code) const;
    KeyCode ResolveKeyCode(const char *keysymName) const;

    Display *_display = nullptr;

    KeyCode _codeA = 0, _codeD = 0, _codeW = 0, _codeS = 0;
    KeyCode _codeLeft = 0, _codeRight = 0, _codeUp = 0, _codeDown = 0;
    KeyCode _codeEsc = 0;

    uint16_t _panSpeed;
    uint16_t _tiltSpeed;
    bool     _quit = false;

    bool _leftWasPressed = false, _rightWasPressed = false;
    bool _upWasPressed = false, _downWasPressed = false;
};
