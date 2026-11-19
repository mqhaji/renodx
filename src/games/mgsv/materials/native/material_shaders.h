// Native G-buffer variants that bake a material cube into albedo. Each port also
// publishes its color without that cube. Variants without a cube are not ported:
// their native albedo is already the material color.
#ifndef MGSV_MATERIAL_SHADERS_H_
#define MGSV_MATERIAL_SHADERS_H_

#include <array>
#include <cstdint>
#include <span>

#include <embed/shaders.h>

namespace mgsv::lighting::material_inputs {

struct MaterialShader {
  uint32_t hash;
  std::span<const uint8_t> code;
};

inline constexpr auto MATERIAL_SHADERS = std::to_array<MaterialShader>({
    {.hash = 0x003B5D88, .code = __0x003B5D88},
    {.hash = 0x0258CB11, .code = __0x0258CB11},
    {.hash = 0x04C8943B, .code = __0x04C8943B},
    {.hash = 0x231088C8, .code = __0x231088C8},
    {.hash = 0x26CF6B28, .code = __0x26CF6B28},
    {.hash = 0x297497E3, .code = __0x297497E3},
    {.hash = 0x29F05A61, .code = __0x29F05A61},
    {.hash = 0x2B619DAC, .code = __0x2B619DAC},
    {.hash = 0x31771507, .code = __0x31771507},
    {.hash = 0x342C90DD, .code = __0x342C90DD},
    {.hash = 0x37E0E714, .code = __0x37E0E714},
    {.hash = 0x3BCA6599, .code = __0x3BCA6599},
    {.hash = 0x3E7A4E6C, .code = __0x3E7A4E6C},
    {.hash = 0x446E10B1, .code = __0x446E10B1},
    {.hash = 0x471240BA, .code = __0x471240BA},
    {.hash = 0x48EB1230, .code = __0x48EB1230},
    {.hash = 0x4ADCE011, .code = __0x4ADCE011},
    {.hash = 0x4BE2F925, .code = __0x4BE2F925},
    {.hash = 0x4FD0DB83, .code = __0x4FD0DB83},
    {.hash = 0x502DCAC7, .code = __0x502DCAC7},
    {.hash = 0x5E9ADB19, .code = __0x5E9ADB19},
    {.hash = 0x619940BD, .code = __0x619940BD},
    {.hash = 0x63B55B1C, .code = __0x63B55B1C},
    {.hash = 0x6513164B, .code = __0x6513164B},
    {.hash = 0x6730F8A9, .code = __0x6730F8A9},
    {.hash = 0x67AC7D4A, .code = __0x67AC7D4A},
    {.hash = 0x687A880B, .code = __0x687A880B},
    {.hash = 0x68AC641A, .code = __0x68AC641A},
    {.hash = 0x6CDBF200, .code = __0x6CDBF200},
    {.hash = 0x70D3479C, .code = __0x70D3479C},
    {.hash = 0x71CE0F14, .code = __0x71CE0F14},
    {.hash = 0x74E7071A, .code = __0x74E7071A},
    {.hash = 0x7F36BD8A, .code = __0x7F36BD8A},
    {.hash = 0x80BD4BD6, .code = __0x80BD4BD6},
    {.hash = 0x8332D9B4, .code = __0x8332D9B4},
    {.hash = 0x8400B30C, .code = __0x8400B30C},
    {.hash = 0x8A1EF5F1, .code = __0x8A1EF5F1},
    {.hash = 0x8EB3185F, .code = __0x8EB3185F},
    {.hash = 0x9374936F, .code = __0x9374936F},
    {.hash = 0x93A8F77A, .code = __0x93A8F77A},
    {.hash = 0x949ECA3A, .code = __0x949ECA3A},
    {.hash = 0x97397306, .code = __0x97397306},
    {.hash = 0x9AFAEBA1, .code = __0x9AFAEBA1},
    {.hash = 0x9C55B6E3, .code = __0x9C55B6E3},
    {.hash = 0x9D6B4198, .code = __0x9D6B4198},
    {.hash = 0xAA0409ED, .code = __0xAA0409ED},
    {.hash = 0xAAF18DAE, .code = __0xAAF18DAE},
    {.hash = 0xADE574B1, .code = __0xADE574B1},
    {.hash = 0xBA477245, .code = __0xBA477245},
    {.hash = 0xBCE393A5, .code = __0xBCE393A5},
    {.hash = 0xC108E224, .code = __0xC108E224},
    {.hash = 0xC286A647, .code = __0xC286A647},
    {.hash = 0xD1302286, .code = __0xD1302286},
    {.hash = 0xD1444E96, .code = __0xD1444E96},
    {.hash = 0xD5C51249, .code = __0xD5C51249},
    {.hash = 0xD8D92B2D, .code = __0xD8D92B2D},
    {.hash = 0xEB8E39D0, .code = __0xEB8E39D0},
    {.hash = 0xF2250E4C, .code = __0xF2250E4C},
    {.hash = 0xF457C82D, .code = __0xF457C82D},
});

}  // namespace mgsv::lighting::material_inputs

#endif  // MGSV_MATERIAL_SHADERS_H_
