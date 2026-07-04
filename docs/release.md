# Release Check

This project builds one CLI executable named `firefly`.

## Windows

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release `
  -DLLVM_DIR=D:\vcpkg\installed\x64-windows-release\share\llvm
cmake --build build --config Release
powershell -ExecutionPolicy Bypass -File .\tests\smoke_llvm.ps1 -Exe .\build\Release\firefly.exe
cmake --install build --config Release --prefix .\dist
```

The installed executable is `dist\bin\firefly.exe`.

## Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
bash ./tests/smoke_llvm.sh ./build/firefly
cmake --install build --config Release --prefix ./dist
```

The installed executable is `dist/bin/firefly`.

## Quick Manual Checks

```bash
firefly --version
firefly --eval examples/interpreter_features.ff
firefly --emit-llvm examples/llvm_basic.ff
```

The interpreter example exercises arrays, hashes, assignment, and borrow
checking. The LLVM example stays inside the compiled subset.
