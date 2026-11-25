#!/usr/bin/env bash
#
# mayhem/test.sh — runs Hypersomnia's own Catch unit tests (the suite the game runs at startup with
# --unit-tests-only), built by mayhem/build.sh as mayhem/build-test/unit_tests with upstream's
# normal flags. Each test case asserts values (readwrite round-trips, path sanitization, pools,
# typesafe sprintf/sscanf, ed25519 and identity certs, cosmos state, ...). The counts come from
# Catch's own summary; a missing summary is a failure.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}


RUNNER="$SRC/mayhem/build-test/unit_tests"
if [ ! -x "$RUNNER" ]; then
  echo "test runner $RUNNER missing — mayhem/build.sh should have built it" >&2
  emit_ctrf "catch2" 0 1 0
  exit 1
fi

WORK=$(mktemp -d "${TMPDIR:-/tmp}/hs-unit-tests.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
REPORT="$WORK/report.txt"

# Like the game, the tests run with the hypersomnia/ folder (content/, detail/) as cwd.
(cd "$SRC/hypersomnia" && "$RUNNER" "$REPORT" "$WORK/appdata")
rc=$?
cat "$REPORT" 2>/dev/null | tail -n 40

passed=0; failed=0
if line=$(grep -E '^All tests passed \(' "$REPORT" 2>/dev/null | tail -n1) && [ -n "$line" ]; then
  # All tests passed (1451 assertions in 52 test cases)
  passed=$(echo "$line" | sed -E 's/.* in ([0-9]+) test cases?\).*/\1/')
elif line=$(grep -E '^test cases:' "$REPORT" 2>/dev/null | tail -n1) && [ -n "$line" ]; then
  # test cases:   52 |   39 passed | 13 failed
  passed=$(echo "$line" | sed -nE 's/.* ([0-9]+) passed.*/\1/p'); passed=${passed:-0}
  failed=$(echo "$line" | sed -nE 's/.* ([0-9]+) failed.*/\1/p'); failed=${failed:-0}
else
  echo "no Catch summary in $REPORT (runner exit $rc)" >&2
  failed=1
fi

# A non-zero runner exit with a clean summary is still a failure.
if [ "$rc" -ne 0 ] && [ "$failed" -eq 0 ]; then failed=1; fi
# The suite has dozens of test cases; far fewer means it did not really run.
if [ "$failed" -eq 0 ] && [ "$passed" -lt 20 ]; then
  echo "only $passed test cases passed — expected the full suite" >&2
  failed=1
fi

emit_ctrf "catch2" "$passed" "$failed" 0
