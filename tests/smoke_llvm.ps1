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
        "build\Release\Firefly_compiler.exe",
        "build\Debug\Firefly_compiler.exe",
        "cmake-build-llvm\Release\Firefly_compiler.exe",
        "cmake-build-debug\Debug\Firefly_compiler.exe",
        "cmake-build-debug\Firefly_compiler.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "compiler executable not found; pass -Exe path\to\Firefly_compiler.exe"
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

function Write-Utf8NoBom {
    param(
        [string]$Path,
        [string]$Value
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Value, $encoding)
}

$compiler = Find-FireflyCompiler $Exe
$tmp = Join-Path $PSScriptRoot ".tmp"
if (Test-Path $tmp) {
    Remove-Item -LiteralPath $tmp -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

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

Remove-Item -LiteralPath $tmp -Recurse -Force
Write-Output "LLVM smoke tests passed"
