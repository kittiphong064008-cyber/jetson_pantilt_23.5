#include "Packet.h"

std::array<uint8_t, Packet::SIZE> Packet::Build(uint8_t en1, uint8_t dir1, uint16_t speed1,
                                                 uint8_t en2, uint8_t dir2, uint16_t speed2) const {
    std::array<uint8_t, SIZE> data{};
    data[0] = START_BYTE;
    data[1] = en1;
    data[2] = dir1;
    data[3] = static_cast<uint8_t>(speed1 & 0xFF);
    data[4] = static_cast<uint8_t>(speed1 >> 8);
    data[5] = en2;
    data[6] = dir2;
    data[7] = static_cast<uint8_t>(speed2 & 0xFF);
    data[8] = static_cast<uint8_t>(speed2 >> 8);
    return data;
}

std::array<uint8_t, Packet::SIZE> Packet::Build(const PanTiltState &state) const {
    return Build(state.pan.en,  state.pan.dir,  state.pan.speed,
                 state.tilt.en, state.tilt.dir, state.tilt.speed);
}
