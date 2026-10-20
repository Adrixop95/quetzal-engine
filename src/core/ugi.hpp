// Unit/building type ids (UGI_*), as used by .mp files, scripts and DisabledID lists.
#pragma once
#include <string_view>

namespace aztec {

constexpr int kUgiCount = 90;
const char* ugi_name(int id);                 // nullptr when out of range
int ugi_from_name(std::string_view name);     // case-insensitive, -1 when unknown

}  // namespace aztec
