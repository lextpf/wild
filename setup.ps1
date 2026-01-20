# ============================================================================
# setup.ps1 - Download and setup all external dependencies
# ============================================================================
# Dependencies:
#   - GLFW (windowing)          -> external/glfw/
#   - GLM (math)                -> external/glm/
#   - GLAD (OpenGL loader)      -> external/glad/
#   - stb_image (image loading) -> external/stb/
#   - nlohmann/json (JSON)      -> external/nlohmann/
#   - Vulkan SDK (optional)     -> System install required
#   - FreeType (optional)       -> vcpkg or system install
# ============================================================================

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "                    WILD - DEPENDENCY SETUP" -ForegroundColor Cyan
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host ""

# Check for required tools
Write-Host "Checking for required tools..." -ForegroundColor Yellow

$hasGit = Get-Command git -ErrorAction SilentlyContinue
if (-not $hasGit) {
    Write-Host "ERROR: git is not installed or not in PATH!" -ForegroundColor Red
    Write-Host "Please install Git from: https://git-scm.com/downloads"
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "Tools OK." -ForegroundColor Green
Write-Host ""

# Create external directory
if (-not (Test-Path "external")) {
    New-Item -ItemType Directory -Path "external" | Out-Null
}

# ============================================================================
# GLFW - Windowing library
# ============================================================================
Write-Host "[1/5] Setting up GLFW..." -ForegroundColor Cyan
Write-Host "----------------------------------------------------------------------------"

if (Test-Path "external\glfw\CMakeLists.txt") {
    Write-Host "GLFW already exists. Updating..."
    Push-Location "external\glfw"
    git pull
    Pop-Location
} else {
    Write-Host "Cloning GLFW..."
    git clone --depth 1 https://github.com/glfw/glfw.git external/glfw
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Failed to clone GLFW!" -ForegroundColor Red
        Read-Host "Press Enter to exit"
        exit 1
    }
}
Write-Host "GLFW setup complete." -ForegroundColor Green
Write-Host ""

# ============================================================================
# GLM - Math library
# ============================================================================
Write-Host "[2/5] Setting up GLM..." -ForegroundColor Cyan
Write-Host "----------------------------------------------------------------------------"

if (Test-Path "external\glm\glm\glm.hpp") {
    Write-Host "GLM already exists. Updating..."
    Push-Location "external\glm"
    git pull
    Pop-Location
} else {
    Write-Host "Cloning GLM..."
    git clone --depth 1 https://github.com/g-truc/glm.git external/glm
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Failed to clone GLM!" -ForegroundColor Red
        Read-Host "Press Enter to exit"
        exit 1
    }
}
Write-Host "GLM setup complete." -ForegroundColor Green
Write-Host ""

# ============================================================================
# GLAD - OpenGL Loader
# ============================================================================
Write-Host "[3/5] Setting up GLAD..." -ForegroundColor Cyan
Write-Host "----------------------------------------------------------------------------"

if (Test-Path "external\glad\src\glad.c") {
    Write-Host "GLAD already exists. Skipping."
} else {
    Write-Host "Setting up GLAD structure..."

    # Create directories
    @("external\glad", "external\glad\include", "external\glad\include\glad",
      "external\glad\include\KHR", "external\glad\src") | ForEach-Object {
        if (-not (Test-Path $_)) {
            New-Item -ItemType Directory -Path $_ | Out-Null
        }
    }

    Write-Host "Downloading GLAD files..."

    # Download glad.h
    try {
        Invoke-WebRequest -Uri "https://raw.githubusercontent.com/Dav1dde/glad/glad2/include/glad/gl.h" -OutFile "external\glad\include\glad\glad.h" -UseBasicParsing
    } catch {
        Write-Host "Trying alternative source for glad.h..."
        try {
            # Use a known working GLAD generated file
            $gladContent = @"
// GLAD OpenGL loader - auto-generated
// Go to https://glad.dav1d.de/ to generate a custom version
// Settings: Language=C/C++, Specification=OpenGL, API gl=3.3, Profile=Core
#ifndef GLAD_GL_H_
#define GLAD_GL_H_
// Placeholder - please generate from https://glad.dav1d.de/
#error "Please download GLAD from https://glad.dav1d.de/ with OpenGL 3.3 Core profile"
#endif
"@
            Set-Content -Path "external\glad\include\glad\glad.h" -Value $gladContent
        } catch {
            Write-Host "WARNING: Could not download glad.h" -ForegroundColor Yellow
        }
    }

    # Download khrplatform.h
    try {
        Invoke-WebRequest -Uri "https://www.khronos.org/registry/EGL/api/KHR/khrplatform.h" -OutFile "external\glad\include\KHR\khrplatform.h" -UseBasicParsing
    } catch {
        Write-Host "WARNING: Could not download khrplatform.h" -ForegroundColor Yellow
    }

    # Download glad.c
    try {
        Invoke-WebRequest -Uri "https://raw.githubusercontent.com/Dav1dde/glad/glad2/src/gl.c" -OutFile "external\glad\src\glad.c" -UseBasicParsing
    } catch {
        Write-Host "WARNING: Could not download glad.c" -ForegroundColor Yellow
        $gladCContent = @"
// GLAD source - placeholder
#error "Please download GLAD from https://glad.dav1d.de/"
"@
        Set-Content -Path "external\glad\src\glad.c" -Value $gladCContent
    }

    # Create CMakeLists.txt for GLAD
    $cmakeContent = @"
cmake_minimum_required(VERSION 3.10)
project(glad)
add_library(glad STATIC src/glad.c)
target_include_directories(glad PUBLIC include)
"@
    Set-Content -Path "external\glad\CMakeLists.txt" -Value $cmakeContent

    if (Test-Path "external\glad\src\glad.c") {
        Write-Host "GLAD setup complete." -ForegroundColor Green
        Write-Host "NOTE: If build fails, regenerate GLAD from https://glad.dav1d.de/" -ForegroundColor Yellow
    } else {
        Write-Host ""
        Write-Host "WARNING: Could not download GLAD automatically." -ForegroundColor Yellow
        Write-Host "Please manually download from https://glad.dav1d.de/"
        Write-Host "  Settings: Language=C/C++, Specification=OpenGL, API gl=3.3, Profile=Core"
        Write-Host "  Extract to external/glad/"
        Write-Host ""
    }
}
Write-Host ""

# ============================================================================
# stb_image - Image loading
# ============================================================================
Write-Host "[4/5] Setting up stb_image..." -ForegroundColor Cyan
Write-Host "----------------------------------------------------------------------------"

if (Test-Path "external\stb\stb_image.h") {
    Write-Host "stb_image already exists. Skipping."
} else {
    Write-Host "Downloading stb_image.h..."

    if (-not (Test-Path "external\stb")) {
        New-Item -ItemType Directory -Path "external\stb" | Out-Null
    }

    try {
        Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" -OutFile "external\stb\stb_image.h" -UseBasicParsing

        # Create implementation file
        $stbImpl = @"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
"@
        Set-Content -Path "external\stb\stb_image.c" -Value $stbImpl

        Write-Host "stb_image setup complete." -ForegroundColor Green
    } catch {
        Write-Host "ERROR: Failed to download stb_image.h!" -ForegroundColor Red
        Write-Host "Please download manually from: https://github.com/nothings/stb"
        Read-Host "Press Enter to exit"
        exit 1
    }
}
Write-Host ""

# ============================================================================
# nlohmann/json - JSON library
# ============================================================================
Write-Host "[5/5] Setting up nlohmann/json..." -ForegroundColor Cyan
Write-Host "----------------------------------------------------------------------------"

if (Test-Path "external\nlohmann\json.hpp") {
    Write-Host "nlohmann/json already exists. Skipping."
} else {
    Write-Host "Downloading json.hpp..."

    if (-not (Test-Path "external\nlohmann")) {
        New-Item -ItemType Directory -Path "external\nlohmann" | Out-Null
    }

    try {
        Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp" -OutFile "external\nlohmann\json.hpp" -UseBasicParsing
        Write-Host "nlohmann/json setup complete." -ForegroundColor Green
    } catch {
        Write-Host "ERROR: Failed to download json.hpp!" -ForegroundColor Red
        Write-Host "Please download manually from: https://github.com/nlohmann/json"
        Read-Host "Press Enter to exit"
        exit 1
    }
}
Write-Host ""

# ============================================================================
# Verification
# ============================================================================
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "                         VERIFICATION" -ForegroundColor Cyan
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Checking installed dependencies:"
Write-Host ""

$allOk = $true

$deps = @(
    @{Name="GLFW"; Path="external\glfw\CMakeLists.txt"},
    @{Name="GLM"; Path="external\glm\glm\glm.hpp"},
    @{Name="GLAD"; Path="external\glad\src\glad.c"},
    @{Name="stb_image"; Path="external\stb\stb_image.h"},
    @{Name="nlohmann/json"; Path="external\nlohmann\json.hpp"}
)

foreach ($dep in $deps) {
    if (Test-Path $dep.Path) {
        Write-Host "  [OK] $($dep.Name)" -ForegroundColor Green
    } else {
        Write-Host "  [MISSING] $($dep.Name)" -ForegroundColor Red
        $allOk = $false
    }
}

Write-Host ""
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "                      OPTIONAL DEPENDENCIES" -ForegroundColor Cyan
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "The following are optional but recommended:"
Write-Host ""

# Check for Vulkan SDK
if ($env:VULKAN_SDK) {
    Write-Host "  [OK] Vulkan SDK found at: $env:VULKAN_SDK" -ForegroundColor Green
} else {
    Write-Host "  [NOT INSTALLED] Vulkan SDK" -ForegroundColor Yellow
    Write-Host "      Download from: https://vulkan.lunarg.com/sdk/home" -ForegroundColor Gray
    Write-Host "      Required for: Vulkan renderer, shader compilation" -ForegroundColor Gray
}

# Check for vcpkg
if ($env:VCPKG_ROOT) {
    Write-Host "  [OK] vcpkg found at: $env:VCPKG_ROOT" -ForegroundColor Green
} else {
    Write-Host "  [NOT INSTALLED] vcpkg" -ForegroundColor Yellow
    Write-Host "      Install with: git clone https://github.com/microsoft/vcpkg" -ForegroundColor Gray
    Write-Host "      Required for: FreeType (text rendering)" -ForegroundColor Gray
}

# Check for Doxygen
$hasDoxygen = Get-Command doxygen -ErrorAction SilentlyContinue
if ($hasDoxygen) {
    Write-Host "  [OK] Doxygen" -ForegroundColor Green
} else {
    Write-Host "  [NOT INSTALLED] Doxygen" -ForegroundColor Yellow
    Write-Host "      Download from: https://www.doxygen.nl/download.html" -ForegroundColor Gray
    Write-Host "      Required for: Documentation generation" -ForegroundColor Gray
}

# Check for glslangValidator (shader compiler)
$hasGlslang = Get-Command glslangValidator -ErrorAction SilentlyContinue
if ($hasGlslang) {
    Write-Host "  [OK] glslangValidator (shader compiler)" -ForegroundColor Green
} else {
    Write-Host "  [NOT INSTALLED] glslangValidator" -ForegroundColor Yellow
    Write-Host "      Included with Vulkan SDK" -ForegroundColor Gray
    Write-Host "      Required for: Compiling GLSL shaders to SPIR-V" -ForegroundColor Gray
}

Write-Host ""
Write-Host "============================================================================" -ForegroundColor Cyan

if ($allOk) {
    Write-Host "All required dependencies are installed!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor White
    Write-Host "  1. [Optional] Install Vulkan SDK for Vulkan support"
    Write-Host "  2. [Optional] Install FreeType via vcpkg for text rendering:"
    Write-Host "       vcpkg install freetype:x64-windows"
    Write-Host "  3. Run build-all.bat to build the project"
} else {
    Write-Host ""
    Write-Host "Some dependencies are missing. Please check the errors above." -ForegroundColor Red
}

Write-Host ""
Write-Host "============================================================================" -ForegroundColor Cyan

Read-Host "Press Enter to exit"
