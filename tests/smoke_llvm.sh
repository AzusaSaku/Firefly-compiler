#!/usr/bin/env bash
set -euo pipefail

compiler="${1:-}"
if [[ -z "$compiler" ]]; then
  for candidate in \
    "build/firefly" \
    "build/Debug/firefly" \
    "cmake-build-llvm/firefly" \
    "cmake-build-debug/firefly"; do
    if [[ -x "$candidate" ]]; then
      compiler="$candidate"
      break
    fi
  done
fi

if [[ -z "$compiler" || ! -x "$compiler" ]]; then
  echo "compiler executable not found; pass path/to/firefly" >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
tmp="$script_dir/.tmp"
rm -rf "$tmp"
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT

object_ext=".o"
exe_ext=""
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    object_ext=".obj"
    exe_ext=".exe"
    ;;
esac

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

assert_fails_contains() {
  local name="$1"
  local pattern="$2"
  shift 2

  set +e
  local output
  output="$("$@" 2>&1)"
  local status=$?
  set -e

  if [[ $status -eq 0 ]]; then
    echo "$name failed: command succeeded unexpectedly" >&2
    exit 1
  fi

  if ! grep -Eq "$pattern" <<<"$output"; then
    echo "$name failed: missing pattern '$pattern'" >&2
    echo "$output" >&2
    exit 1
  fi
}

assert_interpreter_native_marker() {
  local name="$1"
  local marker="$2"
  local file="$tmp/$name.ff"
  cat > "$file"

  local eval_output
  eval_output="$("$compiler" --eval "$file")"
  if ! grep -Fq "$marker" <<<"$eval_output"; then
    echo "$name eval failed: missing marker '$marker'" >&2
    exit 1
  fi

  "$compiler" --build "$file" >/dev/null
  local exe="$tmp/$name$exe_ext"
  if [[ ! -f "$exe" ]]; then
    echo "$name build failed: executable was not created" >&2
    exit 1
  fi

  local native_output
  native_output="$("$exe")"
  if ! grep -Fq "$marker" <<<"$native_output"; then
    echo "$name executable failed: missing marker '$marker'" >&2
    exit 1
  fi
}

section() {
  printf '\n== %s ==\n' "$1"
}

section "release/package smoke"
version_output="$("$compiler" --version)"
if [[ "$version_output" != "firefly 0.1.0" ]]; then
  echo "version command failed: $version_output" >&2
  exit 1
fi

help_output="$("$compiler" --help)"
for pattern in \
  "--eval <file>" \
  "--ast <file>" \
  "--emit-llvm <file>" \
  "--emit-obj <file>" \
  "--build <file>" \
  "--compile <file>" \
  "--version" \
  "--help"; do
  if ! grep -Fq -- "$pattern" <<<"$help_output"; then
    echo "help command failed: missing '$pattern'" >&2
    exit 1
  fi
done

section "interpreter behavior"
interpreter_example="$repo_root/examples/interpreter_features.ff"
interpreter_example_eval="$("$compiler" --eval "$interpreter_example")"
assert_equal "interpreter example eval" "$interpreter_example_eval" "3"

llvm_example="$repo_root/examples/llvm_basic.ff"
llvm_example_eval="$("$compiler" --eval "$llvm_example")"
assert_equal "LLVM example eval" "$llvm_example_eval" "8"

section "LLVM IR generation"
llvm_example_tmp="$tmp/example_llvm_basic.ff"
cp "$llvm_example" "$llvm_example_tmp"
"$compiler" --emit-llvm "$llvm_example_tmp" >/dev/null

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

let_file="$tmp/let_var.ff"
cat > "$let_file" <<'FF'
let answer = 42;
var total = answer;
total = total + 1;

total;
FF

let_eval="$("$compiler" --eval "$let_file")"
assert_equal "let/var eval" "$let_eval" "43"

"$compiler" --emit-llvm "$let_file" >/dev/null
let_ir="$tmp/let_var.ll"
assert_contains "let/var IR" "$let_ir" "answer"
assert_contains "let/var IR" "$let_ir" "total"

typed_function_file="$tmp/typed_function.ff"
cat > "$typed_function_file" <<'FF'
let inc = func(n: int): int {
  n + 1
};

inc(41);
FF

typed_function_eval="$("$compiler" --eval "$typed_function_file")"
assert_equal "typed function eval" "$typed_function_eval" "42"

"$compiler" --emit-llvm "$typed_function_file" >/dev/null
typed_function_ir="$tmp/typed_function.ll"
assert_contains "typed function IR" "$typed_function_ir" "define i64 @inc\\(i64 %n\\)"
assert_contains "typed function IR" "$typed_function_ir" "call i64 @inc"

copy_value_file="$tmp/copy_value.ff"
cat > "$copy_value_file" <<'FF'
let x = 10;
let y = x;

x + y;
FF

copy_value_eval="$("$compiler" --eval "$copy_value_file")"
assert_equal "copy value eval" "$copy_value_eval" "20"

move_reassign_file="$tmp/move_reassign.ff"
cat > "$move_reassign_file" <<'FF'
var data = [1, 2];
let moved = data;
data = [3, 4];

data[0];
FF

move_reassign_eval="$("$compiler" --eval "$move_reassign_file")"
assert_equal "move reassign eval" "$move_reassign_eval" "3"

typed_array_file="$tmp/typed_array.ff"
cat > "$typed_array_file" <<'FF'
let first = func(xs: array<int>): int {
  xs[0]
};

first([41, 1]);
FF

typed_array_eval="$("$compiler" --eval "$typed_array_file")"
assert_equal "typed array eval" "$typed_array_eval" "41"

borrow_file="$tmp/borrow.ff"
cat > "$borrow_file" <<'FF'
let data = [1, 2];
let borrowed: &array<int> = &data;

data[0];
FF

borrow_eval="$("$compiler" --eval "$borrow_file")"
assert_equal "borrow eval" "$borrow_eval" "1"

borrow_param_file="$tmp/borrow_param.ff"
cat > "$borrow_param_file" <<'FF'
let first = func(xs: &array<int>): int {
  xs[0]
};

let values = [41, 1];
first(&values) + values[1];
FF

borrow_param_eval="$("$compiler" --eval "$borrow_param_file")"
assert_equal "borrow param eval" "$borrow_param_eval" "42"

mutable_borrow_file="$tmp/mutable_borrow.ff"
cat > "$mutable_borrow_file" <<'FF'
var data = [1, 2];
let borrowed: &var array<int> = &var data;

1;
FF

mutable_borrow_eval="$("$compiler" --eval "$mutable_borrow_file")"
assert_equal "mutable borrow eval" "$mutable_borrow_eval" "1"

borrow_scope_file="$tmp/borrow_scope.ff"
cat > "$borrow_scope_file" <<'FF'
var data = [1, 2];
if (true) {
  let borrowed = &data;
  0
};
let moved = data;

1;
FF

borrow_scope_eval="$("$compiler" --eval "$borrow_scope_file")"
assert_equal "borrow scope eval" "$borrow_scope_eval" "1"

section "native object/executable build"
"$compiler" --emit-obj "$loop_file" >/dev/null
loop_object="$tmp/loop_assign$object_ext"
if [[ ! -f "$loop_object" ]]; then
  echo "loop emit-obj failed: object file was not created" >&2
  exit 1
fi

"$compiler" --build "$loop_file" >/dev/null
loop_exe="$tmp/loop_assign$exe_ext"
if [[ ! -f "$loop_exe" ]]; then
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

print_file="$tmp/print_string.ff"
cat > "$print_file" <<'FF'
puts("firefly native");
0;
FF

print_eval="$("$compiler" --eval "$print_file")"
if ! grep -Fq "firefly native" <<<"$print_eval"; then
  echo "print eval failed: missing output" >&2
  exit 1
fi

"$compiler" --emit-llvm "$print_file" >/dev/null
print_ir="$tmp/print_string.ll"
assert_contains "print IR declaration" "$print_ir" "declare i32 @puts"
assert_contains "print IR call" "$print_ir" "call i32 @puts"

"$compiler" --build "$print_file" >/dev/null
print_exe="$tmp/print_string$exe_ext"
if [[ ! -f "$print_exe" ]]; then
  echo "print build failed: executable was not created" >&2
  exit 1
fi
native_print="$("$print_exe")"
if ! grep -Fq "firefly native" <<<"$native_print"; then
  echo "print executable failed: missing output" >&2
  exit 1
fi

section "interpreter/native consistency"
assert_interpreter_native_marker "consistency_arithmetic" "consistency arithmetic" <<'FF'
if (1 + 2 * 3 == 7) {
  puts("consistency arithmetic")
} else {
  puts("bad arithmetic")
};
FF

assert_interpreter_native_marker "consistency_if" "consistency if" <<'FF'
if (true) {
  puts("consistency if")
} else {
  puts("bad if")
};
FF

assert_interpreter_native_marker "consistency_while" "consistency while" <<'FF'
var i = 0;
var sum = 0;

while (i < 5) {
  sum = sum + i;
  i = i + 1;
};

if (sum == 10) {
  puts("consistency while")
} else {
  puts("bad while")
};
FF

assert_interpreter_native_marker "consistency_function" "consistency function" <<'FF'
let inc = func(n: int): int {
  n + 1
};

if (inc(41) == 42) {
  puts("consistency function")
} else {
  puts("bad function")
};
FF

assert_interpreter_native_marker "consistency_printing" "consistency printing" <<'FF'
puts("consistency printing");
FF

section "parser and semantic errors"
parser_location_file="$tmp/parser_location.ff"
cat > "$parser_location_file" <<'FF'
let = 1;
FF

assert_fails_contains "parser diagnostic location" "parser error: expected next token to be IDENT, got ASSIGN instead at line 1, column 5" "$compiler" --eval "$parser_location_file"

semantic_location_file="$tmp/semantic_location.ff"
cat > "$semantic_location_file" <<'FF'
if (1) {
  1
};
FF

assert_fails_contains "semantic diagnostic location" "semantic error: if condition must be bool, got int at line 1, column 1" "$compiler" --eval "$semantic_location_file"

immutable_file="$tmp/immutable_assign.ff"
cat > "$immutable_file" <<'FF'
let x = 1;
x = 2;
FF

assert_fails_contains "immutable eval" "cannot assign to immutable binding: x" "$compiler" --eval "$immutable_file"
assert_fails_contains "immutable emit-llvm" "cannot assign to immutable binding: x" "$compiler" --emit-llvm "$immutable_file"

type_mismatch_file="$tmp/type_mismatch.ff"
cat > "$type_mismatch_file" <<'FF'
var x = 1;
x = true;
FF

assert_fails_contains "type mismatch eval" "semantic error: type mismatch in assignment to 'x': int vs bool" "$compiler" --eval "$type_mismatch_file"
assert_fails_contains "type mismatch emit-llvm" "semantic error: type mismatch in assignment to 'x': int vs bool" "$compiler" --emit-llvm "$type_mismatch_file"

binding_type_mismatch_file="$tmp/binding_type_mismatch.ff"
cat > "$binding_type_mismatch_file" <<'FF'
let x: bool = 1;
FF

assert_fails_contains "binding type mismatch eval" "semantic error: type mismatch in binding 'x': bool vs int" "$compiler" --eval "$binding_type_mismatch_file"

condition_mismatch_file="$tmp/condition_mismatch.ff"
cat > "$condition_mismatch_file" <<'FF'
if (1) {
  1
};
FF

assert_fails_contains "condition mismatch eval" "semantic error: if condition must be bool, got int" "$compiler" --eval "$condition_mismatch_file"

argument_type_mismatch_file="$tmp/argument_type_mismatch.ff"
cat > "$argument_type_mismatch_file" <<'FF'
let inc = func(n: int): int {
  n + 1
};

inc(true);
FF

assert_fails_contains "argument type mismatch eval" "semantic error: type mismatch in argument 1: int vs bool" "$compiler" --eval "$argument_type_mismatch_file"

return_type_mismatch_file="$tmp/return_type_mismatch.ff"
cat > "$return_type_mismatch_file" <<'FF'
let bad = func(): int {
  true
};

bad();
FF

assert_fails_contains "return type mismatch eval" "semantic error: type mismatch in function return: int vs bool" "$compiler" --eval "$return_type_mismatch_file"

mixed_array_file="$tmp/mixed_array.ff"
cat > "$mixed_array_file" <<'FF'
let values = [1, true];
FF

assert_fails_contains "mixed array eval" "semantic error: type mismatch in array element: int vs bool" "$compiler" --eval "$mixed_array_file"

array_binding_mismatch_file="$tmp/array_binding_mismatch.ff"
cat > "$array_binding_mismatch_file" <<'FF'
let values: array<int> = [true];
FF

assert_fails_contains "array binding mismatch eval" "semantic error: type mismatch in binding 'values' element: int vs bool" "$compiler" --eval "$array_binding_mismatch_file"

array_index_assignment_mismatch_file="$tmp/array_index_assignment_mismatch.ff"
cat > "$array_index_assignment_mismatch_file" <<'FF'
var values: array<int> = [1, 2];
values[0] = true;
FF

assert_fails_contains "array index assignment mismatch eval" "semantic error: type mismatch in index assignment: int vs bool" "$compiler" --eval "$array_index_assignment_mismatch_file"

borrow_type_mismatch_file="$tmp/borrow_type_mismatch.ff"
cat > "$borrow_type_mismatch_file" <<'FF'
let data = [1, 2];
let borrowed: &array<bool> = &data;
FF

assert_fails_contains "borrow type mismatch eval" "semantic error: type mismatch in binding 'borrowed' reference element: bool vs int" "$compiler" --eval "$borrow_type_mismatch_file"

mutable_borrow_immutable_file="$tmp/mutable_borrow_immutable.ff"
cat > "$mutable_borrow_immutable_file" <<'FF'
let data = [1, 2];
let borrowed = &var data;
FF

assert_fails_contains "mutable borrow immutable eval" "semantic error: cannot mutably borrow immutable binding: data" "$compiler" --eval "$mutable_borrow_immutable_file"

move_borrowed_file="$tmp/move_borrowed.ff"
cat > "$move_borrowed_file" <<'FF'
let data = [1, 2];
let borrowed = &data;
let moved = data;
FF

assert_fails_contains "move borrowed eval" "semantic error: cannot move borrowed value: data" "$compiler" --eval "$move_borrowed_file"

assign_borrowed_file="$tmp/assign_borrowed.ff"
cat > "$assign_borrowed_file" <<'FF'
var data = [1, 2];
let borrowed = &data;
data = [3, 4];
FF

assert_fails_contains "assign borrowed eval" "semantic error: cannot assign to borrowed value: data" "$compiler" --eval "$assign_borrowed_file"

index_assign_borrowed_file="$tmp/index_assign_borrowed.ff"
cat > "$index_assign_borrowed_file" <<'FF'
var data = [1, 2];
let borrowed = &data;
data[0] = 3;
FF

assert_fails_contains "index assign borrowed eval" "semantic error: cannot assign to borrowed value: data" "$compiler" --eval "$index_assign_borrowed_file"

mutable_borrow_after_shared_file="$tmp/mutable_borrow_after_shared.ff"
cat > "$mutable_borrow_after_shared_file" <<'FF'
var data = [1, 2];
let shared = &data;
let exclusive = &var data;
FF

assert_fails_contains "mutable borrow after shared eval" "semantic error: cannot mutably borrow already borrowed value: data" "$compiler" --eval "$mutable_borrow_after_shared_file"

shared_borrow_after_mutable_file="$tmp/shared_borrow_after_mutable.ff"
cat > "$shared_borrow_after_mutable_file" <<'FF'
var data = [1, 2];
let exclusive = &var data;
let shared = &data;
FF

assert_fails_contains "shared borrow after mutable eval" "semantic error: cannot immutably borrow mutably borrowed value: data" "$compiler" --eval "$shared_borrow_after_mutable_file"

read_mutable_borrowed_file="$tmp/read_mutable_borrowed.ff"
cat > "$read_mutable_borrowed_file" <<'FF'
var data = [1, 2];
let exclusive = &var data;
data[0];
FF

assert_fails_contains "read mutable borrowed eval" "semantic error: cannot read mutably borrowed value: data" "$compiler" --eval "$read_mutable_borrowed_file"

use_after_move_file="$tmp/use_after_move.ff"
cat > "$use_after_move_file" <<'FF'
let data = [1, 2];
let moved = data;

data[0];
FF

assert_fails_contains "use after move eval" "semantic error: use of moved value: data" "$compiler" --eval "$use_after_move_file"

call_move_file="$tmp/call_move.ff"
cat > "$call_move_file" <<'FF'
let consume = func(xs: array): int {
  1
};

let data = [1, 2];
consume(data);
data[0];
FF

assert_fails_contains "call move eval" "semantic error: use of moved value: data" "$compiler" --eval "$call_move_file"

section "unsupported LLVM diagnostics"
llvm_array_unsupported_file="$tmp/llvm_array_unsupported.ff"
cat > "$llvm_array_unsupported_file" <<'FF'
let values = [1, 2];
values[0];
FF

llvm_array_eval="$("$compiler" --eval "$llvm_array_unsupported_file")"
assert_equal "llvm array boundary eval" "$llvm_array_eval" "1"
assert_fails_contains "llvm array unsupported" "llvm error: unsupported feature for --emit-llvm: arrays" "$compiler" --emit-llvm "$llvm_array_unsupported_file"

llvm_hash_unsupported_file="$tmp/llvm_hash_unsupported.ff"
cat > "$llvm_hash_unsupported_file" <<'FF'
let user = {"age": 3};
user["age"];
FF

llvm_hash_eval="$("$compiler" --eval "$llvm_hash_unsupported_file")"
assert_equal "llvm hash boundary eval" "$llvm_hash_eval" "3"
assert_fails_contains "llvm hash unsupported" "llvm error: unsupported feature for --emit-llvm: hashes" "$compiler" --emit-llvm "$llvm_hash_unsupported_file"

llvm_borrow_unsupported_file="$tmp/llvm_borrow_unsupported.ff"
cat > "$llvm_borrow_unsupported_file" <<'FF'
let name = "firefly";
let borrowed = &name;
borrowed;
FF

llvm_borrow_eval="$("$compiler" --eval "$llvm_borrow_unsupported_file")"
assert_equal "llvm borrow boundary eval" "$llvm_borrow_eval" "firefly"
assert_fails_contains "llvm borrow unsupported" "llvm error: unsupported feature for --emit-llvm: borrow expressions" "$compiler" --emit-llvm "$llvm_borrow_unsupported_file"

llvm_reference_annotation_unsupported_file="$tmp/llvm_reference_annotation_unsupported.ff"
cat > "$llvm_reference_annotation_unsupported_file" <<'FF'
let take = func(value: &string): int {
  1
};

0;
FF

assert_fails_contains "llvm reference annotation unsupported" "llvm error: unsupported feature for --emit-llvm: reference type annotations" "$compiler" --emit-llvm "$llvm_reference_annotation_unsupported_file"

llvm_array_annotation_unsupported_file="$tmp/llvm_array_annotation_unsupported.ff"
cat > "$llvm_array_annotation_unsupported_file" <<'FF'
let count = func(values: array<int>): int {
  1
};

0;
FF

assert_fails_contains "llvm array annotation unsupported" "llvm error: unsupported feature for --emit-llvm: array and hash type annotations" "$compiler" --emit-llvm "$llvm_array_annotation_unsupported_file"

llvm_index_assignment_unsupported_file="$tmp/llvm_index_assignment_unsupported.ff"
cat > "$llvm_index_assignment_unsupported_file" <<'FF'
var values = [1, 2];
values[0] = 3;
FF

assert_fails_contains "llvm index assignment unsupported" "llvm error: unsupported feature for --emit-llvm: index assignment" "$compiler" --emit-llvm "$llvm_index_assignment_unsupported_file"

llvm_builtin_unsupported_file="$tmp/llvm_builtin_unsupported.ff"
cat > "$llvm_builtin_unsupported_file" <<'FF'
len("abc");
FF

llvm_builtin_eval="$("$compiler" --eval "$llvm_builtin_unsupported_file")"
assert_equal "llvm builtin boundary eval" "$llvm_builtin_eval" "3"
assert_fails_contains "llvm builtin unsupported" "llvm error: unsupported feature for --emit-llvm: builtins" "$compiler" --emit-llvm "$llvm_builtin_unsupported_file"

llvm_nested_function_unsupported_file="$tmp/llvm_nested_function_unsupported.ff"
cat > "$llvm_nested_function_unsupported_file" <<'FF'
let outer = func(): int {
  let inner = func(): int {
    1
  };
  inner()
};

outer();
FF

assert_fails_contains "llvm nested function unsupported" "llvm error: nested function literals are not supported by --emit-llvm yet: inner" "$compiler" --emit-llvm "$llvm_nested_function_unsupported_file"

echo "LLVM smoke tests passed"
