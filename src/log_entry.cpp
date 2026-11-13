#include "logtriage/log_entry.hpp"

#include <cctype>

namespace logtriage {

const char* to_string(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO";
        case Level::Warn:  return "WARN";
        case Level::Error: return "ERROR";
        case Level::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

std::optional<Level> parse_level(std::string_view text) {
    std::string up;
    up.reserve(text.size());
    for (char c : text) {
        up.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    if (up == "DEBUG" || up == "DBG" || up == "TRACE") return Level::Debug;
    if (up == "INFO" || up == "NOTICE") return Level::Info;
    if (up == "WARN" || up == "WARNING") return Level::Warn;
    if (up == "ERROR" || up == "ERR") return Level::Error;
    if (up == "FATAL" || up == "CRIT" || up == "CRITICAL" || up == "EMERG") return Level::Fatal;
    return std::nullopt;
}

}  // namespace logtriage
