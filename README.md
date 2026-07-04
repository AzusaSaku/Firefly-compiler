<h1>
  <img src="assets/firefly-logo.png" width="52" alt="Firefly logo" align="center" />
  Firefly
</h1>

![Firefly hero](assets/firefly-hero.png)

## Contents

- [Introduction](#introduction)
- [Compile](#compile)
- [Usage](#usage)
- [Examples](#examples)
- [Release Check](#release-check)

## Introduction

Firefly is a small language implementation written in C++. 

```firefly
let fib = func(n: int): int {
  if (n < 2) {
    n
  } else {
    fib(n - 1) + fib(n - 2)
  }
};

fib(10);
```

Use `let` for immutable bindings and `var` for mutable bindings:

```firefly
let answer = 42;
var total = answer;
total = total + 1;
```

Assigning a new value to a `let` binding is an error. `var` is the spelling for
values that are meant to change.

Bindings and functions can carry explicit type annotations:

```firefly
let answer: int = 42;

let add_one = func(n: int): int {
  n + 1
};
```

Supported annotation names are `int`, `bool`, `string`, `array<T>`, `hash`,
and `void`.

Arrays are statically checked as homogeneous values. Firefly infers element
types from array literals and uses them for indexing:

```firefly
let values: array<int> = [1, 2, 3];
let first: int = values[0];

let mixed = [1, true]; // semantic error: array element types differ
```

Borrow syntax is available in the semantic checker. Firefly uses `&T` for an
immutable reference and `&var T` for a mutable reference, matching the
`let`/`var` spelling instead of using a separate `mut` keyword:

```firefly
let values: array<int> = [1, 2, 3];
let borrowed: &array<int> = &values;

var editable: array<int> = [1, 2, 3];
let editable_ref: &var array<int> = &var editable;
```

Borrowed function parameters can be written with the same type syntax:

```firefly
let first = func(xs: &array<int>): int {
  xs[0]
};

first(&values);
```

References are typed and do not move the original value. Shared references can
coexist with other shared references. A mutable reference is exclusive: while it
is alive, the original value cannot be read, moved, assigned, or borrowed again.
While any reference is alive, the original value cannot be moved or assigned.
References created inside a block are released when that block ends.

File execution also runs a small static semantic pass before evaluating or
emitting LLVM IR. It catches undefined names, assigning to `let`, assigning a
different type into an existing variable, non-boolean `if`/`while` conditions,
basic operator type mismatches, annotated binding mismatches, function argument
type mismatches, function return type mismatches, array element mismatches, and
borrow conflicts.

Firefly also checks ownership moves. `int`, `bool`, and function values are
Copy. `string`, `array`, and `hash` values are treated as owned values: binding
them to another name or passing them to a function moves the value, and using the
old name afterwards is a semantic error.

```firefly
let data = [1, 2];
let moved = data;

data[0]; // semantic error: use of moved value: data
```

## Compile

Firefly uses LLVM through CMake's `find_package(LLVM CONFIG REQUIRED)`.

If LLVM is available through your normal CMake package search path:

```bash
cmake -S . -B build
cmake --build build
```

With vcpkg manifest mode, configure with the vcpkg toolchain file:

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE=D:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build
```

With a vcpkg classic-mode LLVM install, pass `LLVM_DIR` explicitly:

```powershell
cmake -S . -B build `
  -DLLVM_DIR=D:\vcpkg\installed\x64-windows-release\share\llvm
cmake --build build
```

Install the built executable:

```bash
cmake --install build --config Release --prefix dist
```

The maintainer's local fallback path is `D:/vcpkg/installed/x64-windows-release`.
CMake uses it only when neither `LLVM_DIR` nor `CMAKE_TOOLCHAIN_FILE` is set.

Run the current LLVM smoke tests after building:

```powershell
.\tests\smoke_llvm.ps1 -Exe .\build\Debug\firefly.exe
```

On Linux:

```bash
bash ./tests/smoke_llvm.sh ./build/firefly
```

On Windows Debug builds, the executable is usually here:

```powershell
.\build\Debug\firefly.exe
```

On Linux/macOS or single-configuration generators:

```bash
./build/firefly
```

## Usage

### REPL interpreter

Start the interactive interpreter:

```bash
firefly
```

The REPL evaluates Firefly code immediately and keeps variables and functions
alive for the current session:

```firefly
>> let x = 10;
10
>> x + 5;
15
```

REPL commands:

```text
:ast   print AST instead of evaluating
:eval  switch back to evaluator mode
:exit  exit REPL
:quit  exit REPL
```

### File interpreter

Evaluate a source file with the interpreter:

```bash
firefly main.ff
```

Equivalent form:

```bash
firefly --eval main.ff
```

The interpreter supports the dynamic Firefly features, including `let`, `var`,
arrays, hashes, assignment, while, functions, calls, strings, and builtins.

When evaluating a file, Firefly first runs the semantic checker. The REPL uses
the interactive interpreter directly.

Print the AST:

```bash
firefly --ast main.ff
```

### LLVM compiler

Emit LLVM IR:

```bash
firefly --emit-llvm main.ff
```

This writes `main.ll` next to the input file.

Emit a native object file:

```bash
firefly --emit-obj main.ff
```

This writes `main.obj` on Windows and `main.o` on Linux.

Build a native executable:

```bash
firefly --build main.ff
```

This writes `main.exe` on Windows and `main` on Linux. `--build` uses LLVM's
`llc` plus `lld-link` on Windows, and `llc` plus `clang`/`cc` on Linux.
`--compile` is an alias for `--build`.

The LLVM compiler targets the statically compiled subset: integers, booleans,
strings, `let`, `var`, assignment, `if`, `while`, functions, calls, `return`,
and primitive type annotations. `let` bindings are immutable in both the
interpreter and LLVM path, and a shared semantic pass rejects basic type and
use-after-move errors before IR generation. Arrays, hashes, references,
closures, builtins, and runtime printing run through the interpreter. If one
of those interpreter-only features reaches `--emit-llvm`, Firefly rejects it
before IR generation with an `llvm error: unsupported feature for --emit-llvm`
diagnostic.

Show CLI help:

```bash
firefly --help
```

Show the version:

```bash
firefly --version
```

Example `.ff` file:

```firefly
let limit: int = 5;
var i = 0;
var sum = 0;

while (i < limit) {
  sum = sum + i;
  i = i + 1;
};

var data = [1, 2, 3];
data[0] = data[0] + 10;

var user = {"name": "firefly", "age": 1};
user["age"] = user["age"] + 1;

sum;
```

## Examples

Run the interpreter feature example:

```bash
firefly --eval examples/interpreter_features.ff
```

Emit LLVM IR from the compiled-subset example:

```bash
firefly --emit-llvm examples/llvm_basic.ff
```

The interpreter example uses arrays, hashes, assignment, and borrow checking.
The LLVM example uses only the compiled subset.

## Release Check

Release smoke commands for Windows and Linux are documented in
[`docs/release.md`](docs/release.md).

