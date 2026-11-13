#include <charconv>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "logtriage/analyzer.hpp"
#include "logtriage/report.hpp"

namespace {

constexpr const char* kVersion = "1.0.0";

constexpr int kExitOk = 0;
constexpr int kExitP1Found = 1;
constexpr int kExitUsage = 2;
constexpr int kExitIo = 3;

void print_usage(std::ostream& out) {
    out << "LogTriage " << kVersion << " - embedded log analyzer and issue triage utility\n\n"
        << "Usage: logtriage <logfile | -> [options]\n\n"
        << "Options:\n"
        << "  --min-level LEVEL     Lowest level to triage: debug|info|warn|error|fatal\n"
        << "                        (default: warn)\n"
        << "  --format text|json    Output format (default: text)\n"
        << "  --top N               Show only the N highest-priority groups (0 = all, default 10)\n"
        << "  --max-line-length N   Lines longer than N bytes are counted as corrupt\n"
        << "                        (default: " << logtriage::kDefaultMaxLineLength << ")\n"
        << "  --fail-on-p1          Exit with status 1 if any P1 issue is found (for CI)\n"
        << "  -h, --help            Show this help\n"
        << "  --version             Show version\n\n"
        << "Use '-' as the file name to read from standard input.\n"
        << "Log line format: YYYY-MM-DD HH:MM:SS [LEVEL] [module] message\n";
}

bool parse_size(std::string_view text, std::size_t& value) {
    if (text.empty()) return false;
    std::size_t parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size()) return false;
    value = parsed;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::string path;
    std::string format = "text";
    std::size_t top_n = 10;
    bool fail_on_p1 = false;
    logtriage::AnalysisOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto next_value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "error: " << name << " requires a value\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "-h" || arg == "--help") {
            print_usage(std::cout);
            return kExitOk;
        } else if (arg == "--version") {
            std::cout << "logtriage " << kVersion << "\n";
            return kExitOk;
        } else if (arg == "--fail-on-p1") {
            fail_on_p1 = true;
        } else if (arg == "--min-level") {
            const char* v = next_value("--min-level");
            if (!v) return kExitUsage;
            const auto level = logtriage::parse_level(v);
            if (!level) {
                std::cerr << "error: unknown level '" << v << "'\n";
                return kExitUsage;
            }
            options.min_level = *level;
        } else if (arg == "--format") {
            const char* v = next_value("--format");
            if (!v) return kExitUsage;
            format = v;
            if (format != "text" && format != "json") {
                std::cerr << "error: --format must be 'text' or 'json'\n";
                return kExitUsage;
            }
        } else if (arg == "--top") {
            const char* v = next_value("--top");
            if (!v || !parse_size(v, top_n)) {
                std::cerr << "error: --top needs a non-negative integer\n";
                return kExitUsage;
            }
        } else if (arg == "--max-line-length") {
            const char* v = next_value("--max-line-length");
            if (!v || !parse_size(v, options.max_line_length) || options.max_line_length == 0) {
                std::cerr << "error: --max-line-length needs a positive integer\n";
                return kExitUsage;
            }
        } else if (arg.size() > 1 && arg[0] == '-') {
            std::cerr << "error: unknown option '" << arg << "'\n";
            print_usage(std::cerr);
            return kExitUsage;
        } else if (path.empty()) {
            path = std::string(arg);
        } else {
            std::cerr << "error: only one log file may be given\n";
            return kExitUsage;
        }
    }

    if (path.empty()) {
        print_usage(std::cerr);
        return kExitUsage;
    }

    logtriage::Analysis analysis;
    if (path == "-") {
        analysis = logtriage::analyze_stream(std::cin, options);
    } else {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            std::cerr << "error: cannot open '" << path << "'\n";
            return kExitIo;
        }
        analysis = logtriage::analyze_stream(file, options);
    }

    if (format == "json") {
        logtriage::print_json_report(std::cout, analysis, top_n);
    } else {
        logtriage::print_text_report(std::cout, analysis, options.min_level, top_n);
    }

    if (fail_on_p1) {
        for (const auto& issue : analysis.issues) {
            if (issue.priority == logtriage::Priority::P1) return kExitP1Found;
        }
    }
    return kExitOk;
}
