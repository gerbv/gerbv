#!/bin/bash
# Run drill-related tests from the gerbv test suite
# Uses the locally built gerbv binary

set -u

GERBV="${GERBV:-../build/src/gerbv}"
LD_LIBRARY_PATH="../build/src:${LD_LIBRARY_PATH:-}"
export LD_LIBRARY_PATH

GERBV_FLAGS="--export=png --window=640x480"
INDIR="./inputs"
OUTDIR="./outputs"
REFDIR="./golden"

mkdir -p "$OUTDIR"

pass=0
fail=0
tot=0

run_test() {
    local name="$1"
    local file="$2"
    local extra_flags="${3:-}"

    tot=$((tot + 1))
    echo "----------------------------------------------------------------------"
    echo "Test: $name"

    local flags="$GERBV_FLAGS"
    if [ -n "$extra_flags" ]; then
        flags="$flags $extra_flags"
    fi

    $GERBV $flags --output="$OUTDIR/$name.png" "$INDIR/$file" 2>/dev/null

    if [ ! -f "$REFDIR/$name.png" ]; then
        echo "  SKIP: no golden file"
        return
    fi

    # Use compare -metric MAE, capture stderr (where metric goes)
    local mae
    mae=$(compare -metric MAE "$REFDIR/$name.png" "$OUTDIR/$name.png" null: 2>&1 || true)
    local val=$(echo "$mae" | awk '{print $1}')

    if [ "$val" = "0" ]; then
        echo "  PASS (MAE=0)"
        pass=$((pass + 1))
    else
        echo "  FAILED (MAE=$val)"
        fail=$((fail + 1))
    fi
}

# Basic drill tests (autod=1, no FILE_FORMAT)
run_test "test-drill-trailing-zero-1" "test-drill-trailing-zero-1.exc"
run_test "test-drill-leading-zero-1" "test-drill-leading-zero-1.exc"
run_test "test-drill-trailing-zero-suppression" "test-drill-trailing-zero-suppression.exc" "-p inputs/test-drill-trailing-zero-suppression.gvp"
run_test "test-drill-repeat-1" "test-drill-repeat-1.exc"
run_test "test-drill-slot-drilled-g85" "test-drill-slot-drilled-g85.exc"
run_test "test-drill-spaces" "test-drill-spaces.exc"

# Excellon routing/milling (G00/G01 + M15/M16/M17)
run_test "test-drill-route-milling" "test-drill-route-milling.exc"

# Altium-style FILE_FORMAT tests (the ones affected by our fix)
run_test "Altium_file_format_inch" "Altium_file_format_inch.drl"
run_test "Altium_inch_file_format" "Altium_inch_file_format.drl"
run_test "LimeSDR-QPCIe_1v2-RoundHoles" "LimeSDR-QPCIe_1v2-RoundHoles.drl"
run_test "Altium_file_format_inch_lz" "Altium_file_format_inch_lz.drl"

echo "----------------------------------------------------------------------"
echo "Passed $pass, failed $fail out of $tot tests."
