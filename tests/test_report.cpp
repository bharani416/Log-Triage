#include <sstream>
#include <string>

#include "logtriage/report.hpp"
#include "test_util.hpp"

using namespace logtriage;

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

Analysis sample_analysis() {
    std::istringstream in(
        "2026-11-13 10:00:00 [ERROR] [therm] CPU overtemp: 98C\n"
        "2026-11-13 10:00:01 [ERROR] [mmc] I/O error \"quoted\" \\ path\n"
        "this line is corrupt\n");
    return analyze_stream(in);
}

}  // namespace

TEST(json_escape_handles_special_characters) {
    CHECK(json_escape("a\"b") == "a\\\"b");
    CHECK(json_escape("a\\b") == "a\\\\b");
    CHECK(json_escape("line1\nline2") == "line1\\nline2");
    CHECK(json_escape("tab\there") == "tab\\there");
    CHECK(json_escape(std::string("x\x01y", 3)) == "x\\u0001y");
    CHECK(json_escape("plain") == "plain");
}

TEST(text_report_contains_summary_and_issues) {
    std::ostringstream out;
    print_text_report(out, sample_analysis(), Level::Warn, 10);
    const std::string text = out.str();
    CHECK(contains(text, "total=3"));
    CHECK(contains(text, "malformed=1"));
    CHECK(contains(text, "BAD_TIMESTAMP=1"));
    CHECK(contains(text, "P1"));
    CHECK(contains(text, "THERMAL"));
    CHECK(contains(text, "STORAGE"));
}

TEST(text_report_for_clean_input_says_none) {
    std::istringstream in("2026-11-13 10:00:00 [INFO] [boot] ok\n");
    std::ostringstream out;
    print_text_report(out, analyze_stream(in), Level::Warn, 10);
    CHECK(contains(out.str(), "none"));
}

TEST(text_report_honors_top_limit) {
    std::ostringstream out;
    print_text_report(out, sample_analysis(), Level::Warn, 1);
    CHECK(contains(out.str(), "1 more group(s)"));
}

TEST(json_report_is_well_formed_and_escaped) {
    std::ostringstream out;
    print_json_report(out, sample_analysis(), 0);
    const std::string json = out.str();
    CHECK(contains(json, "\"total_lines\": 3"));
    CHECK(contains(json, "\"malformed_lines\": 1"));
    CHECK(contains(json, "\"BAD_TIMESTAMP\": 1"));
    CHECK(contains(json, "\"priority\": \"P1\""));
    CHECK(contains(json, "\\\"quoted\\\""));  // quotes escaped
    CHECK(contains(json, "\\\\ path"));       // backslash escaped
    CHECK(json.front() == '{');
    CHECK(json.back() == '\n');
}

TEST(json_report_with_no_issues_has_empty_array) {
    std::istringstream in("");
    std::ostringstream out;
    print_json_report(out, analyze_stream(in), 0);
    CHECK(contains(out.str(), "\"issues\": []"));
}
