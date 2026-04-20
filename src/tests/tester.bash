#!/bin/bash
# Run all Runes tests with expected-failure support
# Convention: first line "-- EXPECT FAIL: <pattern>" marks expected-failure tests

PASS=0
FAIL=0
XPASS=0  # expected failures that passed correctly

echo "=== TEST RESULTS ==="
for f in src/tests/samples/*.runes; do
  name=$(basename "$f")

  # Check for expected-failure marker on first line
  expected_pattern=$(head -1 "$f" | sed -n 's/^-- EXPECT FAIL: //p')

  output=$(./runes src/std/prelude.runes "$f" 2>&1)
  status=$?

  if [ -n "$expected_pattern" ]; then
    # Expected-failure test
    if [ $status -ne 0 ] && echo "$output" | grep -q "$expected_pattern"; then
      echo "PASS (expected failure) $name"
      XPASS=$((XPASS + 1))
    elif [ $status -eq 0 ]; then
      echo "FAIL $name (expected failure but compiled successfully)"
      FAIL=$((FAIL + 1))
    else
      echo "FAIL $name (expected pattern '$expected_pattern' not found in output)"
      FAIL=$((FAIL + 1))
    fi
  else
    # Normal test -- should compile successfully
    if [ $status -eq 0 ]; then
      echo "PASS $name"
      PASS=$((PASS + 1))
    else
      errors=$(echo "$output" | grep -c "Error")
      echo "FAIL $name ($errors errors)"
      FAIL=$((FAIL + 1))
    fi
  fi
done

echo ""
echo "=== SUMMARY ==="
echo "Passed: $PASS"
echo "Expected failures: $XPASS"
echo "Failed: $FAIL"
TOTAL=$((PASS + XPASS + FAIL))
echo "Total: $TOTAL"

if [ $FAIL -gt 0 ]; then
  exit 1
else
  exit 0
fi
