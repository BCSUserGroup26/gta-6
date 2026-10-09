#pragma once
#include <cstddef>
#include <cstdint>
namespace bjb {
constexpr std::uint32_t audioStereoS16 = 1;
constexpr std::uint32_t audioFrames = 256;
constexpr std::uint32_t audioRate = 48000;
constexpr std::size_t audioSamples = audioFrames * 2;
// pl_mpeg writes RGB channels only; the compositing canvas consumes alpha too.
inline void makeVideoOpaque(std::uint8_t* pixels, std::size_t size) noexcept {
    for (std::size_t i=3;i<size;i+=4) pixels[i]=255;
}
}
