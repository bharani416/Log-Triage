#include <sstream>
#include <string>

#include "logtriage/analyzer.hpp"
#include "test_util.hpp"

using namespace logtriage;

TEST(analyzer_counts_mixed_good_and_corrupt_lines) {
    std::istringstream in(
        "2026-11-13 10:00:00 [INFO] [boot] system started\n"
        "2026-11-13 10:00:01 [ERROR] [mmc] I/O error on mmcblk0\n"
        "\n"
        "garbage that is not a log line\n"
        "2026-13-99 99:99:99 [ERROR] [mmc] bad timestamp\n"
        "2026-11-13 10:00:02 [WARN] [net] connection timed out\n");
    Analysis a = analyze_stream(in);

    CHECK(a.stats.total_lines == 6);
    CHECK(a.stats.parsed_lines == 3);
    CHECK(a.stats.blank_lines == 1);
    CHECK(a.stats.malformed_lines == 2);
    CHECK(a.stats.error_counts[ParseError::BadTimestamp] == 2);
    CHECK(a.stats.level_counts[static_cast<std::size_t>(Level::Info)] == 1);
    CHECK(a.stats.level_counts[static_cast<std::size_t>(Level::Error)] == 1);
    CHECK(a.stats.level_counts[static_cast<std::size_t>(Level::Warn)] == 1);
    CHECK(a.issues.size() == 2);  // INFO is below the default WARN threshold
}

TEST(analyzer_empty_input) {
    std::istringstream in("");
    Analysis a = analyze_stream(in);
    CHECK(a.stats.total_lines == 0);
    CHECK(a.issues.empty());
}

TEST(analyzer_only_blank_lines) {
    std::istringstream in("\n\n   \n");
    Analysis a = analyze_stream(in);
    CHECK(a.stats.total_lines == 3);
    CHECK(a.stats.blank_lines == 3);
    CHECK(a.stats.malformed_lines == 0);
}

TEST(analyzer_counts_last_line_without_newline) {
    std::istringstream in("2026-11-13 10:00:00 [ERROR] [m] failed");
    Analysis a = analyze_stream(in);
    CHECK(a.stats.total_lines == 1);
    CHECK(a.stats.parsed_lines == 1);
}

TEST(analyzer_handles_crlf_files) {
    std::istringstream in(
        "2026-11-13 10:00:00 [ERROR] [m] failed\r\n"
        "2026-11-13 10:00:01 [ERROR] [m] failed again\r\n");
    Analysis a = analyze_stream(in);
    CHECK(a.stats.parsed_lines == 2);
    CHECK(a.stats.malformed_lines == 0);
}

TEST(analyzer_survives_enormous_line_and_continues) {
    std::string input = "2026-11-13 10:00:00 [INFO] [m] before\n";
    input += std::string(2 * 1024 * 1024, 'A');  // 2 MB with no newline in sight
    input += "\n2026-11-13 10:00:01 [ERROR] [m] after\n";
    std::istringstream in(input);

    AnalysisOptions opts;
    opts.max_line_length = 1024;
    Analysis a = analyze_stream(in, opts);

    CHECK(a.stats.total_lines == 3);
    CHECK(a.stats.parsed_lines == 2);
    CHECK(a.stats.error_counts[ParseError::TooLong] == 1);
    CHECK(a.issues.size() == 1);
}

TEST(analyzer_handles_binary_garbage) {
    std::string input = "2026-11-13 10:00:00 [ERROR] [m] ok\n";
    input += std::string("\x00\x01\x02\xFF\xFE", 5) + "\n";
    input += "2026-11-13 10:00:01 [ERROR] [m] ok again\n";
    std::istringstream in(input);
    Analysis a = analyze_stream(in);

    CHECK(a.stats.parsed_lines == 2);
    CHECK(a.stats.malformed_lines == 1);
    CHECK(a.stats.error_counts[ParseError::BinaryData] == 1);
}

TEST(analyzer_min_level_option) {
    std::istringstream in(
        "2026-11-13 10:00:00 [INFO] [m] hello\n"
        "2026-11-13 10:00:01 [DEBUG] [m] noisy\n");
    AnalysisOptions opts;
    opts.min_level = Level::Debug;
    Analysis a = analyze_stream(in, opts);
    CHECK(a.issues.size() == 1);  // both entries land in the same group
    CHECK(a.issues[0].count == 2);
}

TEST(read_bounded_line_limits_memory) {
    std::istringstream in(std::string(10000, 'x') + "\nshort\n");
    std::string line;
    CHECK(read_bounded_line(in, line, 100));
    CHECK(line.size() == 102);  // max_len + 2, remainder discarded
    CHECK(read_bounded_line(in, line, 100));
    CHECK(line == "short");
    CHECK(!read_bounded_line(in, line, 100));
}
