cmake_minimum_required(VERSION 3.14)
project(LogTriage VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(LOGTRIAGE_BUILD_TESTS "Build unit tests" ON)

# Core library (parser, classifier, analyzer, reports)
add_library(logtriage_core
  src/log_entry.cpp
  src/parser.cpp
  src/triage.cpp
  src/analyzer.cpp
  src/report.cpp
)
target_include_directories(logtriage_core PUBLIC include)
target_compile_options(logtriage_core PRIVATE -Wall -Wextra -Wpedantic)

# Command-line tool
add_executable(logtriage src/main.cpp)
target_link_libraries(logtriage PRIVATE logtriage_core)
target_compile_options(logtriage PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS logtriage RUNTIME DESTINATION bin)

if(LOGTRIAGE_BUILD_TESTS)
  enable_testing()

  add_executable(logtriage_tests
    tests/test_main.cpp
    tests/test_parser.cpp
    tests/test_triage.cpp
    tests/test_analyzer.cpp
    tests/test_report.cpp
  )
  target_link_libraries(logtriage_tests PRIVATE logtriage_core)
  target_compile_options(logtriage_tests PRIVATE -Wall -Wextra -Wpedantic)

  add_test(NAME unit_tests COMMAND logtriage_tests)

  # End-to-end checks of the CLI against the bundled sample logs.
  add_test(NAME cli_sample_log
    COMMAND logtriage ${CMAKE_SOURCE_DIR}/samples/device.log)
  add_test(NAME cli_corrupted_log
    COMMAND logtriage ${CMAKE_SOURCE_DIR}/samples/corrupted.log --format json)
  add_test(NAME cli_fail_on_p1
    COMMAND logtriage ${CMAKE_SOURCE_DIR}/samples/device.log --fail-on-p1)
  set_tests_properties(cli_fail_on_p1 PROPERTIES WILL_FAIL TRUE)
  add_test(NAME cli_missing_file
    COMMAND logtriage ${CMAKE_SOURCE_DIR}/samples/does_not_exist.log)
  set_tests_properties(cli_missing_file PROPERTIES WILL_FAIL TRUE)
endif()
