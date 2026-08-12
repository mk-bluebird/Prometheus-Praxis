param(
    [string]$RepoRoot = ".",
    [switch]$Configure,
    [switch]$Build,
    [switch]$Test,
    [switch]$Lint,
    [switch]$ValidateLua,
    [switch]$ValidateSql
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path $RepoRoot).Path
if (-not (Test-Path (Join-Path $root ".git")) -and
    -not (Test-Path (Join-Path $root "CMakeLists.txt"))) {
    throw "RepoRoot must contain .git or CMakeLists.txt."
}

$targets = @(
    "cpp/eco_restoration",
    "cpp/simulation",
    "cpp/tools",
    "lua/eco_restoration",
    "sql/eco_restoration"
) | ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path $_ }

Push-Location $root
try {
    Write-Host "Repository: $root"

    foreach ($directory in $targets) {
        $files = Get-ChildItem -Path $directory -Recurse -File -Include *.cpp,*.hpp,*.h,*.lua,*.sql
        Write-Host ("{0}: {1} relevant files" -f $directory, $files.Count)
    }

    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    $buildDirectory = Join-Path $root "build"

    if ($Configure) {
        if (-not $cmake) { throw "CMake is not available; do not install it from this script." }
        & cmake -S $root -B $buildDirectory
        if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }
    }

    if ($Build) {
        if (-not (Test-Path (Join-Path $buildDirectory "CMakeCache.txt"))) {
            throw "Build directory is not configured. Re-run with -Configure."
        }
        & cmake --build $buildDirectory --config Release --parallel
        if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }
    }

    if ($Test) {
        $ctestFile = Join-Path $buildDirectory "CTestTestfile.cmake"
        if (Test-Path $ctestFile) {
            & ctest --test-dir $buildDirectory --output-on-failure
            if ($LASTEXITCODE -ne 0) { throw "CTest failed." }
        } else {
            Write-Host "CTest metadata is absent; skipping tests."
        }
    }

    if ($Lint) {
        $formatter = Get-Command clang-format -ErrorAction SilentlyContinue
        if ($formatter) {
            Get-ChildItem -Path $targets -Recurse -File -Include *.cpp,*.hpp,*.h |
                ForEach-Object {
                    & clang-format --dry-run --Werror $_.FullName
                    if ($LASTEXITCODE -ne 0) { throw "Formatting check failed: $($_.FullName)" }
                }
        } else {
            Write-Host "clang-format is unavailable; skipping formatting validation."
        }
    }

    if ($ValidateLua) {
        $luac = Get-Command luac -ErrorAction SilentlyContinue
        if ($luac) {
            Get-ChildItem -Path $targets -Recurse -File -Filter *.lua |
                ForEach-Object {
                    & luac -p $_.FullName
                    if ($LASTEXITCODE -ne 0) { throw "Lua validation failed: $($_.FullName)" }
                }
        } else {
            Write-Host "luac is unavailable; skipping Lua validation."
        }
    }

    if ($ValidateSql) {
        $sqlite = Get-Command sqlite3 -ErrorAction SilentlyContinue
        if ($sqlite) {
            Get-ChildItem -Path $targets -Recurse -File -Filter *.sql |
                ForEach-Object {
                    Get-Content $_.FullName | & sqlite3 ":memory:"
                    if ($LASTEXITCODE -ne 0) { throw "SQL validation failed: $($_.FullName)" }
                }
        } else {
            Write-Host "sqlite3 is unavailable; skipping SQL validation."
        }
    }
}
finally {
    Pop-Location
}
