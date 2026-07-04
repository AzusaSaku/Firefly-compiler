param(
    [string]$Exe = ""
)

$ErrorActionPreference = "Stop"

function Find-FireflyCompiler {
    param([string]$Requested)

    if ($Requested -ne "") {
        if (!(Test-Path $Requested)) {
            throw "compiler executable not found: $Requested"
        }
        return (Resolve-Path $Requested).Path
    }

    $candidates = @(
        "build\Release\firefly.exe",
        "build\Debug\firefly.exe",
        "cmake-build-llvm\Release\firefly.exe",
        "cmake-build-debug\Debug\firefly.exe",
        "cmake-build-debug\firefly.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "compiler executable not found; pass -Exe path\to\firefly.exe"
}

function Assert-Equal {
    param(
        [string]$Name,
        [string]$Actual,
        [string]$Expected
    )

    if ($Actual.Trim() -ne $Expected) {
        throw "$Name failed: expected '$Expected', got '$($Actual.Trim())'"
    }
}

function Assert-Contains {
    param(
        [string]$Name,
        [string]$Text,
        [string]$Pattern
    )

    if ($Text -notmatch $Pattern) {
        throw "$Name failed: missing pattern '$Pattern'"
    }
}

function Assert-Fails-Contains {
    param(
        [string]$Name,
        [string[]]$Command,
        [string]$Pattern
    )

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = & $Command[0] $Command[1..($Command.Length - 1)] 2>&1 | Out-String
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -eq 0) {
        throw "$Name failed: command succeeded unexpectedly"
    }
    Assert-Contains $Name $output $Pattern
}

function Write-Utf8NoBom {
    param(
        [string]$Path,
        [string]$Value
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Value, $encoding)
}

$compiler = Find-FireflyCompiler $Exe
$repoRoot = Split-Path $PSScriptRoot -Parent
$versionOutput = & $compiler --version
if ($LASTEXITCODE -ne 0) {
    throw "version command failed"
}
Assert-Contains "version command" $versionOutput "^firefly "

$tmp = Join-Path $PSScriptRoot ".tmp"
if (Test-Path $tmp) {
    Remove-Item -LiteralPath $tmp -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

$interpreterExampleFile = Join-Path $repoRoot "examples\interpreter_features.ff"
$evalInterpreterExample = & $compiler --eval $interpreterExampleFile
if ($LASTEXITCODE -ne 0) {
    throw "interpreter example eval failed"
}
Assert-Equal "interpreter example eval" $evalInterpreterExample "3"

$llvmExampleFile = Join-Path $repoRoot "examples\llvm_basic.ff"
$evalLlvmExample = & $compiler --eval $llvmExampleFile
if ($LASTEXITCODE -ne 0) {
    throw "LLVM example eval failed"
}
Assert-Equal "LLVM example eval" $evalLlvmExample "8"

$llvmExampleTmpFile = Join-Path $tmp "example_llvm_basic.ff"
Copy-Item -LiteralPath $llvmExampleFile -Destination $llvmExampleTmpFile
& $compiler --emit-llvm $llvmExampleTmpFile | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "LLVM example emit-llvm failed"
}

$loopSource = @'
var i = 0;
var sum = 0;

while (i < 5) {
  sum = sum + i;
  i = i + 1;
};

sum;
'@
$loopFile = Join-Path $tmp "loop_assign.ff"
Write-Utf8NoBom $loopFile $loopSource

$evalLoop = & $compiler --eval $loopFile
if ($LASTEXITCODE -ne 0) {
    throw "loop eval failed"
}
Assert-Equal "loop eval" $evalLoop "10"

& $compiler --emit-llvm $loopFile | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "loop emit-llvm failed"
}
$loopIr = Get-Content (Join-Path $tmp "loop_assign.ll") -Raw
Assert-Contains "loop IR" $loopIr "while\.cond"
Assert-Contains "loop IR" $loopIr "while\.body"
Assert-Contains "loop IR" $loopIr "while\.end"
Assert-Contains "loop IR" $loopIr "store i64"

$letSource = @'
let answer = 42;
var total = answer;
total = total + 1;

total;
'@
$letFile = Join-Path $tmp "let_var.ff"
Write-Utf8NoBom $letFile $letSource

$evalLet = & $compiler --eval $letFile
if ($LASTEXITCODE -ne 0) {
    throw "let/var eval failed"
}
Assert-Equal "let/var eval" $evalLet "43"

& $compiler --emit-llvm $letFile | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "let/var emit-llvm failed"
}
$letIr = Get-Content (Join-Path $tmp "let_var.ll") -Raw
Assert-Contains "let/var IR" $letIr "answer"
Assert-Contains "let/var IR" $letIr "total"

$typedFunctionSource = @'
let inc = func(n: int): int {
  n + 1
};

inc(41);
'@
$typedFunctionFile = Join-Path $tmp "typed_function.ff"
Write-Utf8NoBom $typedFunctionFile $typedFunctionSource

$evalTypedFunction = & $compiler --eval $typedFunctionFile
if ($LASTEXITCODE -ne 0) {
    throw "typed function eval failed"
}
Assert-Equal "typed function eval" $evalTypedFunction "42"

& $compiler --emit-llvm $typedFunctionFile | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "typed function emit-llvm failed"
}
$typedFunctionIr = Get-Content (Join-Path $tmp "typed_function.ll") -Raw
Assert-Contains "typed function IR" $typedFunctionIr "define i64 @inc\(i64 %n\)"
Assert-Contains "typed function IR" $typedFunctionIr "call i64 @inc"

$copyValueSource = @'
let x = 10;
let y = x;

x + y;
'@
$copyValueFile = Join-Path $tmp "copy_value.ff"
Write-Utf8NoBom $copyValueFile $copyValueSource

$evalCopyValue = & $compiler --eval $copyValueFile
if ($LASTEXITCODE -ne 0) {
    throw "copy value eval failed"
}
Assert-Equal "copy value eval" $evalCopyValue "20"

$moveReassignSource = @'
var data = [1, 2];
let moved = data;
data = [3, 4];

data[0];
'@
$moveReassignFile = Join-Path $tmp "move_reassign.ff"
Write-Utf8NoBom $moveReassignFile $moveReassignSource

$evalMoveReassign = & $compiler --eval $moveReassignFile
if ($LASTEXITCODE -ne 0) {
    throw "move reassign eval failed"
}
Assert-Equal "move reassign eval" $evalMoveReassign "3"

$typedArraySource = @'
let first = func(xs: array<int>): int {
  xs[0]
};

first([41, 1]);
'@
$typedArrayFile = Join-Path $tmp "typed_array.ff"
Write-Utf8NoBom $typedArrayFile $typedArraySource

$evalTypedArray = & $compiler --eval $typedArrayFile
if ($LASTEXITCODE -ne 0) {
    throw "typed array eval failed"
}
Assert-Equal "typed array eval" $evalTypedArray "41"

$borrowSource = @'
let data = [1, 2];
let borrowed: &array<int> = &data;

data[0];
'@
$borrowFile = Join-Path $tmp "borrow.ff"
Write-Utf8NoBom $borrowFile $borrowSource

$evalBorrow = & $compiler --eval $borrowFile
if ($LASTEXITCODE -ne 0) {
    throw "borrow eval failed"
}
Assert-Equal "borrow eval" $evalBorrow "1"

$borrowParamSource = @'
let first = func(xs: &array<int>): int {
  xs[0]
};

let values = [41, 1];
first(&values) + values[1];
'@
$borrowParamFile = Join-Path $tmp "borrow_param.ff"
Write-Utf8NoBom $borrowParamFile $borrowParamSource

$evalBorrowParam = & $compiler --eval $borrowParamFile
if ($LASTEXITCODE -ne 0) {
    throw "borrow param eval failed"
}
Assert-Equal "borrow param eval" $evalBorrowParam "42"

$mutableBorrowSource = @'
var data = [1, 2];
let borrowed: &var array<int> = &var data;

1;
'@
$mutableBorrowFile = Join-Path $tmp "mutable_borrow.ff"
Write-Utf8NoBom $mutableBorrowFile $mutableBorrowSource

$evalMutableBorrow = & $compiler --eval $mutableBorrowFile
if ($LASTEXITCODE -ne 0) {
    throw "mutable borrow eval failed"
}
Assert-Equal "mutable borrow eval" $evalMutableBorrow "1"

$borrowScopeSource = @'
var data = [1, 2];
if (true) {
  let borrowed = &data;
  0
};
let moved = data;

1;
'@
$borrowScopeFile = Join-Path $tmp "borrow_scope.ff"
Write-Utf8NoBom $borrowScopeFile $borrowScopeSource

$evalBorrowScope = & $compiler --eval $borrowScopeFile
if ($LASTEXITCODE -ne 0) {
    throw "borrow scope eval failed"
}
Assert-Equal "borrow scope eval" $evalBorrowScope "1"

& $compiler --emit-obj $loopFile | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "loop emit-obj failed"
}
$loopObject = Join-Path $tmp "loop_assign.obj"
if (!(Test-Path $loopObject)) {
    throw "loop emit-obj failed: object file was not created"
}

& $compiler --build $loopFile | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "loop build failed"
}
$loopExe = Join-Path $tmp "loop_assign.exe"
if (!(Test-Path $loopExe)) {
    throw "loop build failed: executable was not created"
}
& $loopExe
if ($LASTEXITCODE -ne 0) {
    throw "loop executable failed with exit code $LASTEXITCODE"
}

$fibSource = @'
var fib = func(n) {
  if (n < 2) {
    n
  } else {
    fib(n - 1) + fib(n - 2)
  }
};

fib(6);
'@
$fibFile = Join-Path $tmp "fib.ff"
Write-Utf8NoBom $fibFile $fibSource

$evalFib = & $compiler --eval $fibFile
if ($LASTEXITCODE -ne 0) {
    throw "fib eval failed"
}
Assert-Equal "fib eval" $evalFib "8"

& $compiler --emit-llvm $fibFile | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "fib emit-llvm failed"
}
$fibIr = Get-Content (Join-Path $tmp "fib.ll") -Raw
Assert-Contains "fib IR" $fibIr "define i64 @fib"
Assert-Contains "fib IR" $fibIr "call i64 @fib"
Assert-Contains "fib IR" $fibIr "ret i64 %iftmp"

$boolSource = @'
var flag = false;
flag = !flag;

if (flag) {
  1
} else {
  0
};
'@
$boolFile = Join-Path $tmp "bool_assign.ff"
Write-Utf8NoBom $boolFile $boolSource

$evalBool = & $compiler --eval $boolFile
if ($LASTEXITCODE -ne 0) {
    throw "bool eval failed"
}
Assert-Equal "bool eval" $evalBool "1"

& $compiler --emit-llvm $boolFile | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "bool emit-llvm failed"
}
$boolIr = Get-Content (Join-Path $tmp "bool_assign.ll") -Raw
Assert-Contains "bool IR" $boolIr "store i1"
Assert-Contains "bool IR" $boolIr "nottmp"

$immutableSource = @'
let x = 1;
x = 2;
'@
$immutableFile = Join-Path $tmp "immutable_assign.ff"
Write-Utf8NoBom $immutableFile $immutableSource

Assert-Fails-Contains "immutable eval" @($compiler, "--eval", $immutableFile) "cannot assign to immutable binding: x"
Assert-Fails-Contains "immutable emit-llvm" @($compiler, "--emit-llvm", $immutableFile) "cannot assign to immutable binding: x"

$typeMismatchSource = @'
var x = 1;
x = true;
'@
$typeMismatchFile = Join-Path $tmp "type_mismatch.ff"
Write-Utf8NoBom $typeMismatchFile $typeMismatchSource

Assert-Fails-Contains "type mismatch eval" @($compiler, "--eval", $typeMismatchFile) "semantic error: type mismatch in assignment to 'x': int vs bool"
Assert-Fails-Contains "type mismatch emit-llvm" @($compiler, "--emit-llvm", $typeMismatchFile) "semantic error: type mismatch in assignment to 'x': int vs bool"

$bindingTypeMismatchSource = @'
let x: bool = 1;
'@
$bindingTypeMismatchFile = Join-Path $tmp "binding_type_mismatch.ff"
Write-Utf8NoBom $bindingTypeMismatchFile $bindingTypeMismatchSource

Assert-Fails-Contains "binding type mismatch eval" @($compiler, "--eval", $bindingTypeMismatchFile) "semantic error: type mismatch in binding 'x': bool vs int"

$conditionMismatchSource = @'
if (1) {
  1
};
'@
$conditionMismatchFile = Join-Path $tmp "condition_mismatch.ff"
Write-Utf8NoBom $conditionMismatchFile $conditionMismatchSource

Assert-Fails-Contains "condition mismatch eval" @($compiler, "--eval", $conditionMismatchFile) "semantic error: if condition must be bool, got int"

$argumentTypeMismatchSource = @'
let inc = func(n: int): int {
  n + 1
};

inc(true);
'@
$argumentTypeMismatchFile = Join-Path $tmp "argument_type_mismatch.ff"
Write-Utf8NoBom $argumentTypeMismatchFile $argumentTypeMismatchSource

Assert-Fails-Contains "argument type mismatch eval" @($compiler, "--eval", $argumentTypeMismatchFile) "semantic error: type mismatch in argument 1: int vs bool"

$returnTypeMismatchSource = @'
let bad = func(): int {
  true
};

bad();
'@
$returnTypeMismatchFile = Join-Path $tmp "return_type_mismatch.ff"
Write-Utf8NoBom $returnTypeMismatchFile $returnTypeMismatchSource

Assert-Fails-Contains "return type mismatch eval" @($compiler, "--eval", $returnTypeMismatchFile) "semantic error: type mismatch in function return: int vs bool"

$mixedArraySource = @'
let values = [1, true];
'@
$mixedArrayFile = Join-Path $tmp "mixed_array.ff"
Write-Utf8NoBom $mixedArrayFile $mixedArraySource

Assert-Fails-Contains "mixed array eval" @($compiler, "--eval", $mixedArrayFile) "semantic error: type mismatch in array element: int vs bool"

$arrayBindingMismatchSource = @'
let values: array<int> = [true];
'@
$arrayBindingMismatchFile = Join-Path $tmp "array_binding_mismatch.ff"
Write-Utf8NoBom $arrayBindingMismatchFile $arrayBindingMismatchSource

Assert-Fails-Contains "array binding mismatch eval" @($compiler, "--eval", $arrayBindingMismatchFile) "semantic error: type mismatch in binding 'values' element: int vs bool"

$arrayIndexAssignmentMismatchSource = @'
var values: array<int> = [1, 2];
values[0] = true;
'@
$arrayIndexAssignmentMismatchFile = Join-Path $tmp "array_index_assignment_mismatch.ff"
Write-Utf8NoBom $arrayIndexAssignmentMismatchFile $arrayIndexAssignmentMismatchSource

Assert-Fails-Contains "array index assignment mismatch eval" @($compiler, "--eval", $arrayIndexAssignmentMismatchFile) "semantic error: type mismatch in index assignment: int vs bool"

$borrowTypeMismatchSource = @'
let data = [1, 2];
let borrowed: &array<bool> = &data;
'@
$borrowTypeMismatchFile = Join-Path $tmp "borrow_type_mismatch.ff"
Write-Utf8NoBom $borrowTypeMismatchFile $borrowTypeMismatchSource

Assert-Fails-Contains "borrow type mismatch eval" @($compiler, "--eval", $borrowTypeMismatchFile) "semantic error: type mismatch in binding 'borrowed' reference element: bool vs int"

$mutableBorrowImmutableSource = @'
let data = [1, 2];
let borrowed = &var data;
'@
$mutableBorrowImmutableFile = Join-Path $tmp "mutable_borrow_immutable.ff"
Write-Utf8NoBom $mutableBorrowImmutableFile $mutableBorrowImmutableSource

Assert-Fails-Contains "mutable borrow immutable eval" @($compiler, "--eval", $mutableBorrowImmutableFile) "semantic error: cannot mutably borrow immutable binding: data"

$moveBorrowedSource = @'
let data = [1, 2];
let borrowed = &data;
let moved = data;
'@
$moveBorrowedFile = Join-Path $tmp "move_borrowed.ff"
Write-Utf8NoBom $moveBorrowedFile $moveBorrowedSource

Assert-Fails-Contains "move borrowed eval" @($compiler, "--eval", $moveBorrowedFile) "semantic error: cannot move borrowed value: data"

$assignBorrowedSource = @'
var data = [1, 2];
let borrowed = &data;
data = [3, 4];
'@
$assignBorrowedFile = Join-Path $tmp "assign_borrowed.ff"
Write-Utf8NoBom $assignBorrowedFile $assignBorrowedSource

Assert-Fails-Contains "assign borrowed eval" @($compiler, "--eval", $assignBorrowedFile) "semantic error: cannot assign to borrowed value: data"

$indexAssignBorrowedSource = @'
var data = [1, 2];
let borrowed = &data;
data[0] = 3;
'@
$indexAssignBorrowedFile = Join-Path $tmp "index_assign_borrowed.ff"
Write-Utf8NoBom $indexAssignBorrowedFile $indexAssignBorrowedSource

Assert-Fails-Contains "index assign borrowed eval" @($compiler, "--eval", $indexAssignBorrowedFile) "semantic error: cannot assign to borrowed value: data"

$mutableBorrowAfterSharedSource = @'
var data = [1, 2];
let shared = &data;
let exclusive = &var data;
'@
$mutableBorrowAfterSharedFile = Join-Path $tmp "mutable_borrow_after_shared.ff"
Write-Utf8NoBom $mutableBorrowAfterSharedFile $mutableBorrowAfterSharedSource

Assert-Fails-Contains "mutable borrow after shared eval" @($compiler, "--eval", $mutableBorrowAfterSharedFile) "semantic error: cannot mutably borrow already borrowed value: data"

$sharedBorrowAfterMutableSource = @'
var data = [1, 2];
let exclusive = &var data;
let shared = &data;
'@
$sharedBorrowAfterMutableFile = Join-Path $tmp "shared_borrow_after_mutable.ff"
Write-Utf8NoBom $sharedBorrowAfterMutableFile $sharedBorrowAfterMutableSource

Assert-Fails-Contains "shared borrow after mutable eval" @($compiler, "--eval", $sharedBorrowAfterMutableFile) "semantic error: cannot immutably borrow mutably borrowed value: data"

$readMutableBorrowedSource = @'
var data = [1, 2];
let exclusive = &var data;
data[0];
'@
$readMutableBorrowedFile = Join-Path $tmp "read_mutable_borrowed.ff"
Write-Utf8NoBom $readMutableBorrowedFile $readMutableBorrowedSource

Assert-Fails-Contains "read mutable borrowed eval" @($compiler, "--eval", $readMutableBorrowedFile) "semantic error: cannot read mutably borrowed value: data"

$useAfterMoveSource = @'
let data = [1, 2];
let moved = data;

data[0];
'@
$useAfterMoveFile = Join-Path $tmp "use_after_move.ff"
Write-Utf8NoBom $useAfterMoveFile $useAfterMoveSource

Assert-Fails-Contains "use after move eval" @($compiler, "--eval", $useAfterMoveFile) "semantic error: use of moved value: data"

$callMoveSource = @'
let consume = func(xs: array): int {
  1
};

let data = [1, 2];
consume(data);
data[0];
'@
$callMoveFile = Join-Path $tmp "call_move.ff"
Write-Utf8NoBom $callMoveFile $callMoveSource

Assert-Fails-Contains "call move eval" @($compiler, "--eval", $callMoveFile) "semantic error: use of moved value: data"

$llvmArrayUnsupportedSource = @'
let values = [1, 2];
values[0];
'@
$llvmArrayUnsupportedFile = Join-Path $tmp "llvm_array_unsupported.ff"
Write-Utf8NoBom $llvmArrayUnsupportedFile $llvmArrayUnsupportedSource

$evalLlvmArrayUnsupported = & $compiler --eval $llvmArrayUnsupportedFile
if ($LASTEXITCODE -ne 0) {
    throw "llvm array boundary eval failed"
}
Assert-Equal "llvm array boundary eval" $evalLlvmArrayUnsupported "1"
Assert-Fails-Contains "llvm array unsupported" @($compiler, "--emit-llvm", $llvmArrayUnsupportedFile) "llvm error: unsupported feature for --emit-llvm: arrays"

$llvmHashUnsupportedSource = @'
let user = {"age": 3};
user["age"];
'@
$llvmHashUnsupportedFile = Join-Path $tmp "llvm_hash_unsupported.ff"
Write-Utf8NoBom $llvmHashUnsupportedFile $llvmHashUnsupportedSource

$evalLlvmHashUnsupported = & $compiler --eval $llvmHashUnsupportedFile
if ($LASTEXITCODE -ne 0) {
    throw "llvm hash boundary eval failed"
}
Assert-Equal "llvm hash boundary eval" $evalLlvmHashUnsupported "3"
Assert-Fails-Contains "llvm hash unsupported" @($compiler, "--emit-llvm", $llvmHashUnsupportedFile) "llvm error: unsupported feature for --emit-llvm: hashes"

$llvmBorrowUnsupportedSource = @'
let name = "firefly";
let borrowed = &name;
borrowed;
'@
$llvmBorrowUnsupportedFile = Join-Path $tmp "llvm_borrow_unsupported.ff"
Write-Utf8NoBom $llvmBorrowUnsupportedFile $llvmBorrowUnsupportedSource

$evalLlvmBorrowUnsupported = & $compiler --eval $llvmBorrowUnsupportedFile
if ($LASTEXITCODE -ne 0) {
    throw "llvm borrow boundary eval failed"
}
Assert-Equal "llvm borrow boundary eval" $evalLlvmBorrowUnsupported "firefly"
Assert-Fails-Contains "llvm borrow unsupported" @($compiler, "--emit-llvm", $llvmBorrowUnsupportedFile) "llvm error: unsupported feature for --emit-llvm: borrow expressions"

$llvmBuiltinUnsupportedSource = @'
len("abc");
'@
$llvmBuiltinUnsupportedFile = Join-Path $tmp "llvm_builtin_unsupported.ff"
Write-Utf8NoBom $llvmBuiltinUnsupportedFile $llvmBuiltinUnsupportedSource

$evalLlvmBuiltinUnsupported = & $compiler --eval $llvmBuiltinUnsupportedFile
if ($LASTEXITCODE -ne 0) {
    throw "llvm builtin boundary eval failed"
}
Assert-Equal "llvm builtin boundary eval" $evalLlvmBuiltinUnsupported "3"
Assert-Fails-Contains "llvm builtin unsupported" @($compiler, "--emit-llvm", $llvmBuiltinUnsupportedFile) "llvm error: unsupported feature for --emit-llvm: builtins"

Remove-Item -LiteralPath $tmp -Recurse -Force
Write-Output "LLVM smoke tests passed"
