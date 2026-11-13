#pragma once

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>

#include "logtriage/analyzer.hpp"

namespace logtriage {

// Human-readable report. `top_n == 0` prints every issue group.
void print_text_report(std::ostream& out, const Analysis& analysis, Level min_level,
                       std::size_t top_n);

// Machine-readable report. `top_n == 0` includes every issue group.
void print_json_report(std::ostream& out, const Analysis& analysis, std::size_t top_n);

// Escapes a string for use inside a JSON string literal.
std::string json_escape(std::string_view text);

}  // namespace logtriage
