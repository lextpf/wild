#pragma once

#include "Tilemap.h"
#include "PlayerCharacter.h"
#include "NonPlayerCharacter.h"
#include "ParticleSystem.h"
#include "DialogueManager.h"
#include "GameStateManager.h"
#include "TimeManager.h"
#include "SkyRenderer.h"
#include "IRenderer.h"
#include "RendererAPI.h"
#include "RendererFactory.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

/**
 * @class Game
 * @brief Central game manager handling the main loop and all subsystems.
 * @author Alex (https://github.com/lextpf)
 * @ingroup Core
 * 
 * The Game class is the application's entry point and primary coordinator.
 * It owns all major game systems and manages their lifecycle.
 * 
 * @par Game Loop
 * Uses a simple variable-timestep loop:
 * @code
 * while (!shouldClose) {
 *     float deltaTime = currentTime - lastTime;
 *     ProcessInput(deltaTime);
 *     Update(deltaTime);
 *     Render();
 * }
 * @endcode
 * 
 * @par Frame Timing
 * Delta time can be clamped to prevent physics explosions (e.g. after
 * debugger pauses); the current implementation does not clamp yet. See
 * Run() for the integration point.
 * 
 * @par Game Modes
 * The game supports multiple modes:
 *
 * | Mode     | Input          | Features                          |
 * |----------|----------------|-----------------------------------|
 * | Gameplay | WASD movement  | Player control, NPC interaction   |
 * | Dialogue | W/S + Enter    | Conversation with NPCs            |
 * | Editor   | Mouse + keys   | Tile placement, collision editing |
 *
 * Toggle editor mode with **E**. Dialogue activates on NPC interaction.
 * 
 * @par Camera System
 * The camera follows the player with smooth interpolation:
 * @f[
 * camera_{new} = camera_{old} + (target - camera_{old}) \times \alpha
 * @f]
 * 
 * Where @f$ \alpha @f$ is calculated for a specific settle time.
 * The camera is also clamped to keep the player centered in the viewport.
 * 
 * @par Render Order
 * The game renders in this order for correct depth:
 * 1. Background layers (Ground, Ground Detail, Objects, Objects2, Objects3) - skips Y-sorted/no-projection tiles
 * 2. Background no-projection tiles (buildings rendered upright, perspective suspended)
 * 3. Y-sorted pass: Y-sorted tiles from ALL layers + NPCs + Player (sorted by Y coordinate)
 * 4. Foreground no-projection tiles (rendered upright)
 * 5. No-projection particles (perspective suspended)
 * 6. Foreground layers (Foreground, Foreground2, Overlay, Overlay2, Overlay3) - skips Y-sorted/no-projection tiles
 * 7. Regular particles
 * 8. Sky/ambient overlay (stars, rays, atmospheric effects)
 * 9. Editor UI (if active)
 * 10. Debug overlays (collision, navigation, layer indicators)
 * 
 * @par Viewport Configuration
 * The game uses a virtual resolution based on visible tiles:
 * - 17 tiles wide x 12 tiles tall
 * - At 16px per tile = 272x192 virtual pixels
 * - Scaled to fit window while maintaining aspect ratio
 * 
 * @htmlonly
 * <pre class="mermaid">
 * graph LR
 * classDef core fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 * classDef render fill:#2e1f5e,stroke:#8b5cf6,color:#e2e8f0
 * classDef world fill:#134e3a,stroke:#10b981,color:#e2e8f0
 * classDef entity fill:#4a3520,stroke:#f59e0b,color:#e2e8f0
 * classDef input fill:#164e54,stroke:#06b6d4,color:#e2e8f0
 *
 * Game((Game)):::core
 * Game --> Tilemap:::world
 * Game --> Player[PlayerCharacter]:::entity
 * Game --> NPCs[vector of NPC]:::entity
 * Game --> Renderer[IRenderer]:::render
 * Game --> Window[GLFW]:::input
 * Tilemap --> CollisionMap:::world
 * Tilemap --> NavigationMap:::world
 * </pre>
 * @endhtmlonly
 * 
 * @par Lifecycle
 * @code
 * Game g;
 * g.Initialize();  // Create window, load assets
 * g.Run();         // Main loop (blocks until window closes)
 * g.Shutdown();    // Release resources
 * @endcode
 * 
 * @see PlayerCharacter, Tilemap, IRenderer
 */
class Game
{
public:
    /**
     * @brief Construct a new Game object.
     * 
     * Does not initialize resources; call Initialize() separately.
     */
    Game();

    /**
     * @brief Destructor calls Shutdown() if not already called.
     */
    ~Game();

    /**
     * @brief Initialize all game systems.
     * 
     * Performs the following initialization sequence:
     * 1. Initialize GLFW and create window
     * 2. Create renderer (OpenGL, can switch to Vulkan)
     * 3. Load tileset and create tilemap
     * 4. Load player character sprites
     * 5. Load map from JSON (or generate default)
     * 6. Initialize NPC patrol routes
     * 7. Set up camera position
     * 
     * @par Error Handling
     * Returns false if any critical initialization fails.
     * Error messages are printed to stderr.
     * 
     * @return `true` if initialization succeeded.
     */
    bool Initialize();

    /**
     * @brief Starts and maintains the engine's main game loop (variable timestep).
     *
     * @details
     * This function is **blocking** and returns only when the application is asked to exit
     *
     * The loop uses a **variable timestep**: each iteration computes a frame-to-frame
     * delta time based on the current GLFW time and forwards it to the simulation and rendering
     * stages.
     *
     * @par Per-frame execution order
     * Each frame performs the following steps in order:
     * - Compute @p deltaTime since the previous frame
     * - @ref ProcessInput(float) "ProcessInput(deltaTime)"
     * - @ref Update(float) "Update(deltaTime)"
     * - @ref Render() "Render()"
     * - Poll GLFW events via @c glfwPollEvents()
     *
     * @see ProcessInput(float)
     * @see Update(float)
     * @see Render()
     */
    void Run();

    /**
     * @brief Shutdown and release all resources.
     * 
     * Performs cleanup in reverse initialization order:
     * 1. Destroy renderer
     * 2. Destroy GLFW window
     * 3. Terminate GLFW
     * 
     * Safe to call multiple times.
     */
    void Shutdown();
    
    /**
     * @brief Set the target FPS limit.
     * @param fps Target FPS (<=0 = unlimited, default).
     */
    void SetTargetFps(float fps) { m_TargetFps = fps; }
    
    /**
     * @brief Switch to a different renderer API at runtime.
     * @param api The renderer API to switch to (OpenGL or Vulkan).
     * @return true if switch was successful, false otherwise.
     * 
     * This destroys the current renderer and creates a new one.
     * Textures and other GPU resources will need to be re-uploaded.
     */
    bool SwitchRenderer(RendererAPI api);
    
    /**
     * @brief Get the currently active renderer API.
     * @return The active renderer API.
     */
    RendererAPI GetRendererAPI() const { return m_RendererAPI; }

    /**
     * @brief GLFW scroll callback for tile picker navigation.
     *
     * Static callback registered with GLFW to handle mouse wheel events.
     * Behavior depends on current mode:
     * - **Elevation edit mode** (no Ctrl): Adjusts elevation paint value (0-32)
     * - **Ctrl+scroll** (tile picker closed): Camera zoom (0.1x-4.0x)
     * - **Ctrl+scroll** (tile picker open): Tile picker zoom
     * - **Scroll** (tile picker open, no Ctrl): Tile picker navigation
     *
     * @param window GLFW window handle.
     * @param xoffset Horizontal scroll offset (unused).
     * @param yoffset Vertical scroll offset (positive = up/zoom in, negative = down/zoom out).
     */
    static void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset);

private:
    /**
     * @brief Process keyboard and mouse input.
     *
     * Handles both gameplay and editor input based on current mode.
     * See @ref GameInput for complete input documentation.
     *
     * @param deltaTime Frame time in seconds (for movement scaling).
     */
    void ProcessInput(float deltaTime);

    /**
     * @brief Update game state.
     * 
     * Updates all dynamic elements:
     * 1. Player animation
     * 2. NPC AI and animation
     * 3. Camera following
     * 4. Dialogue state
     * 
     * @param deltaTime Frame time in seconds.
     */
    void Update(float deltaTime);

    /**
     * @brief Render all game elements.
     * 
     * Performs the full render pass:
     * 1. Begin frame (clear, set projection)
     * 2. Render tilemap layers (with depth ordering)
     * 3. Render entities (NPCs, player)
     * 4. Render editor UI (if active)
     * 5. Render debug overlays (if enabled)
     * 6. End frame
     */
    void Render();

    /**
     * @brief Compute a projection matrix with 3D globe effect.
     * 
     * Configures the renderer's perspective settings based on whether
     * the 3D globe effect is enabled.
     *
     * @param width  World width in pixels.
     * @param height World height in pixels.
     */
    void ConfigureRendererPerspective(float width, float height);

    /**
     * @brief Get a standard orthographic projection matrix.
     *
     * @param width  World width in pixels.
     * @param height World height in pixels.
     * @return Orthographic projection matrix.
     */
    glm::mat4 GetOrthoProjection(float width, float height);

    /**
     * @brief Toggle the 3D globe effect on/off.
     */
    void Toggle3DEffect();

    /**
     * @brief Process mouse input for editor interactions.
     * 
     * Handles:
     * - Tile picker selection
     * - World tile placement
     * - Collision/navigation painting
     * - NPC placement/removal
     * - Multi-tile selection and rotation
     */
    void ProcessMouseInput();

    /**
     * @brief Render the editor UI overlay.
     * 
     * Draws:
     * - Current layer indicator
     * - Selected tile preview
     * - Tile picker panel (when open)
     * - Mode indicators (NPC, navigation, etc.)
     */
    // TODO: Move console indicators to UI system.
    void RenderEditorUI();

    /**
     * @brief Render collision debug overlays.
     * 
     * Draws red semi-transparent tiles over collision areas.
     * Also shows player and NPC hitboxes when debug mode is active.
     */
    void RenderCollisionOverlays();

    /**
     * @brief Render Ground Detail layer overlays.
     *
     * Draws blue indicators for Ground Detail tiles (index 1) in editor mode.
     */
    void RenderLayer2Overlays();

    /**
     * @brief Render Objects layer overlays.
     *
     * Draws green indicators for Objects tiles (index 2) in editor mode.
     */
    void RenderLayer3Overlays();

    /**
     * @brief Render Objects2 layer overlays.
     *
     * Draws magenta indicators for Objects2 tiles (index 3) in editor mode.
     */
    void RenderLayer4Overlays();

    /**
     * @brief Render Objects3 layer overlays.
     *
     * Draws orange indicators for Objects3 tiles (index 4) in editor mode.
     */
    void RenderLayer5Overlays();

    /**
     * @brief Render Foreground layer overlays.
     *
     * Draws yellow indicators for Foreground tiles (index 5) in editor mode.
     */
    void RenderLayer6Overlays();

    /**
     * @brief Render Foreground2 layer overlays.
     *
     * Draws cyan indicators for Foreground2 tiles (index 6) in editor mode.
     */
    void RenderLayer7Overlays();

    /**
     * @brief Render Overlay layer overlays.
     *
     * Draws red indicators for Overlay tiles (index 7) in editor mode.
     */
    void RenderLayer8Overlays();

    /**
     * @brief Render Overlay2 layer overlays.
     *
     * Draws magenta indicators for Overlay2 tiles (index 8) in editor mode.
     */
    void RenderLayer9Overlays();

    /**
     * @brief Render Overlay3 layer overlays.
     *
     * Draws white indicators for Overlay3 tiles (index 9) in editor mode.
     */
    void RenderLayer10Overlays();

    /**
     * @brief Render navigation mesh overlays.
     * 
     * Draws cyan indicators for walkable tiles (navigation = true).
     */
    void RenderNavigationOverlays();

    /**
     * @brief Render elevation value overlays.
     * 
     * Draws purple indicators showing elevation values on tiles.
     * Higher elevations are shown with darker/more opaque colors.
     */
    void RenderElevationOverlays();
    /**
     * @brief Render no-projection tile overlays.
     * 
     * Draws orange indicators for tiles marked as no-projection.
     */
    void RenderNoProjectionOverlays();

    /**
     * @brief Render no-projection structure anchors.
     *
     * Draws green cross markers at the bottom-left and bottom-right anchor
     * positions for all no-projection structures, using the same projection
     * calculations as the actual rendering. Rendered on top of everything.
     */
    void RenderNoProjectionAnchors();

    /**
     * @brief Render structure edit mode overlays.
     *
     * When in structure edit mode, draws:
     * - Blue markers for defined structure anchors
     * - Purple overlay for tiles assigned to structures
     * - Temporary anchor markers when placing
     */
    void RenderStructureOverlays();

    /**
     * @brief Render Y-sort-plus tile overlays.
     *
     * Draws cyan indicators for tiles marked as Y-sort-plus.
     */
    void RenderYSortPlusOverlays();

    /**
     * @brief Render Y-sort-minus tile overlays.
     *
     * Draws magenta indicators for tiles marked with Y-sort-minus flag.
     */
    void RenderYSortMinusOverlays();

    /**
     * @brief Render particle zone overlays.
     *
     * Draws indicators for tiles that are particle zones.
     */
    void RenderParticleZoneOverlays();

    /**
     * @brief Render NPC debug information.
     * 
     * Displays:
     * - Patrol route waypoints and connections
     * - Current target tile
     * - NPC hitboxes
     * - Movement direction indicators
     */
    void RenderNPCDebugInfo();

    /**
     * @brief Render corner cutting debug overlays.
     * 
     * Visualizes which corners allow diagonal movement passage.
     */
    void RenderCornerCuttingOverlays();

    /**
     * @brief Render multi-tile placement preview.
     * 
     * Shows a ghost preview of selected tiles before placement.
     * Includes rotation visualization.
     */
    void RenderPlacementPreview();

    /**
     * @brief Render simple dialogue text above NPC's head.
     *
     * Fallback for NPCs without dialogue trees.
     */
    void RenderNPCHeadText();

    /**
     * @brief Render text inside the dialogue box.
     *
     * @param boxPos  Center position of dialogue box.
     * @param boxSize Dimensions of dialogue box.
     */
    void RenderDialogueText(glm::vec2 boxPos, glm::vec2 boxSize);

    /**
     * @brief Render branching dialogue tree UI.
     *
     * Shows dialogue text and response options for tree-based dialogue.
     */
    void RenderDialogueTreeBox();

    /**
     * @brief Check if dialogue is on the last page.
     * @return True if on last page or no dialogue active.
     */
    bool IsDialogueOnLastPage() const;

    /**
     * @brief Recalculate patrol routes for all NPCs.
     * 
     * Called when navigation map changes. Each NPC's patrol route
     * is regenerated from its current position.
     */
    void RecalculateNPCPatrolRoutes();

    /// @name Window Management
    /// @{
    GLFWwindow *m_Window;   ///< GLFW window handle
    int m_ScreenWidth;      ///< Window width in pixels
    int m_ScreenHeight;     ///< Window height in pixels
    /// @}

    /**
     * @name Viewport Settings
     * @brief Define the virtual game resolution based on window size.
     * 
     * The number of visible tiles is calculated from window size, with the
     * window size snapped to tile boundaries (16 pixel increments) for clean rendering.
     * @{
     */
    int m_TilesVisibleWidth;                    ///< Tiles visible horizontally (based on window width)
    int m_TilesVisibleHeight;                   ///< Tiles visible vertically (based on window height)
    static constexpr int TILE_PIXEL_SIZE = 16;  ///< Size of a tile in pixels
    static constexpr int PIXEL_SCALE = 5;       ///< Scale factor for rendering (5x)
    float m_ResizeSnapTimer;                    ///< Timer for deferred window snap after resize
    bool m_PendingWindowSnap;                   ///< Whether a window snap is pending
    /** @} */
    
    /**
     * @brief Handle window resize - updates viewport immediately, defers snap.
     * @param width  New framebuffer width
     * @param height New framebuffer height
     */
    void OnFramebufferResized(int width, int height);
    
    /**
     * @brief Snap window to tile boundaries (called after resize settles).
     */
    void SnapWindowToTileBoundaries();
    
    /**
     * @brief GLFW framebuffer size callback.
     */
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

    /**
     * @name Game Entities
     * @brief Core game objects.
     * @{
     */
    Tilemap m_Tilemap;                       ///< The game world
    PlayerCharacter m_Player;                ///< Player-controlled character
    std::vector<NonPlayerCharacter> m_NPCs;  ///< All NPCs in the world
    ParticleSystem m_Particles;              ///< Ambient particle effects (fireflies, etc.)
    TimeManager m_TimeManager;               ///< Day/night cycle time management
    SkyRenderer m_SkyRenderer;               ///< Sky rendering (sun, moon, stars)
    std::unique_ptr<IRenderer> m_Renderer;   ///< Graphics renderer
    RendererAPI m_RendererAPI;               ///< Active renderer type
    /** @} */

    /**
     * @name Camera State
     * @brief Variables controlling camera movement.
     *
     * The camera uses exponential smoothing to follow the player. The "follow target"
     * pattern enables smooth transitions: when m_HasCameraFollowTarget is true, the
     * camera interpolates from m_CameraPosition toward m_CameraFollowTarget each frame.
     * When false, the camera snaps instantly to the player position (used on load/teleport).
     * @{
     */
    glm::vec2 m_CameraPosition;      ///< Current camera world position (rendered position)
    glm::vec2 m_CameraFollowTarget;  ///< Target position camera is smoothing toward
    bool m_HasCameraFollowTarget;    ///< True = smooth follow mode, false = instant snap mode
    float m_CameraZoom;              ///< Zoom multiplier (1.0 = 100%)
    float m_CameraTilt;              ///< Tilt angle for 3D effect (0.0 = flat, 1.0 = max tilt)
    bool m_Enable3DEffect;           ///< Whether 3D tilt effect is active
    float m_GlobeSphereRadius;       ///< Radius for globe + vanishing point projection (larger = subtler)
    bool m_FreeCameraMode;           ///< Free camera mode (Space toggle) - camera doesn't follow player
    /** @} */

    float m_LastFrameTime;  ///< Timestamp of last frame (for delta calculation)

    /**
     * @name FPS Counter
     * @brief Frame rate measurement.
     * @{
     */
    float m_FpsUpdateTimer;     ///< Accumulator for FPS update interval
    float m_FpsConsoleTimer;    ///< Timer for console FPS output (every 2s)
    int m_FrameCount;           ///< Frames since last FPS update
    float m_CurrentFps;         ///< Calculated FPS for display
    float m_TargetFps;          ///< Target FPS limit (<=0 = unlimited)
    int m_DrawCallAccumulator;  ///< Accumulated draw calls since last update
    int m_CurrentDrawCalls;     ///< Average draw calls per frame for display
    /** @} */

    /**
     * @name Editor State
     * @brief Variables for the level editor.
     * @{
     */
    bool m_EditorMode;                             ///< Editor mode active
    bool m_ShowTilePicker;                         ///< Tile picker visible
    bool m_EditNavigationMode;                     ///< Right-click edits navigation (not collision)
    bool m_ElevationEditMode;                      ///< Left-click paints elevation values
    bool m_NPCPlacementMode;                       ///< Left-click places/removes NPCs
    bool m_NoProjectionEditMode;                   ///< Left-click toggles no-projection flag
    bool m_YSortPlusEditMode;                      ///< Left-click toggles Y-sort-plus flag
    bool m_YSortMinusEditMode;                     ///< Left-click toggles Y-sort-minus flag
    bool m_ParticleZoneEditMode;                   ///< Left-click places/removes particle zones
    ParticleType m_CurrentParticleType;            ///< Current particle type for zone placement
    bool m_ParticleNoProjection;                   ///< If true, new particle zones use no projection
    bool m_PlacingParticleZone;                    ///< Currently dragging to create a zone
    glm::vec2 m_ParticleZoneStart;                 ///< Start position of zone being placed
    bool m_StructureEditMode;                      ///< Structure definition mode (K key)
    int m_CurrentStructureId;                      ///< Currently selected structure (-1 = none/new)
    int m_PlacingAnchor;                           ///< 0=not placing, 1=left anchor, 2=right anchor
    glm::vec2 m_TempLeftAnchor;                    ///< Temporary left anchor world position
    glm::vec2 m_TempRightAnchor;                   ///< Temporary right anchor world position
    bool m_AssigningTilesToStructure;              ///< Shift held - assigning tiles to structure
    bool m_AnimationEditMode;                      ///< Creating animated tile definitions
    std::vector<int> m_AnimationFrames;            ///< Frames being collected for new animation
    float m_AnimationFrameDuration;                ///< Frame duration for new animation (seconds)
    int m_SelectedAnimationId;                     ///< Currently selected animation to apply (-1 = none)
    bool m_DebugMode;                              ///< Show all debug overlays (F3)
    bool m_ShowDebugInfo;                          ///< Show FPS and position info (F4)
    bool m_ShowNoProjectionAnchors;                ///< Show all no-projection anchors (F6)
    int m_SelectedTileID;                          ///< Currently selected tile
    int m_CurrentLayer;                            ///< Active editing layer (0-7, maps to dynamic layers)
    int m_CurrentElevation;                        ///< Elevation value for painting (pixels)
    std::vector<std::string> m_AvailableNPCTypes;  ///< NPC type names
    size_t m_SelectedNPCTypeIndex;                 ///< Selected NPC type
    double m_LastMouseX;                           ///< Previous mouse X
    double m_LastMouseY;                           ///< Previous mouse Y
    bool m_MousePressed;                           ///< Left mouse button state
    bool m_RightMousePressed;                      ///< Right mouse button state
    int m_LastPlacedTileX;                         ///< Last placed tile X (for drag)
    int m_LastPlacedTileY;                         ///< Last placed tile Y
    int m_LastNavigationTileX;                     ///< Last navigation edit X
    int m_LastNavigationTileY;                     ///< Last navigation edit Y
    bool m_NavigationDragState;                    ///< Navigation paint state (on/off)
    int m_LastCollisionTileX;                      ///< Last collision edit X
    int m_LastCollisionTileY;                      ///< Last collision edit Y
    bool m_CollisionDragState;                     ///< Collision paint state
    int m_LastNPCPlacementTileX;                   ///< Last NPC placement tile X
    int m_LastNPCPlacementTileY;                   ///< Last NPC placement tile Y
    /** @} */

    /**
     * @name Tile Picker State
     * @brief Variables for the tile selection UI.
     * @{
     */
    float m_TilePickerZoom;           ///< Picker zoom level
    float m_TilePickerOffsetX;        ///< Horizontal pan offset
    float m_TilePickerOffsetY;        ///< Vertical pan offset
    float m_TilePickerTargetOffsetX;  ///< Target horizontal offset (smooth)
    float m_TilePickerTargetOffsetY;  ///< Target vertical offset (smooth)
    /** @} */

    /**
     * @name Multi-Tile Selection
     * @brief Variables for selecting and placing multiple tiles.
     * @{
     */
    bool m_MultiTileSelectionMode;   ///< Multi-tile mode active
    int m_SelectedTileStartID;       ///< First tile in selection
    int m_SelectedTileWidth;         ///< Selection width in tiles
    int m_SelectedTileHeight;        ///< Selection height in tiles
    bool m_IsSelectingTiles;         ///< Currently dragging selection
    int m_SelectionStartTileID;      ///< Drag start tile
    float m_PlacementCameraZoom;     ///< Camera zoom during placement
    bool m_IsPlacingMultiTile;       ///< Placement mode active
    int m_MultiTileRotation;         ///< Rotation (0, 90, 180, 270)
    /** @} */

    /**
     * @name Collision Resolution
     * @brief For movement rollback.
     * @{
     */
    glm::vec2 m_PlayerPreviousPosition;  ///< Position before movement (for rollback)
    /** @} */

    /**
     * @name Dialogue System
     * @brief NPC dialogue UI state.
     * @{
     */
    bool m_InDialogue;                     ///< Dialogue mode active (simple dialogue)
    NonPlayerCharacter *m_DialogueNPC;     ///< NPC being talked to
    std::string m_DialogueText;            ///< Current dialogue text (simple dialogue)
    DialogueManager m_DialogueManager;     ///< Branching dialogue tree manager
    GameStateManager m_GameState;          ///< Game flags and state for consequences
    int m_DialoguePage = 0;                ///< Current page of dialogue text (for pagination)
    mutable int m_DialogueTotalPages = 1;  ///< Total pages (cached during rendering)
    /** @} */
};
