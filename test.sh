#!/usr/bin/env bash

set -e

VM_STAT2="${1:-./build/vm_stat2}"
PASSED=0
FAILED=0

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

pass() {
    echo -e "${GREEN}PASS${NC}: $1"
    PASSED=$((PASSED + 1))
}

fail() {
    echo -e "${RED}FAIL${NC}: $1"
    echo "  Expected: $2"
    echo "  Got:      $3"
    FAILED=$((FAILED + 1))
}

test_success() {
    local desc="$1"
    shift
    if "$VM_STAT2" "$@" > /dev/null 2>&1; then
        pass "$desc"
    else
        fail "$desc" "exit 0" "exit $?"
    fi
}

test_failure() {
    local desc="$1"
    shift
    if "$VM_STAT2" "$@" > /dev/null 2>&1; then
        fail "$desc" "exit non-zero" "exit 0"
    else
        pass "$desc"
    fi
}

test_output_contains() {
    local desc="$1"
    local expected="$2"
    shift 2
    local output
    output=$("$VM_STAT2" "$@" 2>&1) || true
    if echo "$output" | grep -q "$expected"; then
        pass "$desc"
    else
        fail "$desc" "output contains '$expected'" "output: $output"
    fi
}

test_stderr_contains() {
    local desc="$1"
    local expected="$2"
    shift 2
    local stderr_output
    stderr_output=$("$VM_STAT2" "$@" 2>&1 >/dev/null) || true
    if echo "$stderr_output" | grep -q "$expected"; then
        pass "$desc"
    else
        fail "$desc" "stderr contains '$expected'" "stderr: $stderr_output"
    fi
}

group() {
    echo ""
    echo "--- $1 ---"
}
echo "Testing: $VM_STAT2"

group "Snapshot Mode Tests"
  test_success "No arguments (snapshot mode)"
  test_output_contains "Output contains 'Mach Virtual Memory'" "Mach Virtual Memory"

group "Unit Options"
  test_success "Unit option -b (bytes)"    -b
  test_success "Unit option -k (KB)"       -k
  test_success "Unit option -m (MB)"       -m
  test_success "Unit option -g (GB)"       -g

  test_output_contains "-b shows bytes" " B" -b
  test_output_contains "-k shows KB" "KB" -k
  test_output_contains "-m shows MB" "MB" -m
  test_output_contains "-g shows GB" "GB" -g

group "All Details Option"
  test_success "All details option -a" -a
  test_output_contains "-a shows Pages free" "Pages free" -a

group "Polling Mode Tests"
  test_success "Polling mode with interval" -c 1 1
  test_success "Polling with unit option" -m -c 1 1
  test_success "Polling with multiple options" -m -a -c 1 1

group "Argument Order (vm_stat compatibility)"
  test_failure "Invalid: interval before -c (vm_stat2 1 -c 2)" 1 -c 2
  test_stderr_contains "Error message for invalid order" "unexpected argument" 1 -c 2

group "Error Cases"
  test_failure "Unknown option -x" -x
  test_failure "Unknown option --help" --help

  test_failure "Invalid count: -c 0" -c 0 1
  test_failure "Invalid count: -c -1" -c -1 1
  test_stderr_contains "Error for -c 0" "count must be positive" -c 0 1

  # interval 0 or non-numeric -> treated as 0 (snapshot mode, like vm_stat)
  test_success "Interval 0 (treated as snapshot, like vm_stat)" 0
  test_success "Non-numeric interval (treated as 0, like vm_stat)" abc

group "Combined Options"
  test_success "Combined: -b -a" -b -a
  test_success "Combined: -m -c 1 1" -m -c 1 1
  test_success "Options can be combined: -ma" -ma
  test_success "Options can be combined: -bac 1 1" -bac 1 1

echo -e "\nResults: ${GREEN}${PASSED} passed${NC}, ${RED}${FAILED} failed${NC}"

if [ $FAILED -gt 0 ]; then
    exit 1
fi
exit 0
