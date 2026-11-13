# LogTriage


**LogTriage** is a lightweight C++17 command-line tool that reads embedded-device logs, groups system faults by category, and ranks them by priority so the most urgent problems surface first.

- Streams logs line by line, so memory stays flat even on very large files
- Classifies faults (thermal, power, crash, memory, storage, network, driver)
- Assigns P1 to P4 priorities and groups repeated errors into a single issue
- Tolerates corrupted input (truncated lines, binary noise, bad timestamps) and reports what it skipped
- Text and JSON output, plus a CI-friendly exit code
- No third-party dependencies (STL only, tests included)

## Build

Requires a C++17 compiler (g++ 8+ or clang 7+) and CMake 3.14+.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The binary is `build/logtriage`.

## Usage

```
logtriage <logfile | -> [options]

  --min-level LEVEL     debug|info|warn|error|fatal   (default: warn)
  --format text|json    output format                  (default: text)
  --top N               show the N highest-priority groups (0 = all, default 10)
  --max-line-length N   lines longer than N bytes count as corrupt (default 4096)
  --fail-on-p1          exit with status 1 if any P1 issue is found
  -h, --help            show help
  --version             show version
```

Use `-` as the file name to read from standard input:

```bash
cat device.log | ./build/logtriage - --min-level error
```

### Exit codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | `--fail-on-p1` was given and a P1 issue was found |
| 2 | Invalid command-line usage |
| 3 | Could not open the log file |

### Example

```
$ ./build/logtriage samples/device.log --top 4

LogTriage report
================
Lines     : total=33 parsed=33 malformed=0 blank=0
Levels    : DEBUG=1 INFO=9 WARN=10 ERROR=12 FATAL=1

Issues (WARN and above): 9 group(s)
  PRI CATEGORY MODULE        LEVEL   COUNT  FIRST SEEN -> LAST SEEN
  P1  THERMAL  thermal       ERROR       3  2026-11-13 09:40:21 -> 2026-11-13 09:41:50
      e.g. "CPU overtemp: 96C, shutting down radios" (line 19)
  P1  POWER    power         ERROR       2  2026-11-13 10:05:33 -> 2026-11-13 10:20:48
      e.g. "battery undervoltage detected: 3.1V" (line 22)
  P1  CRASH    kernel        FATAL       1  2026-11-13 11:30:00 -> 2026-11-13 11:30:00
      e.g. "Kernel panic - not syncing: watchdog timeout" (line 30)
  P2  NETWORK  net           ERROR       9  2026-11-13 08:12:44 -> 2026-11-13 08:36:02
      e.g. "connection timed out to 10.0.0.5:8883" (line 7)
  ... 5 more group(s); use --top 0 to show all
```

## Log format

```
YYYY-MM-DD HH:MM:SS [LEVEL] [module] message
```

- **LEVEL** (case-insensitive): `DEBUG`, `INFO`, `WARN`/`WARNING`, `ERROR`/`ERR`, `FATAL`/`CRIT`/`CRITICAL`
- **module**: letters, digits and `_ - . / :`, up to 64 characters
- Windows (`\r\n`) line endings and extra spaces are accepted

## How triage works

1. **Classify.** The message is matched (case-insensitive, whole words only) against keyword rules. The first matching rule wins:

   | Category | Examples of keywords |
   |----------|----------------------|
   | THERMAL | overtemp, overheat, temperature |
   | POWER | brownout, undervoltage, battery |
   | CRASH | kernel panic, segfault, watchdog |
   | MEMORY | out of memory, oom, malloc |
   | STORAGE | I/O error, flash, filesystem, sector |
   | NETWORK | timeout, connection refused, link down |
   | DRIVER | i2c, spi, uart, firmware, sensor |
   | UNKNOWN | anything else |

2. **Group.** Entries at or above `--min-level` are grouped by (category, module). Each group records its count, first and last timestamp, and a sample message from its most severe entry.

3. **Prioritize.**

   | Priority | Rule |
   |----------|------|
   | P1 | any FATAL, or an ERROR in THERMAL, POWER or CRASH |
   | P2 | any other ERROR |
   | P3 | WARN seen 5 or more times (recurring) |
   | P4 | everything else |

   Groups are sorted by priority, then by count.

## Corrupted input handling

Lines that cannot be parsed are never fatal. They are counted by reason and shown in the report:

| Reason | Example |
|--------|---------|
| `BAD_TIMESTAMP` | truncated line, impossible date, free text |
| `BAD_LEVEL` | unknown level, missing brackets |
| `BAD_MODULE` | missing or invalid module |
| `MISSING_MESSAGE` | nothing after the module |
| `BINARY_DATA` | control characters from a garbled capture |
| `TOO_LONG` | line exceeds `--max-line-length` |

Try it with `samples/corrupted.log`.

## Performance

Input is read through a bounded line reader, so a log with no newlines cannot exhaust memory, and only per-group counters are kept. On a 62 MB synthetic log (1,000,000 lines, about 0.2% corrupted), measured in a Linux sandbox:

- Runtime: about 0.4 s
- Peak memory: about 13 MB, the same as for a 1,000-line file

Reproduce it:

```bash
python3 tools/gen_log.py 1000000 > /tmp/big.log
time ./build/logtriage /tmp/big.log
```

## Testing

`ctest` runs 38 unit tests and 4 end-to-end CLI checks. The unit tests cover:

- parser edge cases (truncated lines, bad dates, binary bytes, CRLF, over-long lines)
- keyword classification and priority rules
- issue grouping, sorting and the group cap
- stream analysis (empty input, missing trailing newline, a 2 MB single line)
- JSON escaping and report output

The test framework is a small header (`tests/test_util.hpp`), so nothing needs to be downloaded.

## Project layout

```
include/logtriage/   public headers
src/                 parser, classifier, analyzer, reports, CLI entry point
tests/               unit tests
samples/             device.log (realistic), corrupted.log (damaged input)
tools/gen_log.py     synthetic log generator for benchmarks
docs/DESIGN.md       design notes
```

## Limitations

- One log format (see above); other formats need a new parser
- Keyword rules are English-only and hard-coded in `src/triage.cpp`
- Timestamps are validated for shape and range, not for real calendar dates (for example Feb 30 is accepted)

## License

MIT, see [LICENSE](LICENSE).
