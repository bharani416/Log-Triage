#include "logtriage/triage.hpp"

#include <algorithm>
#include <cctype>

namespace logtriage {
namespace {

constexpr std::size_t kMaxSampleLength = 120;

std::string to_lower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

bool is_word_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }

// True if `needle` occurs in `haystack` not embedded inside a longer word
// (so "oom" matches "oom-killer" but not "room").
bool contains_word(const std::string& haystack, std::string_view needle) {
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        const std::size_t end = pos + needle.size();
        const bool left_ok = pos == 0 || !is_word_char(haystack[pos - 1]);
        const bool right_ok = end >= haystack.size() || !is_word_char(haystack[end]);
        if (left_ok && right_ok) return true;
        ++pos;
    }
    return false;
}

struct Rule {
    Category category;
    std::vector<std::string_view> keywords;
};

// Order matters: the first matching rule wins.
const std::vector<Rule>& rules() {
    static const std::vector<Rule> kRules = {
        {Category::Thermal,
         {"overtemp", "over-temp", "overheat", "overheating", "thermal", "temperature"}},
        {Category::Power,
         {"brownout", "undervoltage", "under-voltage", "voltage", "battery", "power loss",
          "power fail", "power failure"}},
        {Category::Crash,
         {"kernel panic", "kernel oops", "segfault", "segmentation fault", "watchdog", "wdt",
          "core dumped", "unexpected reboot", "stack overflow"}},
        {Category::Memory,
         {"out of memory", "oom", "oom-killer", "allocation", "malloc", "memory leak",
          "low memory", "heap"}},
        {Category::Storage,
         {"i/o error", "io error", "disk", "flash", "filesystem", "file system", "ext4",
          "sector", "read-only", "mount", "eeprom", "no space left", "bad block"}},
        {Category::Network,
         {"timeout", "timed out", "connection refused", "connection reset", "link down",
          "network unreachable", "unreachable", "dns", "dhcp", "packet loss", "wifi",
          "ethernet", "socket"}},
        {Category::Driver,
         {"driver", "firmware", "i2c", "spi", "uart", "gpio", "sensor", "bus error",
          "not responding", "probe failed"}},
    };
    return kRules;
}

}  // namespace

const char* to_string(Category category) {
    switch (category) {
        case Category::Crash:   return "CRASH";
        case Category::Thermal: return "THERMAL";
        case Category::Power:   return "POWER";
        case Category::Memory:  return "MEMORY";
        case Category::Storage: return "STORAGE";
        case Category::Network: return "NETWORK";
        case Category::Driver:  return "DRIVER";
        case Category::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* to_string(Priority priority) {
    switch (priority) {
        case Priority::P1: return "P1";
        case Priority::P2: return "P2";
        case Priority::P3: return "P3";
        case Priority::P4: return "P4";
    }
    return "P4";
}

Category classify(std::string_view message) {
    const std::string lowered = to_lower(message);
    for (const Rule& rule : rules()) {
        for (std::string_view keyword : rule.keywords) {
            if (contains_word(lowered, keyword)) return rule.category;
        }
    }
    return Category::Unknown;
}

Priority compute_priority(Category category, Level max_level, std::size_t count) {
    if (max_level == Level::Fatal) return Priority::P1;
    const bool critical_category = category == Category::Thermal ||
                                   category == Category::Power || category == Category::Crash;
    if (max_level == Level::Error && critical_category) return Priority::P1;
    if (max_level == Level::Error) return Priority::P2;
    if (max_level == Level::Warn && count >= kRecurringWarnThreshold) return Priority::P3;
    return Priority::P4;
}

Triager::Triager(Level min_level, std::size_t max_groups)
    : min_level_(min_level), max_groups_(max_groups) {}

void Triager::add(const LogEntry& entry) {
    if (entry.level < min_level_) return;

    Key key{classify(entry.message), entry.module};
    auto it = groups_.find(key);
    if (it == groups_.end()) {
        if (groups_.size() >= max_groups_) {
            ++dropped_;
            return;
        }
        Issue issue;
        issue.category = key.category;
        issue.module = key.module;
        issue.max_level = entry.level;
        issue.first_seen = entry.timestamp;
        issue.first_line = entry.line_no;
        issue.sample_message = entry.message.substr(0, kMaxSampleLength);
        it = groups_.emplace(std::move(key), std::move(issue)).first;
    }

    Issue& issue = it->second;
    ++issue.count;
    issue.last_seen = entry.timestamp;
    if (entry.level > issue.max_level) {
        issue.max_level = entry.level;
        issue.sample_message = entry.message.substr(0, kMaxSampleLength);
    }
}

std::vector<Issue> Triager::issues() const {
    std::vector<Issue> out;
    out.reserve(groups_.size());
    for (const auto& kv : groups_) {
        Issue issue = kv.second;
        issue.priority = compute_priority(issue.category, issue.max_level, issue.count);
        out.push_back(std::move(issue));
    }
    std::sort(out.begin(), out.end(), [](const Issue& a, const Issue& b) {
        if (a.priority != b.priority) return a.priority < b.priority;
        if (a.count != b.count) return a.count > b.count;
        return a.module < b.module;
    });
    return out;
}

}  // namespace logtriage
