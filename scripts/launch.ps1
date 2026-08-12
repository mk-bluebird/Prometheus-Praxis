#!/usr/bin/env pwsh
# launch.ps1 - Local automation entry point for Prometheus-Praxis eco-restoration wiring
param(
    [Parameter(Mandatory=$true)]
    [string]$RepoRoot,
    
    [switch]$Configure,
    [switch]$Build,
    [switch]$Test,
    [switch]$Lint,
    [switch]$ValidateLua,
    [switch]$ValidateSql
)

$ErrorActionPreference = "Stop"

# Resolve RepoRoot to absolute path
$RepoRoot = (Resolve-Path $RepoRoot).Path

# Verify this is a repository directory
if (-not (Test-Path "$RepoRoot/.git")) {
    Write-Error "RepoRoot '$RepoRoot' is not a repository directory (missing .git)"
    exit 1
}

Write-Host "Repository root: $RepoRoot"

# Define known paths to target
$KnownPaths = @(
    "cpp/eco_restoration",
    "cpp/simulation",
    "cpp/tools",
    "lua/eco_restoration",
    "sql/eco_restoration"
)

# Inventory relevant source files
$CppFiles = @()
$LuaFiles = @()
$SqlFiles = @()

foreach ($path in $KnownPaths) {
    $fullPath = Join-Path $RepoRoot $path
    if (Test-Path $fullPath) {
        Write-Host "Scanning: $path"
        
        # C++ source files
        $CppFiles += Get-ChildItem -Path $fullPath -Filter "*.cpp" -Recurse -File | Select-Object -ExpandProperty FullName
        
        # Lua files
        $LuaFiles += Get-ChildItem -Path $fullPath -Filter "*.lua" -Recurse -File | Select-Object -ExpandProperty FullName
        
        # SQL files
        $SqlFiles += Get-ChildItem -Path $fullPath -Filter "*.sql" -Recurse -File | Select-Object -ExpandProperty FullName
    }
}

Write-Host "Found $($CppFiles.Count) C++ files, $($LuaFiles.Count) Lua files, $($SqlFiles.Count) SQL files"

# Check for tool availability
$CmakeAvailable = $null -ne (Get-Command "cmake" -ErrorAction SilentlyContinue)
$ClangFormatAvailable = $null -ne (Get-Command "clang-format" -ErrorAction SilentlyContinue)
$LuacAvailable = $null -ne (Get-Command "luac" -ErrorAction SilentlyContinue)
$Sqlite3Available = $null -ne (Get-Command "sqlite3" -ErrorAction SilentlyContinue)

Write-Host "Tools available: cmake=$CmakeAvailable, clang-format=$ClangFormatAvailable, luac=$LuacAvailable, sqlite3=$Sqlite3Available"

# Lint: Run clang-format --dry-run --Werror only when available
if ($Lint -and $ClangFormatAvailable) {
    Write-Host "Running clang-format dry-run on C++ files..."
    $formatErrors = @()
    foreach ($file in $CppFiles) {
        $result = & clang-format --dry-run --Werror $file 2>&1
        if ($LASTEXITCODE -ne 0) {
            $formatErrors += $file
        }
    }
    if ($formatErrors.Count -gt 0) {
        Write-Warning "clang-format found issues in $($formatErrors.Count) files"
    } else {
        Write-Host "clang-format passed"
    }
} elseif ($Lint) {
    Write-Host "Skipping clang-format: tool not available"
}

# Validate Lua syntax with luac -p only when available
if ($ValidateLua -and $LuacAvailable) {
    Write-Host "Validating Lua syntax..."
    $luaErrors = @()
    foreach ($file in $LuaFiles) {
        $output = & luac -p $file 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Lua syntax error in $file : $output"
            $luaErrors += $file
        }
    }
    if ($luaErrors.Count -eq 0) {
        Write-Host "Lua validation passed"
    }
} elseif ($ValidateLua) {
    Write-Host "Skipping Lua validation: luac not available"
}

# Validate SQL syntax against in-memory SQLite database
if ($ValidateSql -and $Sqlite3Available) {
    Write-Host "Validating SQL syntax..."
    $sqlErrors = @()
    foreach ($file in $SqlFiles) {
        try {
            $output = & sqlite3 ":memory:" (Get-Content -Path $file -Raw) 2>&1
            if ($LASTEXITCODE -ne 0 -and $output -match "Error") {
                Write-Warning "SQL error in $file : $output"
                $sqlErrors += $file
            }
        } catch {
            Write-Warning "SQL error in $file : $_"
            $sqlErrors += $file
        }
    }
    if ($sqlErrors.Count -eq 0) {
        Write-Host "SQL validation passed"
    }
} elseif ($ValidateSql) {
    Write-Host "Skipping SQL validation: sqlite3 not available"
}

# Configure and build through CMake when CMakeLists.txt and cmake are available
if (($Configure -or $Build) -and $CmakeAvailable) {
    $RootCMake = Join-Path $RepoRoot "CMakeLists.txt"
    if (Test-Path $RootCMake) {
        $BuildDir = Join-Path $RepoRoot "build"
        New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
        
        if ($Configure) {
            Write-Host "Configuring CMake..."
            Push-Location $BuildDir
            try {
                cmake .. -DCMAKE_BUILD_TYPE=Release
                if ($LASTEXITCODE -ne 0) {
                    throw "CMake configuration failed"
                }
                Write-Host "CMake configuration complete"
            } finally {
                Pop-Location
            }
        }
        
        if ($Build) {
            Write-Host "Building CMake targets..."
            Push-Location $BuildDir
            try {
                cmake --build . --config Release
                if ($LASTEXITCODE -ne 0) {
                    throw "CMake build failed"
                }
                Write-Host "CMake build complete"
            } finally {
                Pop-Location
            }
        }
    } else {
        Write-Host "No root CMakeLists.txt found, skipping CMake"
    }
} elseif ($Configure -or $Build) {
    Write-Host "Skipping CMake: cmake not available or no CMakeLists.txt"
}

# Run CTest only when configured build directory contains CTestTestfile.cmake
if ($Test -and $CmakeAvailable) {
    $CTestFile = Join-Path $RepoRoot "build/CTestTestfile.cmake"
    if (Test-Path $CTestFile) {
        Write-Host "Running CTest..."
        Push-Location (Join-Path $RepoRoot "build")
        try {
            ctest --output-on-failure -C Release
            if ($LASTEXITCODE -ne 0) {
                Write-Warning "Some tests failed"
            } else {
                Write-Host "All tests passed"
            }
        } finally {
            Pop-Location
        }
    } else {
        Write-Host "CTestTestfile.cmake not found, skipping tests"
    }
}

Write-Host "Launch script completed successfully"
exit 0
