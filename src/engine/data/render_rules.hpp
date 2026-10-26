// Render metadata only. All masks, sprites and frame records are loaded from user files.
#pragma once
#include <cstdint>

namespace aztec::data {

inline constexpr int kAnimationTicksPerSecond = 12;
inline constexpr uint32_t kBodyGroup = 0x44c, kShadowGroup = 0x454, kLightGroup = 0x455;
inline constexpr int kTilePixels = 48, kTileCentre = 24;
inline constexpr int kShadowShade = 12;

// The reference sorts a shared list of terrain overlays and objects by ascending
// ground Y, then descending X. Height == -1 entries precede ordinary entries;
// their mutual order is unspecified. There is no separate air pass.
enum class DrawOrder { Underlay, GroundY };
enum class PositionSource { Pixels, Fixed256, Fixed256Truncate, TileCentre };
enum class HeightSource { Zero, Frame, ObjectPixels, ObjectFixed256 };

struct ObjectRenderLayout {
  const char* name;
  uint32_t vtable;
  uint16_t anim_pointer, second_anim_pointer;
  uint16_t x, y;  // object field offsets
  PositionSource position;
  HeightSource height;
  uint16_t height_offset;
  uint16_t light_method;  // virtual method offset; 0 means no dynamic light
};

inline constexpr ObjectRenderLayout kObjectRenderLayouts[] = {
  {"Marine",       0x4932cc, 0x7c, 0,    0x6c, 0x70, PositionSource::Fixed256, HeightSource::Frame,          0,    0x70},
  {"Spider",       0x4940ec, 0x70, 0x74, 0x78, 0x7c, PositionSource::Fixed256, HeightSource::Zero,           0,    0x70},
  {"Building",     0x48fd20, 0x6c, 0,    0x44, 0x48, PositionSource::TileCentre,   HeightSource::Frame,          0,    0x70},
  {"Zeppelin",     0x4951b8, 0x8c, 0,    0x68, 0x6c, PositionSource::Fixed256, HeightSource::ObjectPixels,   0x7c, 0x70},
  {"Truck",        0x4948e8, 0x94, 0,    0x6c, 0x70, PositionSource::Fixed256, HeightSource::Frame,          0,    0x70},
  {"Bird",         0x48f97c, 0x98, 0,    0x68, 0x6c, PositionSource::Fixed256, HeightSource::ObjectPixels,   0x7c, 0x70},
  {"Headtruck",    0x492c38, 0x9c, 0xa0, 0x70, 0x74, PositionSource::Fixed256, HeightSource::Frame,          0,    0x70},
  {"Defence",      0x4905bc, 0x70, 0x74, 0x44, 0x48, PositionSource::TileCentre,   HeightSource::Frame,          0,    0x70},
  {"Boat",         0x48faa8, 0x64, 0,    0x24, 0x28, PositionSource::Pixels,   HeightSource::Zero,           0,    0},
  {"Cannon",       0x490380, 0x6c, 0,    0x70, 0x74, PositionSource::Fixed256, HeightSource::Frame,          0,    0x70},
  {"Johnka",       0x492d64, 0x64, 0,    0x24, 0x28, PositionSource::Pixels,   HeightSource::Zero,           0,    0},
  {"Arrow",        0x48f558, 0x68, 0,    0x40, 0x44, PositionSource::Fixed256Truncate, HeightSource::ObjectFixed256, 0x58, 0x80},
  {"Axe",          0x48f7e0, 0x5c, 0,    0x40, 0x44, PositionSource::Fixed256Truncate, HeightSource::Frame,          0, 0x80},
  {"BallExpl",     0x48f8f4, 0x40, 0,    0x44, 0x48, PositionSource::Fixed256Truncate, HeightSource::ObjectFixed256, 0x54, 0x80},
  {"FireThrow",    0x4927a8, 0x38, 0,    0x24, 0x28, PositionSource::Pixels,   HeightSource::ObjectPixels,   0x3c, 0x80},
  {"SplashDamage", 0x4943a4, 0x40, 0,    0x24, 0x28, PositionSource::Pixels,   HeightSource::ObjectPixels,   0x44, 0x80},
  {"Decoration",   0x4904ac, 0x30, 0,    0x24, 0x28, PositionSource::Pixels,   HeightSource::ObjectPixels,   0x34, 0x5c},
  {"DecorationOnce",0x49406c,0x30, 0,    0x24, 0x28, PositionSource::Pixels,   HeightSource::ObjectPixels,   0x34, 0x5c},
};

// Every ordinary item draws base shadow, base body, turret shadow, turret body,
// then any overlay (+0x2c virtual). Shadows are per item, not a global first pass.
enum class DrawPart { BaseShadow, BaseBody, TurretShadow, TurretBody, Overlay };
inline constexpr DrawPart kObjectDrawParts[] = {
  DrawPart::BaseShadow, DrawPart::BaseBody, DrawPart::TurretShadow,
  DrawPart::TurretBody, DrawPart::Overlay
};
// Body: (world_x - hotspot_x, world_y - height - hotspot_y).
// Shadows use the ground position, without height. Terrain overlay positions
// are (tile_x * 48 + 24, tile_y * 48 + 24).
// Overlay ids are the upper 16 bits of the map cell, used in both image groups.

inline constexpr int kLightMaskWidth = 20, kLightMaskHeight = 20;
inline constexpr int kLightMaskHeaderBytes = 4;
inline constexpr int kMaxLightValue = 152, kPaletteShadeCount = 20;
inline constexpr int kFirstGlowMask = 1, kLastGlowMask = 16;
inline constexpr int kFirstVisionMask = 64, kLastVisionMask = 71;
// Glow masks add brightness, capped at 152. Vision masks take the minimum
// darkness (152 = dark). These are separate grids; do not invert glow bytes.
inline constexpr int kLightGridObjectData = 0x14;
inline constexpr int kLightGridCellWidth = 0x0c, kLightGridCellHeight = 0x10;
inline constexpr int kLightMaskWorldOriginBias = 1;  // top-left = pos - 9 * cell size
inline constexpr int kFrameLightField = 4;
inline constexpr int kFrameHeightField = 24;
inline constexpr int kBuildingLightDescriptorOffset = 0xdc;
inline constexpr int kDefenceLightDescriptorOffset = 0xe8;
inline constexpr int kTransportVisionMask = 71;

}  // namespace aztec::data
