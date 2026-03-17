runesprev ❯ # Run all tests                                                                                        main      
$ echo "=== TEST RESULTS ==="
for f in src/tests/samples/*.runes; do
  errors=$(./ast_tool "$f" 2>&1 | grep -c "\[Error\]")
  name=$(basename "$f")
  if [ "$errors" -eq 0 ]; then
    echo "✅ $name"
  else
    echo "❌ $name ($errors errors)"
  fi
done