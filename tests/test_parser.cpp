#include <string>

#include "logtriage/parser.hpp"
#include "test_util.hpp"

using namespace logtriage;

TEST(parser_valid_line) {
    auto r = parse_line("2026-11-13 10:15:32 [ERROR] [kernel] I/O error on mmcblk0", 7);
    CHECK(r.entry.has_value());
    CHECK(r.error == ParseError::None);
    CHECK(r.entry->timestamp == "2026-11-13 10:15:32");
    CHECK(r.entry->level == Level::Error);
    CHECK(r.entry->module == "kernel");
    CHECK(r.entry->message == "I/O error on mmcblk0");
    CHECK(r.entry->line_no == 7);
}

TEST(parser_level_aliases_case_insensitive) {
    CHECK(parse_line("2026-01-01 00:00:00 [warning] [m] x", 1).entry->level == Level::Warn);
    CHECK(parse_line("2026-01-01 00:00:00 [err] [m] x", 1).entry->level == Level::Error);
    CHECK(parse_line("2026-01-01 00:00:00 [CRIT] [m] x", 1).entry->level == Level::Fatal);
    CHECK(parse_line("2026-01-01 00:00:00 [Info] [m] x", 1).entry->level == Level::Info);
}

TEST(parser_handles_crlf_line_endings) {
    auto r = parse_line("2026-11-13 10:15:32 [INFO] [boot] started\r", 1);
    CHECK(r.entry.has_value());
    CHECK(r.entry->message == "started");
}

TEST(parser_tolerates_extra_whitespace) {
    auto r = parse_line("2026-11-13 10:15:32   [WARN]   [net]   link flapping   ", 1);
    CHECK(r.entry.has_value());
    CHECK(r.entry->message == "link flapping");
}

TEST(parser_keeps_brackets_inside_message) {
    auto r = parse_line("2026-11-13 10:15:32 [ERROR] [net] link [eth0] down", 1);
    CHECK(r.entry.has_value());
    CHECK(r.entry->module == "net");
    CHECK(r.entry->message == "link [eth0] down");
}

TEST(parser_blank_lines_are_empty_not_corrupt) {
    CHECK(parse_line("", 1).error == ParseError::Empty);
    CHECK(parse_line("   \t ", 1).error == ParseError::Empty);
    CHECK(parse_line("\r", 1).error == ParseError::Empty);
}

TEST(parser_rejects_bad_timestamps) {
    CHECK(parse_line("not a log line at all", 1).error == ParseError::BadTimestamp);
    CHECK(parse_line("2026-11-13 10:1", 1).error == ParseError::BadTimestamp);  // truncated
    CHECK(parse_line("2026-13-01 10:00:00 [INFO] [m] x", 1).error == ParseError::BadTimestamp);
    CHECK(parse_line("2026-11-32 10:00:00 [INFO] [m] x", 1).error == ParseError::BadTimestamp);
    CHECK(parse_line("2026-11-13 25:00:00 [INFO] [m] x", 1).error == ParseError::BadTimestamp);
    CHECK(parse_line("2026/11/13 10:00:00 [INFO] [m] x", 1).error == ParseError::BadTimestamp);
}

TEST(parser_rejects_bad_levels) {
    CHECK(parse_line("2026-11-13 10:00:00 [BOGUS] [m] x", 1).error == ParseError::BadLevel);
    CHECK(parse_line("2026-11-13 10:00:00 ERROR [m] x", 1).error == ParseError::BadLevel);
    CHECK(parse_line("2026-11-13 10:00:00 [ERROR [m] x", 1).error == ParseError::BadLevel);
    CHECK(parse_line("2026-11-13 10:00:00", 1).error == ParseError::BadLevel);
}

TEST(parser_rejects_bad_modules) {
    CHECK(parse_line("2026-11-13 10:00:00 [INFO] kernel x", 1).error == ParseError::BadModule);
    CHECK(parse_line("2026-11-13 10:00:00 [INFO] [] x", 1).error == ParseError::BadModule);
    CHECK(parse_line("2026-11-13 10:00:00 [INFO] [bad module] x", 1).error ==
          ParseError::BadModule);
    CHECK(parse_line("2026-11-13 10:00:00 [INFO]", 1).error == ParseError::BadModule);
}

TEST(parser_rejects_missing_message) {
    CHECK(parse_line("2026-11-13 10:00:00 [INFO] [m]", 1).error == ParseError::MissingMessage);
    CHECK(parse_line("2026-11-13 10:00:00 [INFO] [m]    ", 1).error ==
          ParseError::MissingMessage);
}

TEST(parser_rejects_binary_data) {
    std::string with_nul = "2026-11-13 10:00:00 [INFO] [m] ab";
    with_nul.push_back('\0');
    with_nul += "cd";
    CHECK(parse_line(with_nul, 1).error == ParseError::BinaryData);
    CHECK(parse_line("2026-11-13 10:00:00 [INFO] [m] \x01\x02 junk", 1).error ==
          ParseError::BinaryData);
}

TEST(parser_allows_tabs_and_utf8) {
    auto r = parse_line("2026-11-13 10:00:00 [INFO] [m] temp\t25\xC2\xB0" "C ok", 1);
    CHECK(r.entry.has_value());
}

TEST(parser_rejects_over_long_lines) {
    std::string line = "2026-11-13 10:00:00 [INFO] [m] " + std::string(200, 'x');
    CHECK(parse_line(line, 1, 4096).entry.has_value());
    CHECK(parse_line(line, 1, 100).error == ParseError::TooLong);
}

TEST(parser_length_limit_is_exact) {
    const std::string base = "2026-11-13 10:00:00 [INFO] [m] ";
    const std::string at_limit = base + std::string(20, 'x');
    CHECK(parse_line(at_limit, 1, at_limit.size()).entry.has_value());
    CHECK(parse_line(at_limit, 1, at_limit.size() - 1).error == ParseError::TooLong);
}
