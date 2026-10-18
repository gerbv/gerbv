#!/usr/bin/env bash
set -euo pipefail

test_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root_dir="$(cd -- "$test_dir/.." && pwd)"
gerbv_bin="${GERBV:-$root_dir/build/src/gerbv}"

if [[ ! -x "$gerbv_bin" ]]; then
    echo "ERROR: gerbv executable not found: $gerbv_bin" >&2
    exit 2
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
export LC_ALL=C

run_export() {
    local input="$1"
    local output="$2"

    if ! "$gerbv_bin" --export=drill --output="$output" "$input" \
        >"$tmpdir/gerbv.log" 2>&1; then
        cat "$tmpdir/gerbv.log" >&2
        return 1
    fi

    if [[ ! -s "$output" ]]; then
        echo "FAIL: empty output: $output" >&2
        return 1
    fi
}

check_line() {
    local file="$1"
    local expected="$2"

    if ! grep -Fqx -- "$expected" "$file"; then
        echo "FAIL: expected line: $expected" >&2
        echo "Actual output:" >&2
        cat "$file" >&2
        return 1
    fi
}

drill="$tmpdir/drill.drl"
run_export "$test_dir/inputs/test-negative-drill-coordinates.gbr" "$drill"

check_line "$drill" 'X-001181Y001181'
check_line "$drill" 'X001181Y001181'

if grep -Fqx -- 'X-01181Y001181' "$drill"; then
    echo "FAIL: original negative-coordinate bug reproduced" >&2
    exit 1
fi

echo "PASS: ordinary drill negative and positive coordinates"

slot="$tmpdir/slot.drl"
run_export "$test_dir/inputs/test-negative-drill-slots.gbr" "$slot"

check_line "$slot" 'X-001181Y-001181G85X001181Y001181'
check_line "$slot" 'X001181Y001181G85X-001181Y-001181'

echo "PASS: slot start and end coordinates"
