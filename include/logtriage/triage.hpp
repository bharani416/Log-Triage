#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "logtriage/log_entry.hpp"

namespace logtriage {

// Fault categories inferred from the message text.
enum class Category {
    Crash,
    Thermal,
    Power,
    Memory,
    Storage,
    Network,
    Driver,
    Unknown,
};

const char* to_string(Category category);

// Keyword-based classifier (case-insensitive, whole-word matching).
Category classify(std::string_view message);

// P1 = act now ... P4 = informational.
enum class Priority { P1 = 1, P2, P3, P4 };

const char* to_string(Priority priority);

constexpr std::size_t kRecurringWarnThreshold = 5;

Priority compute_priority(Category category, Level max_level, std::size_t count);

// A group of related log entries (same category + module).
struct Issue {
    Category category = Category::Unknown;
    std::string module;
    Level max_level = Level::Info;
    std::size_t count = 0;
    std::string first_seen;
    std::string last_seen;
    std::size_t first_line = 0;
    std::string sample_message;  // message of the most severe occurrence
    Priority priority = Priority::P4;
};

// Accumulates entries into issue groups using bounded memory:
// at most `max_groups` distinct (category, module) groups are tracked.
class Triager {
public:
    explicit Triager(Level min_level = Level::Warn, std::size_t max_groups = 10000);

    void add(const LogEntry& entry);

    // Sorted: priority (P1 first), then count (desc), then module name.
    std::vector<Issue> issues() const;

    // Entries ignored because the group limit was reached.
    std::size_t dropped_entries() const { return dropped_; }

private:
    struct Key {
        Category category;
        std::string module;
        bool operator<(const Key& other) const {
            if (category != other.category) return category < other.category;
            return module < other.module;
        }
    };

    Level min_level_;
    std::size_t max_groups_;
    std::size_t dropped_ = 0;
    std::map<Key, Issue> groups_;
};

}  // namespace logtriage
