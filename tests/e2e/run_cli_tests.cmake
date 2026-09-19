# Cross-platform end-to-end tests for the sizecheck CLI, driven by CMake so no
# shell is involved.  Invoked from tests/CMakeLists.txt via `cmake -P`.

# Required variables: BIN (path to the sizecheck executable), WORK (a scratch
# build directory to create test fixtures in).
set(sc_log_file "${WORK}/e2e.log")
file(MAKE_DIRECTORY "${WORK}")

# Start from a clean slate: the previous invocation may have left fixtures
# behind (this script builds state incrementally across steps).
file(REMOVE_RECURSE "${WORK}/tree")
file(REMOVE_RECURSE "${WORK}/tree2")

set(sc_failures 0)

function(sc_report MESSAGE)
  message(STATUS "[e2e] ${MESSAGE}")
  file(APPEND "${sc_log_file}" "${MESSAGE}\n")
endfunction()

function(sc_fail MESSAGE)
  message(FATAL_ERROR "[e2e] FAILED: ${MESSAGE}")
endfunction()

function(sc_assert_exit NAME EXPECTED EXIT)
  if(NOT "${EXIT}" EQUAL "${EXPECTED}")
    sc_fail("${NAME}: expected exit code ${EXPECTED}, got ${EXIT}")
  endif()
endfunction()

# ---------------------------------------------------------------------------

# --help prints usage and exits 0.
execute_process(COMMAND "${BIN}" --help
  RESULT_VARIABLE exit OUTPUT_VARIABLE out ERROR_VARIABLE err)
sc_assert_exit("--help" 0 "${exit}")
if(NOT out MATCHES "Usage:")
  sc_fail("--help output does not contain 'Usage:'")
endif()
sc_report("--help OK")

# --version prints a version and exits 0.
execute_process(COMMAND "${BIN}" --version
  RESULT_VARIABLE exit OUTPUT_VARIABLE out ERROR_VARIABLE err)
sc_assert_exit("--version" 0 "${exit}")
if(NOT out MATCHES "sizecheck [0-9]+\\.[0-9]+\\.[0-9]")
  sc_fail("--version output malformed: '${out}'")
endif()
sc_report("--version OK")

# Unknown options exit with code 2.
execute_process(COMMAND "${BIN}" --frobnicate
  RESULT_VARIABLE exit OUTPUT_VARIABLE out ERROR_VARIABLE err)
sc_assert_exit("unknown option" 2 "${exit}")
if(NOT err MATCHES "unknown option")
  sc_fail("unknown option error message not on stderr: '${err}'")
endif()
sc_report("unknown option OK")

# Nonexistent paths fail with exit code 1.
execute_process(COMMAND "${BIN}" "${WORK}/does-not-exist"
  RESULT_VARIABLE exit OUTPUT_VARIABLE out ERROR_VARIABLE err)
sc_assert_exit("missing path" 1 "${exit}")
if(NOT err MATCHES "does not exist")
  sc_fail("missing path message unexpected: '${err}'")
endif()
sc_report("missing path OK")

# Validate a real scan on fixtures.
file(MAKE_DIRECTORY "${WORK}/tree")
file(WRITE "${WORK}/tree/data.txt" "0123456789")
file(WRITE "${WORK}/tree/nested/small.txt" "abc")
file(MAKE_DIRECTORY "${WORK}/tree/empty")

execute_process(COMMAND "${BIN}" "${WORK}/tree"
  RESULT_VARIABLE exit OUTPUT_VARIABLE out ERROR_VARIABLE err)
sc_assert_exit("scan dir" 0 "${exit}")
if(NOT out MATCHES "Files:")
  sc_fail("scan output missing aggregate rows")
endif()
sc_report("scan dir OK")

# JSON output is valid enough for tools and reports the correct byte total
# (10 + 3 bytes).
execute_process(COMMAND "${BIN}" "${WORK}/tree" --json
  RESULT_VARIABLE exit OUTPUT_VARIABLE out ERROR_VARIABLE err)
sc_assert_exit("scan json" 0 "${exit}")
if(NOT out MATCHES "\"files\": 2")
  sc_fail("json files count wrong: '${out}'")
endif()
if(NOT out MATCHES "\"total_bytes\": 13")
  sc_fail("json total_bytes wrong: '${out}'")
endif()
if(NOT out MATCHES "\"largest_files\"")
  sc_fail("json missing largest_files: '${out}'")
endif()
sc_report("scan json OK")

# --hidden includes dotfiles.
file(WRITE "${WORK}/tree/.dot" "abcdef")
execute_process(COMMAND "${BIN}" "${WORK}/tree" --hidden --json
  RESULT_VARIABLE exit OUTPUT_VARIABLE out ERROR_VARIABLE err)
sc_assert_exit("scan hidden" 0 "${exit}")
if(NOT out MATCHES "\"files\": 3")
  sc_fail("--hidden did not include the dotfile: '${out}'")
endif()
sc_report("--hidden OK")

# --exclude skips a directory by name.  Hidden files stay excluded, so the
# remaining total is data.txt (10) + nested/small.txt (3) = 13.
file(MAKE_DIRECTORY "${WORK}/tree/build")
file(WRITE "${WORK}/tree/build/artifact.bin" "0123456789")
execute_process(COMMAND "${BIN}" "${WORK}/tree" --exclude build --json
  RESULT_VARIABLE exit OUTPUT_VARIABLE out ERROR_VARIABLE err)
sc_assert_exit("exclude" 0 "${exit}")
if(NOT out MATCHES "\"total_bytes\": 13")
  sc_fail("--exclude did not skip build dir: '${out}'")
endif()
sc_report("--exclude OK")

# --depth 0 restricts the walk to the top level: only data.txt is a file
# there (the dotfile stays hidden, the subdirectories are not descended).
execute_process(COMMAND "${BIN}" "${WORK}/tree" --depth 0 --json
  RESULT_VARIABLE exit OUTPUT_VARIABLE out ERROR_VARIABLE err)
sc_assert_exit("depth 0" 0 "${exit}")
if(NOT out MATCHES "\"files\": 1")
  sc_fail("--depth 0 files wrong: '${out}'")
endif()
sc_report("--depth OK")

# Single-file scans work.
execute_process(COMMAND "${BIN}" "${WORK}/tree/data.txt"
  RESULT_VARIABLE exit OUTPUT_VARIABLE out ERROR_VARIABLE err)
sc_assert_exit("single file" 0 "${exit}")
if(NOT out MATCHES "Total size:")
  sc_fail("single-file scan output missing totals")
endif()
sc_report("single file OK")

# Symlink safety: a directory symlink pointing outside the tree must not be
# followed (POSIX; Windows needs privileges to create symlinks).
if(NOT WIN32)
  file(MAKE_DIRECTORY "${WORK}/outside")
  file(WRITE "${WORK}/outside/secret.big" "012345678901234567890")
  file(REMOVE_RECURSE "${WORK}/tree2")
  file(MAKE_DIRECTORY "${WORK}/tree2")
  file(WRITE "${WORK}/tree2/ok.txt" "xyz")
  file(CREATE_LINK "${WORK}/outside" "${WORK}/tree2/leak"
       RESULT result SYMBOLIC)
  execute_process(COMMAND "${BIN}" "${WORK}/tree2" --json
    RESULT_VARIABLE exit OUTPUT_VARIABLE out ERROR_VARIABLE err)
  sc_assert_exit("symlink escape" 0 "${exit}")
  if(NOT out MATCHES "\"total_bytes\": 3")
    sc_fail("symlink escape was followed: '${out}'")
  endif()
  sc_report("symlink escape OK")
else()
  sc_report("symlink escape SKIPPED on Windows")
endif()

sc_report("ALL E2E TESTS PASSED")