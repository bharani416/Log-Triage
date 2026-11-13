#include "logtriage/parser.hpp"

#include <cctype>
#include <utility>

namespace logtriage {
namespace {

constexpr std::size_t kMaxModuleLength = 64;
constexpr std::size_t kTimestampLength = 19;  // "YYYY-MM-DD HH:MM:SS"

bool is_digit(char c) { return c >= '0' && c <= '9'; }

bool is_blank(std::string_view s) {
    for (char c : s) {
        if (c != ' ' && c != '\t') return false;
    }
    return true;
}

// Control characters (other than tab) mean the line is binary garbage.
bool has_control_chars(std::string_view s) {
    for (char c : s) {
        const auto u = static_cast<unsigned char>(c);
        if ((u < 0x20 && u != '\t') || u == 0x7F) return true;
    }
    return false;
}

bool valid_timestamp(std::string_view s) {
    if (s.size() != kTimestampLength) return false;
    static const char kPattern[] = "dddd-dd-dd dd:dd:dd";
    for (std::size_t i = 0; i < kTimestampLength; ++i) {
        if (kPattern[i] == 'd') {
            if (!is_digit(s[i])) return false;
        } else if (s[i] != kPattern[i]) {
            return false;
        }
    }
    auto num = [&](std::size_t pos, std::size_t len) {
        int v = 0;
        for (std::size_t i = 0; i < len; ++i) v = v * 10 + (s[pos + i] - '0');
        return v;
    };
    const int month = num(5, 2), day = num(8, 2);
    const int hour = num(11, 2), minute = num(14, 2), second = num(17, 2);
    return month >= 1 && month <= 12 && day >= 1 && day <= 31 && hour <= 23 &&
           minute <= 59 && second <= 59;
}

bool valid_module(std::string_view m) {
    if (m.empty() || m.size() > kMaxModuleLength) return false;
    for (char c : m) {
        const bool ok = std::isalnum(static_cast<unsigned char>(c)) || c == '_' ||
                        c == '-' || c == '.' || c == '/' || c == ':';
        if (!ok) return false;
    }
    return true;
}

}  // namespace

const char* to_string(ParseError error) {
    switch (error) {
        case ParseError::None:           return "NONE";
        case ParseError::Empty:          return "EMPTY";
        case ParseError::TooLong:        return "TOO_LONG";
        case ParseError::BinaryData:     return "BINARY_DATA";
        case ParseError::BadTimestamp:   return "BAD_TIMESTAMP";
        case ParseError::BadLevel:       return "BAD_LEVEL";
        case ParseError::BadModule:      return "BAD_MODULE";
        case ParseError::MissingMessage: return "MISSING_MESSAGE";
    }
    return "UNKNOWN";
}

ParseResult parse_line(std::string_view line, std::size_t line_no, std::size_t max_len) {
    auto fail = [](ParseError e) { return ParseResult{std::nullopt, e}; };

    // Tolerate Windows line endings before any length/format checks.
    while (!line.empty() && line.back() == '\r') line.remove_suffix(1);

    if (line.size() > max_len) return fail(ParseError::TooLong);
    if (is_blank(line)) return fail(ParseError::Empty);
    if (has_control_chars(line)) return fail(ParseError::BinaryData);

    if (line.size() < kTimestampLength || !valid_timestamp(line.substr(0, kTimestampLength))) {
        return fail(ParseError::BadTimestamp);
    }
    const std::string_view timestamp = line.substr(0, kTimestampLength);
    std::size_t pos = kTimestampLength;

    auto skip_spaces = [&] {
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
    };
    auto read_bracketed = [&](std::string_view& out) {
        if (pos >= line.size() || line[pos] != '[') return false;
        const std::size_t close = line.find(']', pos + 1);
        if (close == std::string_view::npos) return false;
        out = line.substr(pos + 1, close - pos - 1);
        pos = close + 1;
        return true;
    };

    skip_spaces();
    std::string_view level_text;
    if (!read_bracketed(level_text)) return fail(ParseError::BadLevel);
    const auto level = parse_level(level_text);
    if (!level) return fail(ParseError::BadLevel);

    skip_spaces();
    std::string_view module_text;
    if (!read_bracketed(module_text) || !valid_module(module_text)) {
        return fail(ParseError::BadModule);
    }

    skip_spaces();
    std::string_view message = line.substr(pos);
    while (!message.empty() && (message.back() == ' ' || message.back() == '\t')) {
        message.remove_suffix(1);
    }
    if (message.empty()) return fail(ParseError::MissingMessage);

    LogEntry entry;
    entry.timestamp.assign(timestamp);
    entry.level = *level;
    entry.module.assign(module_text);
    entry.message.assign(message);
    entry.line_no = line_no;
    return ParseResult{std::move(entry), ParseError::None};
}

}  // namespace logtriage
