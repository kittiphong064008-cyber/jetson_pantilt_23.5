#include "Keyboard.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

Keyboard::Keyboard(uint16_t panSpeed, uint16_t tiltSpeed)
    : _panSpeed(panSpeed), _tiltSpeed(tiltSpeed) {

    _display = XOpenDisplay(nullptr);
    if (!_display) {
        std::fprintf(stderr,
            "[Keyboard] XOpenDisplay() failed — no X display available.\n");
        return;
    }

    _codeA     = ResolveKeyCode("a");
    _codeD     = ResolveKeyCode("d");
    _codeW     = ResolveKeyCode("w");
    _codeS     = ResolveKeyCode("s");
    _codeLeft  = ResolveKeyCode("Left");
    _codeRight = ResolveKeyCode("Right");
    _codeUp    = ResolveKeyCode("Up");
    _codeDown  = ResolveKeyCode("Down");
    _codeEsc   = ResolveKeyCode("Escape");

    std::fprintf(stderr, "[Keyboard] connected to X display %s\n",
                 DisplayString(_display));
}

Keyboard::~Keyboard() {
    if (_display) XCloseDisplay(_display);
}

KeyCode Keyboard::ResolveKeyCode(const char *keysymName) const {
    KeySym sym = XStringToKeysym(keysymName);
    if (sym == NoSymbol) return 0;
    return XKeysymToKeycode(_display, sym);
}

bool Keyboard::IsKeyPressed(KeyCode code) const {
    if (!_display || code == 0) return false;

    char keys[32];
    XQueryKeymap(_display, keys);
    return (keys[code / 8] & (1 << (code % 8))) != 0;
}

PanTiltState Keyboard::GetCurrentCommand() {
    if (!_display) return PanTiltState::Stop();

    char keys[32];
    XQueryKeymap(_display, keys);

    auto isDown = [&](KeyCode code) {
        return code != 0 && (keys[code / 8] & (1 << (code % 8))) != 0;
    };

    bool a     = isDown(_codeA);
    bool d     = isDown(_codeD);
    bool w     = isDown(_codeW);
    bool s     = isDown(_codeS);
    bool left  = isDown(_codeLeft);
    bool right = isDown(_codeRight);
    bool up    = isDown(_codeUp);
    bool down  = isDown(_codeDown);

    if (isDown(_codeEsc)) _quit = true;

    if (left && !_leftWasPressed)
        _panSpeed = std::max<uint16_t>(MIN_SPEED, _panSpeed - STEP);
    if (right && !_rightWasPressed)
        _panSpeed = std::min<uint16_t>(MAX_SPEED, _panSpeed + STEP);
    if (up && !_upWasPressed)
        _tiltSpeed = std::min<uint16_t>(MAX_SPEED, _tiltSpeed + STEP);
    if (down && !_downWasPressed)
        _tiltSpeed = std::max<uint16_t>(MIN_SPEED, _tiltSpeed - STEP);

    _leftWasPressed  = left;
    _rightWasPressed = right;
    _upWasPressed    = up;
    _downWasPressed  = down;

    bool panLeft   = a && !d;
    bool panRight  = d && !a;
    bool tiltUp    = w && !s;
    bool tiltDown  = s && !w;

    PanTiltState st{};
    if (panLeft)       { st.pan.en = 1; st.pan.dir = 0; st.pan.speed = _panSpeed; }
    else if (panRight) { st.pan.en = 1; st.pan.dir = 1; st.pan.speed = _panSpeed; }

    if (tiltUp)        { st.tilt.en = 1; st.tilt.dir = 1; st.tilt.speed = _tiltSpeed; }
    else if (tiltDown) { st.tilt.en = 1; st.tilt.dir = 0; st.tilt.speed = _tiltSpeed; }

    return st;
}
