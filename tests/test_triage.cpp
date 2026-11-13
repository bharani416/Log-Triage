#include "logtriage/triage.hpp"
#include "test_util.hpp"

using namespace logtriage;

namespace {

LogEntry make_entry(Level level, const std::string& module, const std::string& message,
                    const std::string& ts = "2026-11-13 10:00:00", std::size_t line = 1) {
    LogEntry e;
    e.timestamp = ts;
    e.level = level;
    e.module = module;
    e.message = message;
    e.line_no = line;
    return e;
}

}  // namespace

TEST(classify_known_categories) {
    CHECK(classify("Out of memory: killed process 412") == Category::Memory);
    CHECK(classify("oom-killer invoked") == Category::Memory);
    CHECK(classify("I/O error, dev mmcblk0, sector 2048") == Category::Storage);
    CHECK(classify("Remounting filesystem read-only") == Category::Storage);
    CHECK(classify("connection timed out to 10.0.0.5") == Category::Network);
    CHECK(classify("link down on eth0") == Category::Network);
    CHECK(classify("CPU overtemp: 98C") == Category::Thermal);
    CHECK(classify("battery undervoltage detected") == Category::Power);
    CHECK(classify("Kernel panic - not syncing") == Category::Crash);
    CHECK(classify("watchdog bark, resetting") == Category::Crash);
    CHECK(classify("i2c bus error on sensor 3") == Category::Driver);
}

TEST(classify_uses_whole_words_only) {
    CHECK(classify("boom in the room") == Category::Unknown);  // "oom" inside other words
    CHECK(classify("everything is fine") == Category::Unknown);
    CHECK(classify("") == Category::Unknown);
}

TEST(classify_connection_reset_is_network_not_crash) {
    CHECK(classify("connection reset by peer") == Category::Network);
}

TEST(priority_rules) {
    CHECK(compute_priority(Category::Unknown, Level::Fatal, 1) == Priority::P1);
    CHECK(compute_priority(Category::Thermal, Level::Error, 1) == Priority::P1);
    CHECK(compute_priority(Category::Power, Level::Error, 1) == Priority::P1);
    CHECK(compute_priority(Category::Crash, Level::Error, 1) == Priority::P1);
    CHECK(compute_priority(Category::Storage, Level::Error, 1) == Priority::P2);
    CHECK(compute_priority(Category::Network, Level::Warn, kRecurringWarnThreshold) ==
          Priority::P3);
    CHECK(compute_priority(Category::Network, Level::Warn, kRecurringWarnThreshold - 1) ==
          Priority::P4);
    CHECK(compute_priority(Category::Unknown, Level::Info, 100) == Priority::P4);
}

TEST(triager_groups_by_category_and_module) {
    Triager t(Level::Warn);
    t.add(make_entry(Level::Error, "mmc", "I/O error on mmcblk0", "2026-11-13 10:00:01", 10));
    t.add(make_entry(Level::Error, "mmc", "I/O error on mmcblk0", "2026-11-13 10:00:05", 20));
    t.add(make_entry(Level::Fatal, "mmc", "disk failure", "2026-11-13 10:00:09", 30));
    t.add(make_entry(Level::Error, "net", "connection refused", "2026-11-13 10:00:02", 15));

    auto issues = t.issues();
    CHECK(issues.size() == 2);

    // Fatal storage group sorts first (P1) and keeps the most severe sample.
    CHECK(issues[0].module == "mmc");
    CHECK(issues[0].count == 3);
    CHECK(issues[0].priority == Priority::P1);
    CHECK(issues[0].max_level == Level::Fatal);
    CHECK(issues[0].first_seen == "2026-11-13 10:00:01");
    CHECK(issues[0].last_seen == "2026-11-13 10:00:09");
    CHECK(issues[0].first_line == 10);
    CHECK(issues[0].sample_message == "disk failure");
    CHECK(issues[1].module == "net");
    CHECK(issues[1].priority == Priority::P2);
}

TEST(triager_ignores_entries_below_min_level) {
    Triager t(Level::Error);
    t.add(make_entry(Level::Info, "m", "all good"));
    t.add(make_entry(Level::Warn, "m", "slow response"));
    CHECK(t.issues().empty());
    t.add(make_entry(Level::Error, "m", "failed"));
    CHECK(t.issues().size() == 1);
}

TEST(triager_sorts_by_priority_then_count) {
    Triager t(Level::Warn);
    for (int i = 0; i < 6; ++i) t.add(make_entry(Level::Warn, "wifi", "packet loss high"));
    for (int i = 0; i < 2; ++i) t.add(make_entry(Level::Error, "fs", "mount failed"));
    t.add(make_entry(Level::Error, "therm", "overtemp 99C"));

    auto issues = t.issues();
    CHECK(issues.size() == 3);
    CHECK(issues[0].category == Category::Thermal);  // P1
    CHECK(issues[1].category == Category::Storage);  // P2
    CHECK(issues[2].category == Category::Network);  // P3
}

TEST(triager_caps_number_of_groups) {
    Triager t(Level::Warn, 2);
    t.add(make_entry(Level::Error, "a", "failure one"));
    t.add(make_entry(Level::Error, "b", "failure two"));
    t.add(make_entry(Level::Error, "c", "failure three"));  // dropped
    t.add(make_entry(Level::Error, "a", "failure again"));  // existing group still updated
    CHECK(t.issues().size() == 2);
    CHECK(t.dropped_entries() == 1);
}

TEST(triager_truncates_long_sample_messages) {
    Triager t(Level::Warn);
    t.add(make_entry(Level::Error, "m", std::string(1000, 'x')));
    CHECK(t.issues()[0].sample_message.size() == 120);
}
