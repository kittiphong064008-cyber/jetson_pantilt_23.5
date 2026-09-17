#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "Types.h"

class Packet {
public:
    static constexpr uint8_t     START_BYTE = 0xAA;
    static constexpr std::size_t SIZE       = 9;

    std::array<uint8_t, SIZE> Build(uint8_t en1, uint8_t dir1, uint16_t speed1,
                                     uint8_t en2, uint8_t dir2, uint16_t speed2) const;

    std::array<uint8_t, SIZE> Build(const PanTiltState &state) const;
};
