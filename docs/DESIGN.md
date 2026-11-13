# LogTriage design notes

## Pipeline

```
istream -> read_bounded_line -> parse_line -> Triager::add -> issues() -> report
```

| Stage | File | Responsibility |
|-------|------|----------------|
| Read | `src/analyzer.cpp` | Reads one line at a time with a hard size cap |
| Parse | `src/parser.cpp` | Validates one line and returns an entry or an error code |
| Classify and group | `src/triage.cpp` | Keyword classification, grouping, priority |
| Report | `src/report.cpp` | Text and JSON output |
| CLI | `src/main.cpp` | Argument parsing, exit codes |

Each stage is a separate function with no global state, which keeps them easy to unit test.

## Memory behavior

- `std::getline` would load an entire line into memory, so a corrupted file with no newlines could use as much memory as the file size. `read_bounded_line` instead keeps at most `max_line_length + 2` characters and discards the rest.
- Entries are never stored. Each parsed entry updates a group in `Triager` and is then dropped.
- The number of groups is capped (`max_groups`, default 10,000). A file with thousands of random module names cannot grow the map without limit; skipped entries are counted and reported.

Memory use is therefore proportional to the number of distinct (category, module) pairs, not to file size.

## Error handling

`parse_line` never throws. It returns a `ParseResult` with either an entry or a `ParseError`. Blank lines are reported separately from corrupt lines so a log with spacing does not look damaged.

Validation order: trailing `\r` is stripped, then length, blank, control characters, timestamp, level, module, message. The first failure decides the reported reason.

## Classification

Rules are an ordered list of (category, keywords). Matching is case-insensitive and whole-word, so `oom` matches `oom-killer` but not `room`. Order encodes precedence, for example `connection reset` is checked as NETWORK and does not fall into CRASH.

## Priority

`compute_priority` is a pure function of (category, highest level, count), covered directly by tests. Recurring warnings (5 or more) are promoted to P3 so a slow-burning problem is not hidden behind one-off warnings.

## Extending

- **New category:** add an enum value and a `Rule` in `src/triage.cpp`, and a case in `to_string(Category)`.
- **New log format:** add a parser function that returns `ParseResult`, and call it from `analyze_stream`.
- **New output format:** add a function in `src/report.cpp` that takes an `Analysis`.
