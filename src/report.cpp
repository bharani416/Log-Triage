#include "logtriage/report.hpp"

#include <algorithm>
#include <cstdio>
#include <iomanip>

namespace logtriage {
namespace {

std::size_t limit_for(const Analysis& analysis, std::size_t top_n) {
    if (top_n == 0) return analysis.issues.size();
    return std::min(top_n, analysis.issues.size());
}

}  // namespace

std::string json_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 2);
    for (char c : text) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

void print_text_report(std::ostream& out, const Analysis& analysis, Level min_level,
                       std::size_t top_n) {
    const Stats& s = analysis.stats;

    out << "LogTriage report\n";
    out << "================\n";
    out << "Lines     : total=" << s.total_lines << " parsed=" << s.parsed_lines
        << " malformed=" << s.malformed_lines << " blank=" << s.blank_lines << "\n";

    out << "Levels    :";
    for (std::size_t i = 0; i < kLevelCount; ++i) {
        out << ' ' << to_string(static_cast<Level>(i)) << '=' << s.level_counts[i];
    }
    out << "\n";

    if (!s.error_counts.empty()) {
        out << "Malformed :";
        for (const auto& kv : s.error_counts) {
            out << ' ' << to_string(kv.first) << '=' << kv.second;
        }
        out << "\n";
    }
    if (s.dropped_entries > 0) {
        out << "Warning   : " << s.dropped_entries
            << " entries skipped (issue group limit reached)\n";
    }

    out << "\nIssues (" << to_string(min_level) << " and above): " << analysis.issues.size()
        << " group(s)\n";
    if (analysis.issues.empty()) {
        out << "  none\n";
        return;
    }

    out << "  " << std::left << std::setw(4) << "PRI" << std::setw(9) << "CATEGORY"
        << std::setw(14) << "MODULE" << std::setw(7) << "LEVEL" << std::right << std::setw(6)
        << "COUNT" << "  " << std::left << "FIRST SEEN -> LAST SEEN\n";

    const std::size_t shown = limit_for(analysis, top_n);
    for (std::size_t i = 0; i < shown; ++i) {
        const Issue& issue = analysis.issues[i];
        out << "  " << std::left << std::setw(4) << to_string(issue.priority) << std::setw(9)
            << to_string(issue.category) << std::setw(14) << issue.module.substr(0, 13)
            << std::setw(7) << to_string(issue.max_level) << std::right << std::setw(6)
            << issue.count << "  " << std::left << issue.first_seen << " -> "
            << issue.last_seen << "\n";
        out << "      e.g. \"" << issue.sample_message << "\" (line " << issue.first_line
            << ")\n";
    }
    if (shown < analysis.issues.size()) {
        out << "  ... " << (analysis.issues.size() - shown)
            << " more group(s); use --top 0 to show all\n";
    }
}

void print_json_report(std::ostream& out, const Analysis& analysis, std::size_t top_n) {
    const Stats& s = analysis.stats;

    out << "{\n  \"stats\": {\n";
    out << "    \"total_lines\": " << s.total_lines << ",\n";
    out << "    \"parsed_lines\": " << s.parsed_lines << ",\n";
    out << "    \"malformed_lines\": " << s.malformed_lines << ",\n";
    out << "    \"blank_lines\": " << s.blank_lines << ",\n";
    out << "    \"dropped_entries\": " << s.dropped_entries << ",\n";

    out << "    \"levels\": {";
    for (std::size_t i = 0; i < kLevelCount; ++i) {
        if (i) out << ", ";
        out << '"' << to_string(static_cast<Level>(i)) << "\": " << s.level_counts[i];
    }
    out << "},\n";

    out << "    \"errors\": {";
    bool first = true;
    for (const auto& kv : s.error_counts) {
        if (!first) out << ", ";
        first = false;
        out << '"' << to_string(kv.first) << "\": " << kv.second;
    }
    out << "}\n  },\n";

    out << "  \"issues\": [";
    const std::size_t shown = limit_for(analysis, top_n);
    for (std::size_t i = 0; i < shown; ++i) {
        const Issue& issue = analysis.issues[i];
        out << (i ? ",\n" : "\n");
        out << "    {\"priority\": \"" << to_string(issue.priority) << "\", "
            << "\"category\": \"" << to_string(issue.category) << "\", "
            << "\"module\": \"" << json_escape(issue.module) << "\", "
            << "\"max_level\": \"" << to_string(issue.max_level) << "\", "
            << "\"count\": " << issue.count << ", "
            << "\"first_seen\": \"" << json_escape(issue.first_seen) << "\", "
            << "\"last_seen\": \"" << json_escape(issue.last_seen) << "\", "
            << "\"first_line\": " << issue.first_line << ", "
            << "\"sample\": \"" << json_escape(issue.sample_message) << "\"}";
    }
    out << (shown ? "\n  ]\n" : "]\n") << "}\n";
}

}  // namespace logtriage
