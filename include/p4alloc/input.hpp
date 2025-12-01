#pragma once

#include "p4alloc/model.hpp"
#include <string_view>

namespace p4alloc {

Result<Program> parse_input(std::string_view input);
Result<Program> load_input(const std::string& path);

}
