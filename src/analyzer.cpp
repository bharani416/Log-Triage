#include "logtriage/analyzer.hpp"

#include <cstdio>
#include <streambuf>
#include <string>

namespace logtriage {

bool read_bounded_line(std::istream& in, std::string& out, std::size_t max_len) {
    out.clear();
    std::streambuf* buf = in.rdbuf();
    if (buf == nullptr) return false;

    // One slot for a trailing '\r', one to signal "longer than max_len".
    const std::size_t capacity = max_len + 2;
    bool any = false;
    for (;;) {
        const int c = buf->sbumpc();
        if (c == std::char_traits<char>::eof()) return any;
        any = true;
        if (c == '\n') return true;
        if (out.size() < capacity) out.push_back(static_cast<char>(c));
    }
}

Analysis analyze_stream(std::istream& in, const AnalysisOptions& options) {
    Analysis result;
    Stats& stats = result.stats;
    Triager triager(options.min_level, options.max_groups);

    std::string line;
    std::size_t line_no = 0;
    while (read_bounded_line(in, line, options.max_line_length)) {
        ++line_no;
        ++stats.total_lines;

        ParseResult parsed = parse_line(line, line_no, options.max_line_length);
        if (parsed.entry) {
            ++stats.parsed_lines;
            ++stats.level_counts[static_cast<std::size_t>(parsed.entry->level)];
            triager.add(*parsed.entry);
        } else if (parsed.error == ParseError::Empty) {
            ++stats.blank_lines;
        } else {
            ++stats.malformed_lines;
            ++stats.error_counts[parsed.error];
        }
    }

    stats.dropped_entries = triager.dropped_entries();
    result.issues = triager.issues();
    return result;
}

}  // namespace logtriage
