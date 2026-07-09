param(
    [string]$Exe = "",
    [string]$OutputDir = "artifacts",
    [switch]$Full,
    [string]$LlvmBin = "",
    [string]$LicensesDir = "licenses"
)

$ErrorActionPreference = "Stop"

function Find-FireflyExe {
    param([string]$Requested)

    if ($Requested -ne "") {
        if (!(Test-Path $Requested)) {
            throw "firefly executable not found: $Requested"
        }
        return (Resolve-Path $Requested).Path
    }

    $candidates = @(
        "build\Release\firefly.exe",
        "cmake-build-llvm\Release\firefly.exe",
        "cmake-build-debug\Debug\firefly.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "firefly executable not found; pass -Exe path\to\firefly.exe"
}

function Copy-RequiredFile {
    param([string]$Source, [string]$Destination)

    if (!(Test-Path $Source)) {
        throw "required file not found: $Source"
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Copy-RequiredDirectory {
    param([string]$Source, [string]$Destination)

    if (!(Test-Path $Source)) {
        throw "required directory not found: $Source"
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
}

function Copy-PublicDocs {
    param([string]$Destination)

    if (!(Test-Path "docs")) {
        throw "required directory not found: docs"
    }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Get-ChildItem -LiteralPath "docs" -File | Where-Object {
        $_.Name -ne "firefly-0.1.0-development-brief.md"
    } | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $Destination -Force
    }
}

function Copy-LicenseNotices {
    param(
        [string]$Destination,
        [string]$SourceDir,
        [string]$LlvmToolsDir
    )

    if (Test-Path $SourceDir) {
        Copy-RequiredDirectory $SourceDir $Destination
        return
    }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-RequiredFile "LICENSE" (Join-Path $Destination "Firefly-MIT.txt")

    $vcpkgRoot = $null
    $toolsPath = Resolve-Path $LlvmToolsDir
    $cursor = $toolsPath.Path
    while ($cursor -and !(Test-Path (Join-Path $cursor "share\llvm\copyright"))) {
        $parent = Split-Path $cursor -Parent
        if ($parent -eq $cursor) {
            break
        }
        $cursor = $parent
    }
    if ($cursor -and (Test-Path (Join-Path $cursor "share\llvm\copyright"))) {
        $vcpkgRoot = $cursor
    }

    if ($vcpkgRoot -eq $null) {
        throw "licenses directory not found and vcpkg notices could not be located; pass -LicensesDir for a full package"
    }

    Copy-RequiredFile (Join-Path $vcpkgRoot "share\llvm\copyright") (Join-Path $Destination "LLVM-Apache-2.0-LLVM-exception.txt")
    Copy-RequiredFile (Join-Path $vcpkgRoot "share\zlib\copyright") (Join-Path $Destination "zlib.txt")
    Copy-RequiredFile (Join-Path $vcpkgRoot "share\zstd\copyright") (Join-Path $Destination "zstd.txt")

    @'
Microsoft Visual C++ Runtime Note
=================================

The Windows lightweight package does not bundle Microsoft Visual C++ runtime
DLLs. Users may need to install the Microsoft Visual C++ Redistributable
2015-2022 x64 before running firefly.exe.

The Windows full package bundles LLVM tools and their zlib/zstd runtime
dependencies, but it still does not bundle MSVC runtime DLLs unless a future
release explicitly says otherwise.
'@ | Set-Content -LiteralPath (Join-Path $Destination "MSVC-runtime-note.txt") -Encoding UTF8
}

function Resolve-LlvmBin {
    param([string]$Requested)

    if ($Requested -ne "") {
        if (!(Test-Path $Requested)) {
            throw "LLVM tools directory not found: $Requested"
        }
        return (Resolve-Path $Requested).Path
    }

    if ($env:FIREFLY_LLVM_TOOLS_DIR -and (Test-Path $env:FIREFLY_LLVM_TOOLS_DIR)) {
        return (Resolve-Path $env:FIREFLY_LLVM_TOOLS_DIR).Path
    }

    $candidates = @(
        "D:\vcpkg\installed\x64-windows-release\tools\llvm",
        "D:\vcpkg\installed\x64-windows\tools\llvm"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "LLVM tools directory not found; pass -LlvmBin for a full package"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $repoRoot

$fireflyExe = Find-FireflyExe $Exe
$versionOutput = & $fireflyExe --version
if ($LASTEXITCODE -ne 0 -or $versionOutput -notmatch "^firefly\s+(.+)$") {
    throw "could not read Firefly version from $fireflyExe"
}
$version = $Matches[1]
$kind = "lightweight"
if ($Full) {
    $kind = "full"
}
$packageName = "firefly-$version-windows-x64-$kind"

$outputPath = Join-Path $repoRoot $OutputDir
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null

$packageRoot = Join-Path $outputPath $packageName
$resolvedOutput = (Resolve-Path $outputPath).Path
if (Test-Path $packageRoot) {
    $resolvedPackage = (Resolve-Path $packageRoot).Path
    if (!$resolvedPackage.StartsWith($resolvedOutput, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "refusing to remove package path outside output directory: $resolvedPackage"
    }
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path (Join-Path $packageRoot "bin") | Out-Null
Copy-RequiredFile $fireflyExe (Join-Path $packageRoot "bin\firefly.exe")
Copy-RequiredFile "README.md" $packageRoot
Copy-RequiredFile "LICENSE" $packageRoot
Copy-RequiredDirectory "examples" (Join-Path $packageRoot "examples")
Copy-PublicDocs (Join-Path $packageRoot "docs")

if ($Full) {
    $llvmTools = Resolve-LlvmBin $LlvmBin
    Copy-RequiredFile (Join-Path $llvmTools "llc.exe") (Join-Path $packageRoot "bin\llc.exe")
    Copy-RequiredFile (Join-Path $llvmTools "lld-link.exe") (Join-Path $packageRoot "bin\lld-link.exe")
    Copy-RequiredFile (Join-Path $llvmTools "z.dll") (Join-Path $packageRoot "bin\z.dll")
    Copy-RequiredFile (Join-Path $llvmTools "zstd.dll") (Join-Path $packageRoot "bin\zstd.dll")
    Copy-LicenseNotices (Join-Path $packageRoot "licenses") $LicensesDir $llvmTools
}

$archivePath = Join-Path $outputPath "$packageName.zip"
if (Test-Path $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
Compress-Archive -Path (Join-Path $packageRoot "*") -DestinationPath $archivePath -CompressionLevel Optimal
Remove-Item -LiteralPath $packageRoot -Recurse -Force

Write-Output "wrote $archivePath"
