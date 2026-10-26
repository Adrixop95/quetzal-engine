// Tiles{TileSet}.xbe overlay metadata; ids and offsets only.
#pragma once
#include "render_rules.hpp"

namespace aztec::data {

struct OverlayRange { int first, last; };
// Nonempty shadow resources shared by Tiles0 and Tiles1. Bodies use the same id
// in group 0x44c. These ranges include trees, rocks and other decorations;
// presence does not establish a semantic tree type.
inline constexpr OverlayRange kTerrainOverlayRanges[] = {{4908, 4937}, {5036, 5132}};
inline constexpr OverlayRange kMission4TreeRange{4909, 4913};
inline constexpr int kOverlayCellSizeOffset = 0x10;
inline constexpr int kOverlayHotspotOffset = 0x14;
inline constexpr int kOverlayPhaseMask = 7;
// Wind intensity is supplied by the view's wind regions, not tile coordinates.
// Counter advances on unpaused view updates; this is not an Anim 12 Hz sequence.
enum class WindStrength { Still, Slow, Fast };
constexpr int overlay_frame(WindStrength wind, unsigned counter) {
  return wind == WindStrength::Still ? 0 : wind == WindStrength::Slow
       ? int((counter & kOverlayPhaseMask) >> 1) : int(counter & 3);
}

}  // namespace aztec::data
