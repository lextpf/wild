#pragma once

#include "IRenderer.h"
#include "RendererAPI.h"

struct GLFWwindow;

/**
 * @brief Checks if a renderer API was compiled into this build.
 * @param api The renderer API to check.
 * @return true if available, false otherwise.
 */
bool IsRendererAvailable(RendererAPI api);

/**
 * @brief Creates a renderer instance for the requested API.
 *
 * Falls back to OpenGL if the requested API is unavailable.
 * Caller owns the returned pointer and must call Init() before use.
 *
 * @param api The renderer API to use.
 * @param window GLFW window handle for initialization.
 * @return Pointer to renderer instance, or nullptr on failure.
 */
IRenderer *CreateRenderer(RendererAPI api, GLFWwindow *window);
