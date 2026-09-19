# CMakeLists.txt for the SizeCheck test suite.

set(SIZECHECK_TEST_SOURCES
  test_main.cpp
  test_formatter.cpp
  test_cli.cpp
  test_scanner.cpp
  test_analyzer.cpp
  test_report.cpp)

add_executable(sizecheck_tests ${SIZECHECK_TEST_SOURCES})
target_link_libraries(sizecheck_tests PRIVATE sizecheck_core)
sizecheck_set_warnings(sizecheck_tests)
sizecheck_setup_sanitizers(sizecheck_tests)

add_test(NAME sizecheck_tests COMMAND sizecheck_tests)

# End-to-end CLI smoke tests, driven by CMake scripts (cross-platform,
# no shell involved).
add_test(
  NAME e2e_cli
  COMMAND ${CMAKE_COMMAND}
          -DBIN=$<TARGET_FILE:sizecheck>
          -DWORK=${CMAKE_CURRENT_BINARY_DIR}/e2e
          -P ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_cli_tests.cmake)