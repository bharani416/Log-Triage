#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

#include "logtriage/log_entry.hpp"

namespace logtriage {

// Why a line could not be parsed. `Empty` is not treated as corruption.
enum class ParseError {
    None,
    Empty,
    TooLong,
    BinaryData,
    BadTimestamp,
    BadLevel,
    BadModule,
    MissingMessage,
};

const char* to_string(ParseError error);

struct ParseResult {
    std::optional<LogEntry> entry;  // set on success
    ParseError error = ParseError::None;
};

constexpr std::size_t kDefaultMaxLineLength = 4096;

// Parses one line of the form:
//   YYYY-MM-DD HH:MM:SS [LEVEL] [module] message
// Never throws; malformed input is reported through ParseResult::error.
ParseResult parse_line(std::string_view line, std::size_t line_no,
                       std::size_t max_len = kDefaultMaxLineLength);

}  // namespace logtriage
