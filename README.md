<h1>
  <img src="assets/firefly-logo.png" width="52" alt="Firefly logo" align="center" />
  Firefly
</h1>

![Firefly hero](assets/firefly-hero.png)

Firefly 0.1.0 is a small C++ language implementation with a REPL interpreter,
a static semantic checker, and an LLVM backend for a documented compiled
subset.

## Contents

- [Quick Start](#quick-start)
- [Install And Build From Source](#install-and-build-from-source)
- [Project Structure](#project-structure)
- [Interpreter Mode](#interpreter-mode)
- [Compiler Mode](#compiler-mode)
- [Feature Matrix](#feature-matrix)
- [Release Packages](#release-packages)
- [Testing](#testing)
- [Limitations](#limitations)

## Quick Start

Start the REPL:

```bash
firefly
```

Evaluate a file with the interpreter:

```bash
firefly --eval examples/interpreter_features.ff
```

Emit LLVM IR for the compiled subset:

```bash
firefly --emit-llvm examples/llvm_basic.ff
```

Build a native executable:

```bash
firefly --build examples/llvm_basic.ff
```

Check the installed version:

```bash
firefly --version
```

## Install And Build From Source

Firefly builds one executable named `firefly`. It requires CMake, a C++20
compiler, and LLVM development files discoverable by CMake.

Configure and build with LLVM available on the normal CMake search path:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

With vcpkg manifest mode:

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE=D:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

With a vcpkg classic-mode LLVM install:

```powershell
cmake -S . -B build `
  -DLLVM_DIR=D:\vcpkg\installed\x64-windows-release\share\llvm
cmake --build build --config Release
```

Install the built executable:

```bash
cmake --install build --config Release --prefix dist
```

The maintainer's local fallback path is `D:/vcpkg/installed/x64-windows-release`.
CMake uses it only when neither `LLVM_DIR` nor `CMAKE_TOOLCHAIN_FILE` is set.

## Project Structure

Firefly is organized around four conceptual layers:

- Frontend: `src/token.hpp`, `src/lexer.hpp`, `src/parser.hpp`, and
  `src/ast.hpp` turn source text into an AST.
- Semantic checker: `src/semantic.hpp` performs static checks shared by file
  evaluation and LLVM compilation.
- Interpreter: `src/object.hpp`, `src/environment.hpp`, `src/evaluator.hpp`,
  and `src/repl.hpp` implement runtime objects, lexical environments,
  tree-walking evaluation, and the interactive REPL.
- Compiler and CLI: `src/llvm_codegen.hpp` emits LLVM IR for the compiled
  subset, `src/driver.hpp` connects frontend/semantic/compiler/runtime paths,
  and `src/main.cpp` owns command-line argument parsing.

## Interpreter Mode

The interpreter is the broadest Firefly execution path. It supports `let`,
`var`, integers, booleans, strings, arrays, hashes, assignment, index
assignment, `if`, `while`, functions, calls, `return`, borrow syntax, ownership
checks, and builtins such as `len`, `first`, `last`, `rest`, `push`, and
`puts`.

```firefly
var numbers: array<int> = [1, 2, 3];
numbers[0] = numbers[0] + 10;

var user = {"name": "firefly", "age": 1};
user["age"] = user["age"] + numbers[1];

user["age"];
```

Run a file:

```bash
firefly main.ff
firefly --eval main.ff
```

Print the parsed AST:

```bash
firefly --ast main.ff
```

REPL commands:

```text
:ast   print AST instead of evaluating
:eval  switch back to evaluator mode
:exit  exit REPL
:quit  exit REPL
```

## Compiler Mode

The LLVM backend supports a smaller compiled subset: integers, booleans,
strings, `let`, `var`, identifier assignment, `if`, `while`, named functions,
calls, `return`, primitive type annotations, and `puts("text")`.

Emit LLVM IR:

```bash
firefly --emit-llvm main.ff
```

Emit a native object file:

```bash
firefly --emit-obj main.ff
```

Build a native executable:

```bash
firefly --build main.ff
firefly --compile main.ff
```

`--compile` is an alias for `--build`. Native output uses LLVM `llc`. Windows
builds use `lld-link`; Linux builds use `clang` or `cc`.

Compiled `puts("text")` lowers to the host C runtime `puts` function. The
runtime boundary is documented in [`docs/runtime.md`](docs/runtime.md), and the
LLVM builtin matrix is documented in
[`docs/llvm-builtins.md`](docs/llvm-builtins.md).

## Feature Matrix

| Feature | Interpreter | LLVM compiler |
| --- | --- | --- |
| Integers, booleans, strings | Yes | Yes |
| `let`, `var`, assignment | Yes | Yes |
| `if`, `while` | Yes | Yes |
| Functions and calls | Yes | Named compiled functions |
| `return` | Yes | Yes |
| Primitive type annotations | Yes | Yes |
| Arrays and hashes | Yes | No |
| Index expressions and assignment | Yes | No |
| Borrow expressions and reference annotations | Semantic/file mode | No |
| Builtins | `len`, `first`, `last`, `rest`, `push`, `puts` | `puts("text")` only |
| Native executable build | Not applicable | Host platform only |

Unsupported LLVM features fail with deterministic diagnostics before or during
LLVM lowering. The full support matrix is in
[`docs/llvm-support.md`](docs/llvm-support.md).

## Release Packages

Release packaging is script-driven:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package_windows.ps1 `
  -Exe .\build\Release\firefly.exe
```

```bash
bash ./scripts/package_linux.sh --exe ./build/firefly
```

Windows packages:

- Lightweight: `bin/firefly.exe`, docs, examples, README, and LICENSE.
- Full: lightweight contents plus `llc.exe`, `lld-link.exe`, `z.dll`,
  `zstd.dll`, and `licenses/` notices.

Linux packages must be built on Linux. Do not reuse Windows binaries for Linux.

Windows packages require the Microsoft Visual C++ Redistributable 2015-2022 x64
unless a package explicitly bundles the runtime DLLs.

Release commands and package checks are documented in
[`docs/release.md`](docs/release.md).

## Testing

Run the smoke tests after building:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\smoke_llvm.ps1 -Exe .\build\Release\firefly.exe
```

```bash
bash ./tests/smoke_llvm.sh ./build/firefly
```

The smoke suite is grouped by release/package smoke, interpreter behavior, LLVM
IR generation, native build output, interpreter/native consistency, parser and
semantic errors, and unsupported LLVM diagnostics. See
[`tests/README.md`](tests/README.md).

GitHub Actions builds and smoke-tests Windows and Linux Release configurations
on pushes and pull requests.

## Limitations

Firefly 0.1.0 is an early formal release, not a mature production compiler.

- Native builds target the host platform; cross-compilation is not supported.
- The LLVM backend supports a compiled subset, not the full interpreter
  language.
- Arrays, hashes, references, closures, and most builtins are interpreter-only.
- There is no bytecode VM, garbage collector, trait system, generic system, or
  full standard library.
- Native build modes depend on external LLVM/system tools unless those tools are
  bundled in a full package.
