#!/usr/bin/env bash
set -euo pipefail

compiler="${1:-}"
if [[ -z "$compiler" ]]; then
  for candidate in \
    "build/Firefly_compiler" \
    "build/Debug/Firefly_compiler" \
    "cmake-build-llvm/Firefly_compiler" \
    "cmake-build-debug/Firefly_compiler"; do
    if [[ -x "$candidate" ]]; then
      compiler="$candidate"
      break
    fi
  done
fi

if [[ -z "$compiler" || ! -x "$compiler" ]]; then
  echo "compiler executable not found; pass path/to/Firefly_compiler" >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tmp="$script_dir/.tmp"
rm -rf "$tmp"
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT

assert_equal() {
  local name="$1"
  local actual="$2"
  local expected="$3"
  if [[ "$(printf '%s' "$actual" | tr -d '\r\n')" != "$expected" ]]; then
    echo "$name failed: expected '$expected', got '$actual'" >&2
    exit 1
  fi
}

assert_contains() {
  local name="$1"
  local file="$2"
  local pattern="$3"
  if ! grep -Eq "$pattern" "$file"; then
    echo "$name failed: missing pattern '$pattern'" >&2
    exit 1
  fi
}

loop_file="$tmp/loop_assign.ff"
cat > "$loop_file" <<'FF'
var i = 0;
var sum = 0;

while (i < 5) {
  sum = sum + i;
  i = i + 1;
};

sum;
FF

loop_eval="$("$compiler" --eval "$loop_file")"
assert_equal "loop eval" "$loop_eval" "10"

"$compiler" --emit-llvm "$loop_file" >/dev/null
loop_ir="$tmp/loop_assign.ll"
assert_contains "loop IR" "$loop_ir" "while\.cond"
assert_contains "loop IR" "$loop_ir" "while\.body"
assert_contains "loop IR" "$loop_ir" "while\.end"
assert_contains "loop IR" "$loop_ir" "store i64"

"$compiler" --emit-obj "$loop_file" >/dev/null
loop_object="$tmp/loop_assign.o"
if [[ ! -f "$loop_object" ]]; then
  echo "loop emit-obj failed: object file was not created" >&2
  exit 1
fi

"$compiler" --build "$loop_file" >/dev/null
loop_exe="$tmp/loop_assign"
if [[ ! -x "$loop_exe" ]]; then
  echo "loop build failed: executable was not created" >&2
  exit 1
fi
"$loop_exe"

fib_file="$tmp/fib.ff"
cat > "$fib_file" <<'FF'
var fib = func(n) {
  if (n < 2) {
    n
  } else {
    fib(n - 1) + fib(n - 2)
  }
};

fib(6);
FF

fib_eval="$("$compiler" --eval "$fib_file")"
assert_equal "fib eval" "$fib_eval" "8"

"$compiler" --emit-llvm "$fib_file" >/dev/null
fib_ir="$tmp/fib.ll"
assert_contains "fib IR" "$fib_ir" "define i64 @fib"
assert_contains "fib IR" "$fib_ir" "call i64 @fib"
assert_contains "fib IR" "$fib_ir" "ret i64 %iftmp"

bool_file="$tmp/bool_assign.ff"
cat > "$bool_file" <<'FF'
var flag = false;
flag = !flag;

if (flag) {
  1
} else {
  0
};
FF

bool_eval="$("$compiler" --eval "$bool_file")"
assert_equal "bool eval" "$bool_eval" "1"

"$compiler" --emit-llvm "$bool_file" >/dev/null
bool_ir="$tmp/bool_assign.ll"
assert_contains "bool IR" "$bool_ir" "store i1"
assert_contains "bool IR" "$bool_ir" "nottmp"

echo "LLVM smoke tests passed"
