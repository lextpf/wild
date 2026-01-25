#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <cmath>
#include "Texture.h"

/**
 * @class IRenderer
 * @brief Abstract interface for 2D rendering operations.
 * @author Alex (https://github.com/lextpf)
 * @ingroup Rendering
 *
 * IRenderer defines the contract that all rendering backends must implement.
 * This abstraction allows the game to run on both OpenGL and Vulkan without
 * modification to the game logic.
 *
 * @section renderer_pattern Design Pattern
 * Implements the **Strategy Pattern** for runtime graphics API selection.
 *
 * @htmlonly
 * <pre class="mermaid">
 * classDiagram
 * class IRenderer:::abstract {
 *     <<interface>>
 *     +Init()
 *     +Shutdown()
 *     +BeginFrame()
 *     +EndFrame()
 *     +DrawSprite()
 *     +DrawText()
 * }
 * class OpenGLRenderer:::opengl {
 *     +Init()
 *     +DrawSprite()
 * }
 * class VulkanRenderer:::vulkan {
 *     +Init()
 *     +DrawSprite()
 * }
 * IRenderer <|-- OpenGLRenderer
 * IRenderer <|-- VulkanRenderer
 * style IRenderer fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 * style OpenGLRenderer fill:#134e3a,stroke:#10b981,color:#e2e8f0
 * style VulkanRenderer fill:#2e1f5e,stroke:#8b5cf6,color:#e2e8f0
 * </pre>
 * @endhtmlonly
 *
 * @section coord_systems Coordinate Systems
 * The renderer operates in multiple coordinate spaces:
 *
 * @htmlonly
 * <pre class="mermaid">
 * flowchart LR
 * W["World Space"]:::space
 * S["Screen Space"]:::space
 * N["Normalized Device Coordinates"]:::space
 * W -->|Camera Transform| S
 * S -->|Projection Matrix| N
 * </pre>
 * @endhtmlonly
 *
 * |  Space |        Origin        | Range                                       |
 * |--------|----------------------|---------------------------------------------|
 * |  World |   Top-left of map    | @f$ (0,0) @f$ to @f$ (16*mapW, 16*mapH) @f$ |
 * | Screen | Top-left of viewport | @f$ (0,0) @f$ to @f$ (screenW, screenH) @f$ |
 * |    NDC |        Center        | @f$ (-1,-1) @f$ to @f$ (+1,+1) @f$          |
 *
 * @section renderer_pipeline Rendering Pipeline
 * @code
 * renderer->BeginFrame();
 * renderer->Clear(0.2f, 0.3f, 0.4f, 1.0f);
 * renderer->SetProjection(orthoMatrix);
 * renderer->DrawSprite(texture, position);
 * renderer->DrawText("Score: 100", pos);
 * renderer->EndFrame();
 * @endcode
 *
 * @section renderer_math Orthographic Projection
 * The renderer uses orthographic projection to map screen pixels to NDC.
 * Unlike perspective projection, parallel lines stay parallel (no depth foreshortening).
 *
 * @par Matrix Parameters
 * | Symbol |   Meaning   | 2D Value |
 * |--------|-------------|----------|
 * |      l |  Left edge  | 0        |
 * |      r | Right edge  | screenW  |
 * |      t |  Top edge   | 0        |
 * |      b | Bottom edge | screenH  |
 * |      n |  Near plane | -1       |
 * |      f |  Far plane  | +1       |
 *
 * @par The Orthographic Matrix
 * @f[
 * M_{ortho} = \begin{bmatrix}
 *   \frac{2}{r-l} & 0 & 0 & -\frac{r+l}{r-l} \\
 *   0 & \frac{2}{t-b} & 0 & -\frac{t+b}{t-b} \\
 *   0 & 0 & -\frac{2}{f-n} & -\frac{f+n}{f-n} \\
 *   0 & 0 & 0 & 1
 * \end{bmatrix}
 * @f]
 *
 * @par What Each Row Does
 * - **Row 1 (X)**: Scales X from [l, r] to [-1, +1] and centers it
 * - **Row 2 (Y)**: Scales Y from [t, b] to [-1, +1] and centers it
 * - **Row 3 (Z)**: Maps depth [n, f] to [-1, +1] (unused in 2D)
 * - **Row 4**: Homogeneous coordinate (always 1 for orthographic)
 *
 * @par Example Transformation
 * For a 1280x720 screen with top-left origin:
 * - @f$ l=0, \; r=1280, \; t=0, \; b=720 @f$
 * - @f$ (640, 360) \rightarrow (0, 0) @f$ (center)
 * - @f$ (0, 0) \rightarrow (-1, -1) @f$ (top-left)
 * - @f$ (1280, 720) \rightarrow (+1, +1) @f$ (bottom-right)
 *
 * @section renderer_uv Texture Coordinates
 * UV coordinates map pixel positions to the 0-1 range the GPU expects:
 * @f[
 * u = \frac{pixelX}{textureWidth}, \quad v = \frac{pixelY}{textureHeight}
 * @f]
 *
 * @par Example
 * For a 256x256 texture, pixel (128, 64) becomes UV (0.5, 0.25).
 *
 * @see OpenGLRenderer, VulkanRenderer, Texture
 */
class IRenderer
{
public:
    /**
     * @brief Virtual destructor for proper polymorphic cleanup.
     */
    virtual ~IRenderer() = default;

    /**
     * @brief Initialize the renderer.
     * 
     * Creates GPU resources, compiles shaders, and sets up rendering state.
     * Must be called after window creation but before any rendering.
     * 
     * @par OpenGL Initialization
     * - Load GLAD function pointers
     * - Compile sprite shader program
     * - Create VAO/VBO for quad rendering
     * - Enable blending for transparency
     * 
     * @par Vulkan Initialization
     * - Create instance, device, swapchain
     * - Create render pass, pipeline
     * - Allocate command buffers
     * - Create descriptor sets
     */
    virtual void Init() = 0;

    /**
     * @brief Shutdown and release all GPU resources.
     * 
     * Destroys all graphics resources created during Init().
     * Must be called before window destruction.
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Begin a new rendering frame.
     * 
     * Prepares the GPU for drawing. Must be called before any draw calls.
     * 
     * @par OpenGL
     * - Clear color/depth buffers
     * - Bind default framebuffer
     * 
     * @par Vulkan
     * - Acquire swapchain image
     * - Begin command buffer recording
     * - Begin render pass
     */
    virtual void BeginFrame() = 0;

    /**
     * @brief End the current frame and present to screen.
     * 
     * Finalizes rendering and displays the result.
     * 
     * @par OpenGL
     * - Flush rendering commands
     * - Swap buffers handled by GLFW
     * 
     * @par Vulkan
     * - End render pass
     * - Submit command buffer
     * - Present swapchain image
     */
    virtual void EndFrame() = 0;

    /**
     * @brief Draw a full texture as a sprite.
     * 
     * Renders the entire texture at the specified position.
     * Position is the **top-left corner** of the sprite.
     * 
     * @par Transformation Order
     * 1. Scale to size
     * 2. Rotate around center
     * 3. Translate to position
     * 
     * @param texture  Texture to draw.
     * @param position World position (top-left corner).
     * @param size     Sprite dimensions in pixels (default: 32x32).
     * @param rotation Rotation in degrees, clockwise (default: 0).
     * @param color    Color tint/modulation (default: white = no tint).
     */
    virtual void DrawSprite(const Texture &texture, glm::vec2 position, glm::vec2 size = glm::vec2(32.0f, 32.0f),
                            float rotation = 0.0f, glm::vec3 color = glm::vec3(1.0f)) = 0;

    /**
     * @brief Draw a sprite with alpha tinting and optional additive blending.
     *
     * Similar to DrawSprite but supports alpha modulation and additive blending
     * for effects like particles and glows.
     *
     * @param texture  Texture to render.
     * @param position World position (top-left corner).
     * @param size     Output size in pixels.
     * @param rotation Rotation in degrees, clockwise (default: 0).
     * @param color    RGBA color tint/modulation.
     * @param additive Use additive blending for glow effects (default: false).
     */
    virtual void DrawSpriteAlpha(const Texture &texture, glm::vec2 position, glm::vec2 size,
                                 float rotation, glm::vec4 color, bool additive = false) = 0;

    /**
     * @brief Draw a region of a texture (sprite sheet).
     * 
     * Renders a rectangular portion of the texture, useful for sprite
     * sheets and tile atlases. The region is specified in **pixel coordinates**.
     * 
     * @param texture  Texture containing the sprite sheet.
     * @param position World position (top-left corner).
     * @param size     Output size in pixels.
     * @param texCoord Top-left corner of region in **pixels**.
     * @param texSize  Size of region in **pixels**.
     * @param rotation Rotation in degrees (default: 0).
     * @param color    Color tint (default: white).
     * @param flipY    Flip vertical UV coordinates (default: true for OpenGL).
     */
    virtual void DrawSpriteRegion(const Texture &texture, glm::vec2 position, glm::vec2 size,
                                  glm::vec2 texCoord, glm::vec2 texSize, float rotation = 0.0f,
                                  glm::vec3 color = glm::vec3(1.0f), bool flipY = true) = 0;

    /**
     * @brief Draw a sprite from a texture atlas with per-vertex alpha.
     *
     * Renders a region of a texture atlas using normalized UV coordinates.
     * Supports per-vertex color/alpha and additive blending for particles.
     *
     * @param texture  Atlas texture.
     * @param position World position (top-left corner).
     * @param size     Output size in pixels.
     * @param uvMin    Top-left UV coordinates (normalized 0-1).
     * @param uvMax    Bottom-right UV coordinates (normalized 0-1).
     * @param rotation Rotation in degrees, clockwise.
     * @param color    RGBA color tint/modulation.
     * @param additive Use additive blending for glow effects.
     */
    virtual void DrawSpriteAtlas(const Texture &texture, glm::vec2 position, glm::vec2 size,
                                 glm::vec2 uvMin, glm::vec2 uvMax, float rotation,
                                 glm::vec4 color, bool additive = false) = 0;

    /**
     * @brief Draw a solid colored rectangle.
     *
     * Renders a filled rectangle with the specified RGBA color.
     * Useful for UI elements, debug overlays, and backgrounds.
     *
     * @par Alpha Blending Variables
     * |          Symbol | Meaning                               |
     * |-----------------|---------------------------------------|
     * | @f$ C_{out} @f$ | Final pixel color written to screen   |
     * | @f$ C_{src} @f$ | Rectangle color                       |
     * | @f$ C_{dst} @f$ | Existing pixel color                  |
     * |  @f$ \alpha @f$ | Opacity (0 = transparent, 1 = opaque) |
     *
     * @par Standard Blend (additive=false)
     * @f[
     * C_{out} = C_{src} \times \alpha + C_{dst} \times (1 - \alpha)
     * @f]
     * Linearly interpolates between source and destination.
     * - @f$ \alpha = 0.5 @f$: 50% mix of both colors
     * - @f$ \alpha = 1.0 @f$: Fully opaque, destination hidden
     * - @f$ \alpha = 0.0 @f$: Fully transparent, destination unchanged
     *
     * @par Additive Blend (additive=true)
     * @f[
     * C_{out} = C_{src} \times \alpha + C_{dst}
     * @f]
     * Adds source color to destination, making pixels brighter.
     * - @see ParticleSystem uses this for glowing/emissive particles
     *
     * @param position Top-left corner in screen coordinates.
     * @param size     Rectangle dimensions.
     * @param color    RGBA color (0-1 range).
     * @param additive Use additive blending for glow effects (default: false).
     */
    virtual void DrawColoredRect(glm::vec2 position, glm::vec2 size, glm::vec4 color, bool additive = false) = 0;

    /**
     * @brief Set the projection matrix.
     * 
     * Updates the GPU uniform for coordinate transformation.
     * Typically called once per frame with an orthographic matrix.
     * 
     * @param projection 4x4 projection matrix.
     */
    virtual void SetProjection(glm::mat4 projection) = 0;

    enum class ProjectionMode
    {
        VanishingPoint,   // Perspective scaling toward horizon only
        Globe,            // Spherical curvature only
        Fisheye           // Globe curvature & vanishing point combined
    };

    struct PerspectiveState
    {
        bool enabled = false;                                  // Perspective configured
        ProjectionMode mode = ProjectionMode::VanishingPoint;  // Which projection to use
        float horizonY = 0.0f;                                 // Screen-space Y of horizon line
        float horizonScale = 1.0f;                             // Scale at horizon (0..1 typically)
        float viewWidth = 0.0f;                                // Current world-view width
        float viewHeight = 0.0f;                               // Current world-view height
        float sphereRadius = 2000.0f;                          // Radius for globe projection
    };

    // Each renderer must store and return the state it got from SetPerspective()
    virtual PerspectiveState GetPerspectiveState() const = 0;

    /**
     * @brief Project a 2D point using the currently configured perspective.
     *
     * Transforms a screen-space point through the active projection mode(s).
     * Works even when perspective is suspended for drawing, making it useful
     * for calculating anchor positions for no-projection structures.
     *
     * @par Coordinate Space
     * **Screen space -> Screen space** (camera-relative coordinates)
     * @code{.cpp}
     * // Input: world position minus camera
     * glm::vec2 screenPos(worldX - cameraPos.x, worldY - cameraPos.y);
     * glm::vec2 projected = renderer.ProjectPoint(screenPos);
     * @endcode
     *
     * @par Projection Modes
     * |               Mode |  Globe Curvature  | Vanishing Point |
     * |--------------------|-------------------|-----------------|
     * |     VanishingPoint |        No         | Yes             |
     * |              Globe |        Yes        | No              |
     * |   GlobePerspective |        Yes        | Yes             |
     *
     * @par Globe Curvature (Step 1)
     * Applies spherical distortion from screen center:
     * @f[
     * x' = center_x + R \cdot \sin\left(\frac{x - center_x}{R}\right)
     * @f]
     *
     * @par Vanishing Point (Step 2)
     * Scales point toward the vanishing point @f$ V = (center_x, horizon_y) @f$
     * based on vertical position. Points near the horizon shrink toward center,
     * points at screen bottom remain at full scale.
     *
     * @f[
     * scale = horizonScale + (1 - horizonScale) \cdot \frac{y - horizon_y}{viewHeight - horizon_y}
     * @f]
     *
     * Where:
     * - @f$ horizon_y @f$ vertical position of the horizon line in screen coordinates
     * - @f$ viewHeight @f$ total height of the viewport
     * - @f$ y @f$ vertical position of the input point
     *
     * @f[
     * x' = center_x + (x - center_x) \cdot scale
     * @f]
     *
     * @param p Screen-space point (world position - camera position).
     * @return Projected screen-space position.
     *
     * @see Tilemap::RenderSingleTile Uses this for no-projection structure anchors
     */
    glm::vec2 ProjectPoint(const glm::vec2& p) const
    {
        PerspectiveState s = GetPerspectiveState();
        if (!s.enabled) return p;

        // Use double precision to match renderer exactly
        double resultX = static_cast<double>(p.x);
        double resultY = static_cast<double>(p.y);
        double centerX = static_cast<double>(s.viewWidth) * 0.5;
        double centerY = static_cast<double>(s.viewHeight) * 0.5;

        bool applyGlobe = (s.mode == ProjectionMode::Globe || s.mode == ProjectionMode::Fisheye);
        bool applyVanishing = (s.mode == ProjectionMode::VanishingPoint || s.mode == ProjectionMode::Fisheye);

        // Step 1: Apply globe curvature using true spherical projection
        if (applyGlobe)
        {
            double R = static_cast<double>(s.sphereRadius);
            double dx = resultX - centerX;
            double dy = resultY - centerY;
            double d = std::sqrt(dx * dx + dy * dy);  // Radial distance from center

            if (d > 0.001)  // Avoid division by zero
            {
                // Project onto sphere: linear distance -> arc length -> projected distance
                double projectedD = R * std::sin(d / R);
                double ratio = projectedD / d;
                resultX = centerX + dx * ratio;
                resultY = centerY + dy * ratio;
            }
            // else: point is at center, no transformation needed
        }

        // Step 2: Apply vanishing point projection
        if (applyVanishing)
        {
            double horizonY = static_cast<double>(s.horizonY);
            double screenHeight = static_cast<double>(s.viewHeight);
            double horizonScale = static_cast<double>(s.horizonScale);

            double denom = screenHeight - horizonY;
            if (denom < 1e-5) return p;

            double t = (resultY - horizonY) / denom;
            t = std::max(0.0, std::min(1.0, t));

            // Match renderer exactly: horizonScale + (1 - horizonScale) * t
            double scale = horizonScale + (1.0 - horizonScale) * t;

            // Apply same transformation as renderer
            double dx = resultX - centerX;
            double dy = resultY - horizonY;
            resultX = centerX + dx * scale;
            resultY = horizonY + dy * scale;
        }

        return glm::vec2(static_cast<float>(resultX), static_cast<float>(resultY));
    }

    /**
     * @brief Set the rendering viewport.
     * 
     * Defines the rectangular region of the window to render into.
     * Typically matches the window size.
     * 
     * @param x Viewport left edge.
     * @param y Viewport bottom edge.
     * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     */
    virtual void SetViewport(int x, int y, int width, int height) = 0;

    /**
     * @brief Configure vanishing point perspective.
     *
     * Enables pseudo-3D depth scaling where objects closer to the horizon
     * appear smaller and converge toward a central vanishing point.
     * Sets projection mode to ProjectionMode::VanishingPoint.
     *
     * @param enabled Whether perspective effect is active.
     * @param horizonY Y position of the vanishing point in screen coordinates.
     * @param horizonScale Scale factor at the horizon (.0f - 1.f). Lower values
     *                     create stronger perspective (0.3 = 30% size at horizon).
     * @param viewWidth Current viewport width in pixels.
     * @param viewHeight Current viewport height in pixels.
     *
     * @see SetGlobePerspective() For curvature only.
     * @see SetFisheyePerspective() For combined curvature + depth scaling.
     * @see ProjectPoint() For the projection math details.
     */
    virtual void SetVanishingPointPerspective(bool enabled, float horizonY, float horizonScale,
                                              float viewWidth, float viewHeight) = 0;

    /**
     * @brief Configure globe curvature only.
     *
     * Wraps the world around a virtual sphere without depth scaling.
     * Sets projection mode to ProjectionMode::Globe.
     *
     * @param enabled Whether globe curvature is active.
     * @param sphereRadius Radius of the virtual sphere in pixels. Larger values
     *                     create subtler curvature (500 = tight curve).
     * @param viewWidth Current viewport width in pixels.
     * @param viewHeight Current viewport height in pixels.
     *
     * @see SetVanishingPointPerspective() For depth scaling only.
     * @see SetFisheyePerspective() For combined curvature + depth scaling.
     * @see ProjectPoint() For the projection math details.
     */
    virtual void SetGlobePerspective(bool enabled, float sphereRadius,
                                     float viewWidth, float viewHeight) = 0;

    /**
     * @brief Configure globe curvature with vanishing point.
     *
     * Combines spherical curvature with vanishing point depth scaling.
     * Sets projection mode to ProjectionMode::Fisheye.
     *
     * @see SetVanishingPointPerspective() For depth scaling only (no curvature).
     * @see SetGlobePerspective() For curvature only (no depth scaling).
     * @see ProjectPoint() For the projection math details.
     */
    virtual void SetFisheyePerspective(bool enabled, float sphereRadius,
                                       float horizonY, float horizonScale,
                                       float viewWidth, float viewHeight) = 0;

    /**
     * @brief Temporarily suspend perspective effect for next draw calls.
     * 
     * Call this before drawing elements that should not be affected by
     * perspective (e.g., player, NPCs). Call with false to resume perspective.
     * 
     * @param suspend True to suspend perspective, false to resume.
     */
    virtual void SuspendPerspective(bool suspend) = 0;

    /**
     * @brief Clear the screen to a solid color.
     * 
     * Fills the entire viewport with the specified color.
     * Should be called at the start of each frame.
     * 
     * @param r Red component (0-1).
     * @param g Green component (0-1).
     * @param b Blue component (0-1).
     * @param a Alpha component (0-1).
     */
    virtual void Clear(float r, float g, float b, float a) = 0;

    /**
     * @brief Ensure a texture is uploaded to GPU memory.
     * 
     * If the texture hasn't been uploaded yet, this creates the GPU
     * resource. Safe to call multiple times.
     * 
     * @par Lazy Loading
     * Textures are typically loaded from disk on first use.
     * This method forces immediate upload if needed.
     * 
     * @param texture Texture to upload.
     */
    virtual void UploadTexture(const Texture &texture) = 0;

    /**
     * @brief Draw text at the specified position.
     *
     * Renders a text string using the loaded font atlas.
     *
     * @par Font Rendering
     * Text is rendered using a bitmap font atlas. Each character
     * is drawn as a textured quad with the appropriate UV coordinates.
     *
     * @param text UTF-8 text string to render.
     * @param position Top-left corner of text.
     * @param scale Text scale multiplier (default: 1.0).
     * @param color Text color (default: white).
     * @param outlineSize Outline/shadow thickness multiplier (default: 1.0).
     * @param alpha Text transparency 0.0-1.0 (default: 0.85).
     */
    virtual void DrawText(const std::string &text, glm::vec2 position, float scale = 1.0f,
                          glm::vec3 color = glm::vec3(1.0f), float outlineSize = 1.0f,
                          float alpha = 0.85f) = 0;

    /**
     * @brief Get text line height metrics for alignment calculations.
     *
     * Returns the ascent (distance from baseline to top of tallest glyph)
     * scaled by the given scale factor. Use this to align UI elements
     * with rendered text.
     *
     * @param scale Text scale multiplier.
     * @return Scaled ascent in pixels.
     */
    virtual float GetTextAscent(float scale = 1.0f) const = 0;

    /**
     * @brief Measure the width of a text string.
     *
     * Returns the width in pixels that the text would occupy when rendered
     * at the given scale. Uses actual glyph advance values for accuracy.
     *
     * @param text Text string to measure.
     * @param scale Text scale multiplier.
     * @return Width in pixels.
     */
    virtual float GetTextWidth(const std::string& text, float scale = 1.0f) const = 0;

    /**
     * @brief Check if this renderer requires Y-axis flipping for textures.
     *
     * OpenGL uses bottom-left origin for textures, so tilesets that are
     * pre-flipped during loading require flipY=true when sampling.
     * Vulkan uses top-left origin, so no flipping is needed.
     *
     * @return true for OpenGL (needs Y-flip), false for Vulkan (no flip)
     */
    virtual bool RequiresYFlip() const = 0;

    /**
     * @brief Set the global ambient light color for day & night cycle.
     *
     * This color is multiplied with all textured sprites to create
     * the effect of changing light throughout the day.
     *
     * @param color RGB ambient color (1,1,1 = full brightness, no tint).
     */
    virtual void SetAmbientColor(const glm::vec3& color) = 0;

    /**
     * @brief Get the number of draw calls made this frame.
     *
     * Returns the count of GPU draw calls (batch flushes) since the last
     * BeginFrame(). Useful for performance debugging and optimization.
     *
     * @return Number of draw calls this frame.
     */
    virtual int GetDrawCallCount() const = 0;
};
