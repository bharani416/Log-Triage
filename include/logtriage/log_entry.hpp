#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace logtriage {

// Severity levels, ordered so that comparison operators work (Debug < Fatal).
enum class Level { Debug = 0, Info, Warn, Error, Fatal };

constexpr std::size_t kLevelCount = 5;

const char* to_string(Level level);

// Case-insensitive; accepts common aliases (WARNING, ERR, CRIT, ...).
std::optional<Level> parse_level(std::string_view text);

// One successfully parsed log line.
struct LogEntry {
    std::string timestamp;  // "YYYY-MM-DD HH:MM:SS"
    Level level = Level::Info;
    std::string module;     // text inside the second [...]
    std::string message;
    std::size_t line_no = 0;
};

}  // namespace logtriage
