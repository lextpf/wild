# Contributing to Wild Engine

Thank you for your interest in contributing to Wild! This document provides guidelines and coding standards to help you get started.

## Getting Started

1. **Fork the repository** on GitHub.
2. **Clone your fork** locally.
3. **Set up the development environment** (see [docs/SETUP.md](docs/SETUP.md)).
4. **Build the project** (see [docs/BUILDING.md](docs/BUILDING.md)).
5. Use MSVC with C++23 enabled. Builds must pass for both OpenGL and Vulkan paths where applicable.

## How to Contribute

### Reporting Bugs

Before submitting a bug report:
- Check if the issue already exists in [GitHub Issues](https://github.com/lextpf/wild/issues)
- Try to reproduce with the latest version

When reporting, include:
- Steps to reproduce the issue
- Expected behavior vs actual behavior
- Your OS, GPU, and driver version
- Screenshots or error logs if applicable

### Suggesting Features

Feature requests are welcome! Please:
- Check existing issues to avoid duplicates
- Describe the use case and why it would be valuable
- Be open to discussion about implementation approaches

### Submitting Code

1. **Create a feature branch**: `git checkout -b feature/your-feature-name`
2. **Make your changes** following the coding standards below
3. **Test thoroughly** - both OpenGL and Vulkan backends, editor mode, overlays, time-of-day toggles
4. **Commit with clear messages**: `git commit -m "Add feature: description"`
5. **Push to your fork**: `git push origin feature/your-feature-name`
6. **Open a Pull Request** with a clear description of your changes

## Coding Standards

### Language

- **C++23** - prefer standard library facilities (ranges, `std::optional`, `std::string_view`) where appropriate.
- Compile with warnings enabled and treat warnings as errors when possible.
- Keep code ASCII unless there is a clear, intentional reason (e.g., emoji in docs).

### Naming Conventions

| Element           | Style                        | Example                          |
|-------------------|------------------------------|----------------------------------|
| Classes/Structs   | PascalCase                   | `PlayerCharacter`, `TimeManager` |
| Enums             | PascalCase with `enum class` | `enum class TimePeriod`          |
| Enum values       | PascalCase                   | `TimePeriod::Dawn`               |
| Methods/Functions | PascalCase                   | `GetPosition()`, `Initialize()`  |
| Member variables  | m_PascalCase                 | `m_Position`, `m_CurrentTime`    |
| Local variables   | camelCase                    | `deltaTime`, `tileIndex`         |
| Parameters        | camelCase                    | `float deltaTime`, `int tileX`   |

### Brace Style

We use **Allman style** - opening braces on their own line:

```cpp
void Game::Update(float deltaTime)
{
    if (m_EditorMode)
    {
        UpdateEditor(deltaTime);
    }
    else
    {
        UpdateGameplay(deltaTime);
    }
}
```

### Include Order

Organize includes in this order, with blank lines between groups:

```cpp
#include "Game.h"              // 1. Corresponding header (for .cpp)

#include "PlayerCharacter.h"   // 2. Project headers
#include "Tilemap.h"

#include <GLFW/glfw3.h>        // 3. External libraries
#include <glm/glm.hpp>

#include <iostream>            // 4. Standard library
#include <vector>
```

### Header Files

- Use `#pragma once` for header guards
- Put Doxygen documentation in headers, not in .cpp files
- Keep headers minimal - forward declare when possible
- Avoid including heavy headers in other headers (prefer forward declarations)

```cpp
#pragma once

#include <glm/glm.hpp>

class Tilemap;  // Forward declaration instead of #include

/**
 * @class PlayerCharacter
 * @brief Represents the player-controlled character.
 */
class PlayerCharacter
{
    // ...
};
```

### Constructor Initialization Lists

Initialize members one per line with comments explaining non-obvious values:

```cpp
Game::Game()
    : m_Window(nullptr)           // GLFW window handle
    , m_ScreenWidth(1360)         // 17 tiles * 16px * 5 scale
    , m_ScreenHeight(960)         // 12 tiles * 16px * 5 scale
    , m_CameraPosition(0.0f)      // Start at origin
{
}
```

### Documentation

**In header files**, use Doxygen comments for public APIs:

```cpp
/**
 * @brief Gets the current time of day.
 * @return Current time in hours (0.0-24.0)
 */
float GetCurrentTime() const;
```

Use `///` for brief enum/member documentation:

```cpp
enum class TimePeriod
{
    Dawn,       ///< 05:00-07:00 - Sunrise transition
    Morning,    ///< 07:00-10:00 - Early day
};
```

**In .cpp files**, use regular `//` comments for implementation details:

```cpp
// Calculate smoothing alpha for ~0.15 second settle time
float alpha = 1.0f - std::exp(-deltaTime / 0.033f);
```

### Modern C++ Guidelines

- Prefer `enum class` over plain `enum`
- Use `nullptr` instead of `NULL`
- Use `auto` when the type is obvious or unwieldy
- Prefer range-based for loops
- Use smart pointers for ownership (`std::unique_ptr`, `std::shared_ptr`)
- Mark things `const` and `constexpr` when possible
- Prefer `std::string_view` for non-owning string parameters
- Use `std::optional` for nullable return values and avoid magic sentinels

### Things to Avoid

- Don't use raw `new`/`delete` - use smart pointers or containers
- Don't cast the renderer to a specific backend type - use the `IRenderer` interface
- Don't add platform-specific code without `#ifdef` guards
- Don't commit debug code (sleep calls, excessive logging, etc.)
- Don't introduce frame-time spin-waits; if you must wait, use platform timers/event waits

## Architecture Guidelines

### Renderer Abstraction

All rendering goes through `IRenderer`. Never access OpenGL/Vulkan directly from game code:

```cpp
// Good - uses abstraction
m_Renderer->DrawSprite(texture, position, size);

// Bad - breaks abstraction
glBindTexture(GL_TEXTURE_2D, textureId);
```

### Coordinate System

- **Origin**: Top-left corner
- **Y-axis**: Increases downward
- **Units**: Pixels (world space)
- **Entity positions**: Bottom-center of sprite

### Adding New Features

1. **Discuss first** - Open an issue to discuss the approach
2. **Keep it focused** - One feature per PR
3. **Update documentation** - Add Doxygen comments, update relevant .md files
4. **Test both renderers** - Press F1 to switch between OpenGL and Vulkan

## Testing Your Changes

Before submitting a PR:

1. **Build successfully** with `.\build.bat` (Windows) or CMake
2. **Run the game** and verify your changes work
3. **Test both renderers** - Press F1 to switch
4. **Test editor mode** - Press E if your changes affect it
5. **Check debug overlays** - Press F3 to verify rendering
6. **Test different times of day** - Press F5 to cycle time periods
7. **Check no-projection visuals** if your change touches anchors, projection, or stacking

## Code Review Process

1. A maintainer will review your PR
2. Address any feedback or questions
3. Once approved, your PR will be merged

## Questions?

- Open a [GitHub Issue](https://github.com/lextpf/wild/issues) for bugs or features
- Start a [GitHub Discussion](https://github.com/lextpf/wild/discussions) for questions

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE).

Thank you for helping improve Wild!
