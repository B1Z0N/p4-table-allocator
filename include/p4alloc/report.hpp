#pragma once

#include "p4alloc/allocator.hpp"
#include <nlohmann/json_fwd.hpp>
#include <ostream>

namespace p4alloc {

nlohmann::json json_report(const Program& program, const Allocation& allocation);
void print_report(std::ostream& stream, const Program& program, const Allocation& allocation);

}
