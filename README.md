<h1>
  <img src="assets/firefly-logo.png" width="52" alt="Firefly logo" align="center" />
  Firefly
</h1>

![Firefly hero](assets/firefly-hero.png)

## Contents

- [Introduction](#introduction)
- [Compile](#compile)
- [Usage](#usage)

## Introduction

Firefly is a small language implementation written in C++. 

```firefly
var fib = func(n) {
  if (n < 2) {
    n
  } else {
    fib(n - 1) + fib(n - 2)
  }
};

fib(10);
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

The maintainer's local fallback path is `D:/vcpkg/installed/x64-windows-release`.
CMake uses it only when neither `LLVM_DIR` nor `CMAKE_TOOLCHAIN_FILE` is set.

On Windows Debug builds, the executable is usually here:

```powershell
.\build\Debug\Firefly_compiler.exe
```

On Linux/macOS or single-configuration generators:

```bash
./build/Firefly_compiler
```

## Usage

Start the REPL:

```bash
Firefly_compiler
```

The REPL keeps variables and functions alive for the current session:

```firefly
>> var x = 10;
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

Evaluate a source file:

```bash
Firefly_compiler main.ff
```

Equivalent form:

```bash
Firefly_compiler --eval main.ff
```

Print the AST:

```bash
Firefly_compiler --ast main.ff
```

Emit LLVM IR:

```bash
Firefly_compiler --emit-llvm main.ff
```

This writes `main.ll` next to the input file.

Show CLI help:

```bash
Firefly_compiler --help
```

Example `.ff` file:

```firefly
var i = 0;
var sum = 0;

while (i < 5) {
  sum = sum + i;
  i = i + 1;
};

var data = [1, 2, 3];
data[0] = data[0] + 10;

var user = {"name": "firefly", "age": 1};
user["age"] = user["age"] + 1;

sum;
```

The first LLVM backend stage supports integers, booleans, strings, `var`, `if`,
functions, calls, and `return`. Arrays, hashes, loops, assignment, closures, and
native executable linking are still interpreter-only for now.
