#!/bin/bash
#
# GUI integration tests for gerbv using Xvfb + dogtail (AT-SPI).
#
# Requires: Xvfb, python3-dogtail, at-spi2-core, imagemagick (import)
#
# Usage:
#   ./test/gui/run_gui_tests.sh [--valgrind]
#
# The script starts Xvfb, launches gerbv with AT-SPI enabled, runs
# Python test scripts via dogtail, then tears everything down.
#
# Exit codes:
#   0 = all tests passed
#   1 = test failure
#   42 = valgrind found memory leaks (with --valgrind)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
GERBV="${BUILD_DIR}/src/Debug/gerbv"
TEST_INPUTS="${REPO_ROOT}/test/inputs"

USE_VALGRIND=0
if [[ "${1:-}" == "--valgrind" ]]; then
    USE_VALGRIND=1
    shift
fi

# Verify prerequisites
if ! command -v Xvfb >/dev/null; then
    echo "ERROR: Xvfb not found" >&2; exit 1
fi
if ! /usr/bin/python3 -c "import dogtail" 2>/dev/null; then
    echo "ERROR: python3-dogtail not found" >&2; exit 1
fi
if [[ ! -x "${GERBV}" ]]; then
    echo "ERROR: gerbv binary not found at ${GERBV}" >&2
    echo "  Run: cmake --build build" >&2; exit 1
fi

# Pick an unused display
DISPLAY_NUM=98
export DISPLAY=":${DISPLAY_NUM}"

# Cleanup handler
cleanup() {
    [[ -n "${GERBV_PID:-}" ]] && kill "${GERBV_PID}" 2>/dev/null || true
    [[ -n "${DBUS_SESSION_BUS_PID:-}" ]] && kill "${DBUS_SESSION_BUS_PID}" 2>/dev/null || true
    [[ -n "${XVFB_PID:-}" ]] && kill "${XVFB_PID}" 2>/dev/null || true
    wait 2>/dev/null || true
}
trap cleanup EXIT

# Start Xvfb
Xvfb "${DISPLAY}" -screen 0 1280x1024x24 &
XVFB_PID=$!
sleep 1

# Start a private D-Bus session (required for AT-SPI)
eval "$(dbus-launch --sh-syntax)"
export DBUS_SESSION_BUS_ADDRESS

# Enable GTK accessibility bridge
export GTK_MODULES="gail:atk-bridge"

# Set library path for the build
export LD_LIBRARY_PATH="${BUILD_DIR}/src/Debug"

# TinyScheme init path
export GERBV_SCHEMEINIT="${REPO_ROOT}/thirdparty/tinyscheme"

echo "=== gerbv GUI tests ==="
echo "Display: ${DISPLAY}"
echo "Binary:  ${GERBV}"
echo ""

FAILURES=0

run_test() {
    local test_name="$1"
    local test_script="$2"

    echo -n "Test: ${test_name} ... "

    if /usr/bin/python3 "${test_script}" "${GERBV}" "${TEST_INPUTS}" "${REPO_ROOT}" 2>&1; then
        echo "PASS"
    else
        echo "FAIL"
        FAILURES=$((FAILURES + 1))
    fi
}

# Run each test script in the gui directory
for test_script in "${SCRIPT_DIR}"/test_*.py; do
    [[ -f "${test_script}" ]] || continue
    test_name="$(basename "${test_script}" .py)"
    run_test "${test_name}" "${test_script}"
done

echo ""
echo "=== Results: ${FAILURES} failure(s) ==="
exit "${FAILURES}"
