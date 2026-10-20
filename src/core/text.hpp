// Text helpers: original game data uses Windows-1250 (Polish). The engine works in UTF-8.
#pragma once
#include <string>
#include <string_view>

namespace aztec {

std::string cp1250_to_utf8(std::string_view in);

}  // namespace aztec
