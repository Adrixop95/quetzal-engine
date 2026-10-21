// Display names for object types from Res/strbase.bds (English), falling back to the UGI name.
#pragma once
#include <string>
#include "core/strbase.hpp"

namespace aztec::engine {

// strbase ordinal for a type id (units 194..210, buildings 169..193; assignment is by name), 0 if unknown.
int type_name_ordinal(int type_id);
std::string type_display_name(const StrBase& strings, int type_id);

}  // namespace aztec::engine
