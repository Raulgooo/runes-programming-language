#!/bin/bash
# Run all tests
echo "=== TEST RESULTS ==="
for f in src/tests/samples/*.runes; do
  output=$(./runes "$f" 2>&1)
  status=$?
  name=$(basename "$f")
  if [ $status -eq 0 ]; then
    echo "✅ $name"
  else
    errors=$(echo "$output" | grep -c "Error")
    echo "❌ $name ($errors errors)"
  fi
done