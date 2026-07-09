# Firefly Smoke Test Organization

The smoke scripts keep Windows and Linux coverage aligned. They are grouped by
behavior so a failure points at the subsystem most likely involved:

- Release/package smoke: verifies the selected `firefly` executable responds to
  stable release commands such as `--version`.
- Interpreter behavior: evaluates examples and dynamic-language programs
  through `--eval`.
- LLVM IR generation: emits `.ll` for compiled-subset programs and checks key
  IR shapes.
- Native object/executable build: exercises `--emit-obj`, `--build`, and the
  generated executable.
- Interpreter/native consistency: runs compiled-subset programs through both
  `--eval` and generated native executables, then compares visible output
  markers.
- Parser and semantic errors: verifies frontend and semantic failures remain
  deterministic and include useful line/column locations.
- Unsupported LLVM diagnostics: confirms interpreter-only features still fail
  clearly in compiler mode.

Run the Windows script with:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\smoke_llvm.ps1 -Exe .\build\Release\firefly.exe
```

Run the Linux script with:

```bash
bash ./tests/smoke_llvm.sh ./build/firefly
```
