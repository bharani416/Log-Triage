#pragma once

#include <array>
#include <cstddef>
#include <istream>
#include <map>
#include <vector>

#include "logtriage/log_entry.hpp"
#include "logtriage/parser.hpp"
#include "logtriage/triage.hpp"

namespace logtriage {

struct AnalysisOptions {
    Level min_level = Level::Warn;                       // entries below this are only counted
    std::size_t max_line_length = kDefaultMaxLineLength;  // longer lines are treated as corrupt
    std::size_t max_groups = 10000;                       // cap on distinct issue groups
};

struct Stats {
    std::size_t total_lines = 0;
    std::size_t blank_lines = 0;
    std::size_t parsed_lines = 0;
    std::size_t malformed_lines = 0;
    std::size_t dropped_entries = 0;  // entries skipped because of the group cap
    std::array<std::size_t, kLevelCount> level_counts{};
    std::map<ParseError, std::size_t> error_counts;
};

struct Analysis {
    Stats stats;
    std::vector<Issue> issues;
};

// Reads one line into `out`, keeping at most max_len + 2 characters and
// discarding the rest of an over-long line. Returns false at end of input.
// This keeps memory bounded even if the file contains a gigantic "line".
bool read_bounded_line(std::istream& in, std::string& out, std::size_t max_len);

// Streams through `in` line by line; memory use does not grow with file size.
Analysis analyze_stream(std::istream& in, const AnalysisOptions& options = {});

}  // namespace logtriage
