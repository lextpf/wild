#pragma once

#include "Texture.h"
#include "IRenderer.h"
#include "Tilemap.h"
#include "PatrolRoute.h"
#include "DialogueSystem.h"

#include <glm/glm.hpp>
#include <string>

/**
 * @enum NPCDirection
 * @brief NPC facing direction (matches sprite sheet row layout).
 * @author Alex (https://github.com/lextpf)
 */
enum class NPCDirection
{
    DOWN = 0,   ///< Facing down (+Y direction)
    UP = 1,     ///< Facing up (-Y direction)
    LEFT = 2,   ///< Facing left (-X direction)
    RIGHT = 3   ///< Facing right (+X direction)
};

/**
 * @class NonPlayerCharacter
 * @brief Character with patrol behavior and player interaction.
 * @author Alex (https://github.com/lextpf)
 * @ingroup Entities
 * 
 * NonPlayerCharacter represents an autonomous entity in the game world.
 * NPCs follow patrol routes through the navigation map and can interact
 * with the player through collision and dialogue.
 * 
 * @par AI Behavior System
 * NPCs follow a patrol route calculated from the navigation map.
 * The behavior varies based on route validity:
 *
 * |       Condition | Behavior                             |
 * |-----------------|--------------------------------------|
 * |     Valid route | Walk patrol route with random pauses |
 * |  No valid route | Stand still and look around          |
 * | Player blocking | Stop and face player                 |
 *
 * @par State Machine
 * @htmlonly
 * <pre class="mermaid">
 * stateDiagram-v2
 *     direction LR
 *
 *     classDef walking fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *     classDef stopped fill:#4a2020,stroke:#ef4444,color:#e2e8f0
 *     classDef noroute fill:#2e1f5e,stroke:#8b5cf6,color:#e2e8f0
 *     classDef paused fill:#4a3520,stroke:#f59e0b,color:#e2e8f0
 *
 *     [*] --> Walking: RouteInitSuccess
 *     [*] --> NoRoute: RouteInitFailed
 *
 *     Walking: Walking<br/>(Following Route)
 *     Stopped: Stopped<br/>(Dialogue/Collision)
 *     NoRoute: No Route<br/>(Looking Around)
 *     RandomPause: Random Pause<br/>(2-5 seconds)
 *
 *     Walking --> Stopped: Dialogue/Collision
 *     Walking --> RandomPause: 30% at waypoint
 *     Walking --> NoRoute: RouteInitFailed
 *
 *     RandomPause --> Stopped: Dialogue/Collision
 *     RandomPause --> Walking: TimerExpired
 *
 *     NoRoute --> Stopped: Dialogue/Collision
 *     NoRoute --> NoRoute: ChangeDir (2s)
 *     NoRoute --> Walking: ReinitializePatrolRoute()
 *
 *     Stopped --> Walking: Resume (was Walking)
 *     Stopped --> NoRoute: Resume (was NoRoute)
 *     Stopped --> RandomPause: Resume (was Paused)
 *
 *     class Walking walking
 *     class Stopped stopped
 *     class NoRoute noroute
 *     class RandomPause paused
 * </pre>
 * @endhtmlonly
 *
 * @par Patrol Route Algorithm
 * Routes are generated using DFS spanning tree traversal from the spawn tile:
 * 1. Verify start tile is walkable (navigation=true, collision=false)
 * 2. DFS traversal recording every step including backtracks
 * 3. Result: path visits every reachable tile at least once
 * 4. Check if route is "closeable" (end tile adjacent to start)
 * 5. If closeable: loop mode (A-B-C-A-B-C-...)
 * 6. If not closeable: ping-pong mode (A-B-C-B-A-B-...)
 *
 * @see PatrolRoute for detailed algorithm documentation
 * 
 * @par Movement Physics
 * NPCs move at a constant speed (32 px/s) and always snap to tile centers:
 * @f[
 * speed = 32 \, \text{px/s} = 2 \, \text{tiles/s}
 * @f]
 * 
 * Movement is **tile-to-tile**: NPCs complete movement to one tile
 * before selecting the next waypoint.
 * 
 * @par Animation System
 * Uses the same 4-direction, 3-frame walk cycle as PlayerCharacter:
 * - Frame sequence: [1, 0, 2, 0] (step left, idle, step right, idle)
 * - Frame duration: 0.15 seconds
 * 
 * @par Collision Model
 * NPC hitbox is 16x16 pixels, matching player dimensions:
 * @f[
 * hitbox_{min} = (feet_x - 8, feet_y - 16)
 * @f]
 * @f[
 * hitbox_{max} = (feet_x + 8, feet_y)
 * @f]
 * 
 * @par Memory Model
 * NonPlayerCharacter is:
 * - **Non-copyable**: Contains Texture (OpenGL resource ownership)
 * - **Movable**: Supports std::vector storage and reallocation
 * 
 * @see PatrolRoute, NavigationMap, PlayerCharacter
 */
class NonPlayerCharacter
{
public:
    /**
     * @brief Construct an uninitialized NPC.
     * 
     * Call Load() and SetTilePosition() before use.
     */
    NonPlayerCharacter();

    /**
     * @brief Deleted copy constructor (Texture is non-copyable).
     */
    NonPlayerCharacter(const NonPlayerCharacter &) = delete;

    /**
     * @brief Deleted copy assignment (Texture is non-copyable).
     */
    NonPlayerCharacter &operator=(const NonPlayerCharacter &) = delete;

    /**
     * @brief Move constructor for container storage.
     */
    NonPlayerCharacter(NonPlayerCharacter &&) noexcept = default;

    /**
     * @brief Move assignment for container storage.
     */
    NonPlayerCharacter &operator=(NonPlayerCharacter &&) noexcept = default;

    /**
     * @brief Load NPC sprite sheet from file.
     * 
     * Loads the sprite sheet and extracts the NPC type from the filename.
     * 
     * @par Filename Convention
     * Expected format: `assets/non-player/{TypeName}.png`
     * Example: `assets/non-player/BW2_NPC1.png` -> type = "BW2_NPC1"
     * 
     * @par Sprite Sheet Format
     * Same layout as PlayerCharacter walking sheet:
     * @code
     *   Frame:  0 (idle)  1 (left)  2 (right)
     *         +--------+--------+--------+
     * Row 0:  |  Down  |  Down  |  Down  |
     *         +--------+--------+--------+
     * Row 1:  |   Up   |   Up   |   Up   |
     *         +--------+--------+--------+
     * Row 2:  |  Left  |  Left  |  Left  |
     *         +--------+--------+--------+
     * Row 3:  | Right  | Right  | Right  |
     *         +--------+--------+--------+
     * @endcode
     * Each frame is 32x32 pixels.
     * 
     * @param relativePath Path to sprite sheet (relative to working directory).
     * @return `true` if loaded successfully.
     */
    bool Load(const std::string &relativePath);

    /**
     * @brief Upload sprite texture to the renderer.
     * 
     * Called when switching renderers to ensure texture is properly
     * recreated in the new graphics context.
     * 
     * @param renderer Reference to the active renderer.
     */
    void UploadTextures(IRenderer &renderer);

    /**
     * @brief Set NPC position by tile coordinates.
     * 
     * Calculates feet position from tile coordinates:
     * @f[
     * feet_x = tileX \times tileSize + \frac{tileSize}{2}
     * @f]
     * @f[
     * feet_y = tileY \times tileSize + tileSize
     * @f]
     * 
     * @param tileX Tile column.
     * @param tileY Tile row.
     * @param tileSize Tile size in pixels (typically 16).
     * @param preservePatrolRoute If true, keeps the current patrol route instead of resetting it.
     */
    void SetTilePosition(int tileX, int tileY, int tileSize, bool preservePatrolRoute = false);

    /**
     * @brief Update NPC AI and animation.
     * 
     * Performs the following each frame:
     * 1. Update animation timer and frame
     * 2. Check patrol route for next waypoint
     * 3. Move towards current target
     * 4. Check for player collision
     * 5. Handle random standing still
     * 
     * @par Movement Algorithm
     * @code
     * if (distance_to_target < 0.5px):
     *     get_next_waypoint()
     * else:
     *     move_toward_target(speed * deltaTime)
     *     update_facing_direction()
     * @endcode
     * 
     * @par Collision Avoidance
     * If player is within 16px, NPC stops and faces the player.
     * 
     * @param deltaTime Frame time in seconds.
     * @param tilemap Tilemap for navigation queries.
     * @param playerPosition Optional player position for collision.
     */
    void Update(float deltaTime, const Tilemap *tilemap, const glm::vec2 *playerPosition = nullptr);

    /**
     * @brief Render the full NPC sprite.
     * 
     * Draws the sprite at current position with current animation frame.
     * 
     * @param renderer Active renderer.
     * @param cameraPos Camera position for world-to-screen conversion.
     */
    void Render(IRenderer &renderer, glm::vec2 cameraPos) const;

    /**
     * @brief Render bottom half of sprite (for depth sorting).
     * 
     * Used when NPCs need to appear behind foreground tiles.
     * Draws the lower 16px of the 32px sprite.
     * 
     * @param renderer Active renderer.
     * @param cameraPos Camera position.
     */
    void RenderBottomHalf(IRenderer &renderer, glm::vec2 cameraPos) const;

    /**
     * @brief Render top half of sprite (for depth sorting).
     * 
     * Used when NPCs need to appear in front of background tiles.
     * Draws the upper 16px of the 32px sprite.
     * 
     * @param renderer Active renderer.
     * @param cameraPos Camera position.
     */
    void RenderTopHalf(IRenderer &renderer, glm::vec2 cameraPos) const;

    /**
     * @brief Get NPC feet position in world coordinates.
     * @return Position vector.
     */
    glm::vec2 GetPosition() const { return m_Position; }

    /**
     * @brief Set target elevation offset for stairs/ramps (smoothly transitions).
     * @param offset Target Y offset in pixels (positive = rendered higher).
     */
    void SetElevationOffset(float offset)
    {
        if (offset != m_TargetElevation)
        {
            m_ElevationStart = m_ElevationOffset;
            m_TargetElevation = offset;
            m_ElevationProgress = 0.0f;
        }
    }

    /**
     * @brief Get current visual elevation offset.
     * @return Y offset in pixels.
     */
    float GetElevationOffset() const { return m_ElevationOffset; }

    /**
     * @brief Get current tile X coordinate.
     * @return Tile column.
     */
    int GetTileX() const { return m_TileX; }

    /**
     * @brief Get current tile Y coordinate.
     * @return Tile row.
     */
    int GetTileY() const { return m_TileY; }

    /**
     * @brief Get NPC type identifier.
     *
     * Type is extracted from sprite sheet filename.
     *
     * @return Type string (e.g., "BW2_NPC1").
     */
    const std::string &GetType() const { return m_Type; }

    /**
     * @brief Get full path to NPC sprite sheet.
     *
     * Constructs the sprite path from the NPC type.
     *
     * @return Sprite sheet path (e.g., "assets/non-player/BW2_NPC1.png").
     */
    std::string GetSpritePath() const { return "assets/non-player/" + m_Type + ".png"; }

    /**
     * @brief Check if NPC is stopped (e.g., player collision).
     * @return `true` if not moving.
     */
    bool IsStopped() const { return m_IsStopped; }

    /**
     * @brief Set stopped state.
     * @param stopped `true` to stop movement.
     */
    void SetStopped(bool stopped) { m_IsStopped = stopped; }

    /**
     * @brief Set facing direction.
     * @param dir Direction to face.
     */
    void SetDirection(NPCDirection dir) { m_Direction = dir; }

    /**
     * @brief Get NPC display name.
     * @return Name string for dialogue display.
     */
    const std::string &GetName() const { return m_Name; }

    /**
     * @brief Set NPC display name.
     * @param name Name to display in dialogue.
     */
    void SetName(const std::string &name) { m_Name = name; }

    /**
     * @brief Get dialogue text for this NPC.
     * @return Dialogue string.
     */
    const std::string &GetDialogue() const { return m_Dialogue; }

    /**
     * @brief Set dialogue text.
     * @param dialogue Text to display when player interacts.
     */
    void SetDialogue(const std::string &dialogue) { m_Dialogue = dialogue; }

    /**
     * @brief Get dialogue tree for branching dialogue.
     * @return Reference to dialogue tree.
     */
    const DialogueTree& GetDialogueTree() const { return m_DialogueTree; }

    /**
     * @brief Get mutable dialogue tree for editing.
     * @return Reference to dialogue tree.
     */
    DialogueTree& GetDialogueTree() { return m_DialogueTree; }

    /**
     * @brief Set dialogue tree for branching dialogue.
     * @param tree Dialogue tree to use.
     */
    void SetDialogueTree(const DialogueTree& tree) { m_DialogueTree = tree; }

    /**
     * @brief Check if NPC uses branching dialogue tree.
     * @return True if dialogue tree has nodes.
     */
    bool HasDialogueTree() const { return !m_DialogueTree.nodes.empty(); }

    /**
     * @brief Reinitialize patrol route from current position.
     * 
     * Called when navigation mesh changes. Generates a new patrol
     * route starting from the NPC's current tile.
     * 
     * @param tilemap Tilemap for navigation queries.
     * @return `true` if valid route was created.
     */
    bool ReinitializePatrolRoute(const Tilemap *tilemap);

    /**
     * @brief Reset animation to idle frame.
     * 
     * Sets frame to 0 (standing pose). Used when entering dialogue
     * or when NPC should appear stationary.
     */
    void ResetAnimationToIdle();

    /// @name Public State (for editor/debug access)
    /// @{
    
    /**
     * @brief Sprite sheet texture.
     */
    Texture m_SpriteSheet;

    /**
     * @brief NPC type identifier (from filename).
     */
    std::string m_Type;

    /**
     * @brief NPC display name for dialogue.
     */
    std::string m_Name;

    /**
     * @brief Dialogue text for player interaction.
     */
    std::string m_Dialogue;

    /**
     * @brief Dialogue tree for branching dialogue (empty nodes = use simple dialogue).
     */
    DialogueTree m_DialogueTree;

    /**
     * @brief Feet position in world coordinates.
     */
    glm::vec2 m_Position{0.0f, 0.0f};

    /**
     * @brief Current visual Y offset from elevation (smoothed, positive = rendered higher).
     */
    float m_ElevationOffset{0.0f};

    /**
     * @brief Target elevation to smoothly transition to.
     */
    float m_TargetElevation{0.0f};

    /**
     * @brief Starting elevation for smoothstep interpolation.
     */
    float m_ElevationStart{0.0f};

    /**
     * @brief Progress 0->1 for smoothstep elevation transition.
     */
    float m_ElevationProgress{1.0f};

    /**
     * @brief Current tile column.
     */
    int m_TileX{0};

    /**
     * @brief Current tile row.
     */
    int m_TileY{0};

    /**
     * @brief Current facing direction.
     */
    NPCDirection m_Direction{NPCDirection::DOWN};

    /**
     * @brief Current animation frame (0-2).
     */
    int m_CurrentFrame{0};

    /**
     * @brief Animation time accumulator.
     */
    float m_AnimationTime{0.0f};

    /**
     * @brief Position in walk cycle [0-3].
     * 
     * Walk sequence: [1, 0, 2, 0]
     * - Index 0: Frame 1 (left step)
     * - Index 1: Frame 0 (idle)
     * - Index 2: Frame 2 (right step)
     * - Index 3: Frame 0 (idle)
     */
    int m_WalkSequenceIndex{0};

    /**
     * @brief Movement speed in pixels per second.
     * 
     * Default: 32 px/s = 2 tiles/s
     */
    float m_Speed{32.0f};

    /**
     * @brief Target tile X for current movement.
     */
    int m_TargetTileX{0};

    /**
     * @brief Target tile Y for current movement.
     */
    int m_TargetTileY{0};

    /**
     * @brief Wait timer between movements.
     */
    float m_WaitTimer{0.0f};

    /**
     * @brief Whether NPC is stopped by external factor.
     */
    bool m_IsStopped{false};

    /**
     * @brief Whether NPC has no valid patrol route.
     * 
     * If true, NPC stands in place and periodically changes direction.
     */
    bool m_StandingStill{false};

    /**
     * @brief Timer for look-around behavior.
     * 
     * When standing still, NPC changes direction every N seconds.
     */
    float m_LookAroundTimer{0.0f};

    /**
     * @brief Timer for random pause checks.
     * 
     * Periodically checked to decide if NPC should pause.
     */
    float m_RandomStandStillCheckTimer{0.0f};

    /**
     * @brief Duration of current random pause.
     */
    float m_RandomStandStillTimer{0.0f};

    /**
     * @brief Patrol route for autonomous movement.
     * 
     * Contains waypoints generated from navigation mesh.
     */
    PatrolRoute m_PatrolRoute;

    /// @}

    /**
     * @brief Calculate sprite sheet UV coordinates.
     * 
     * Maps animation state to sprite position:
     * @f[
     * u = frame \times 32
     * @f]
     * @f[
     * v = direction \times 32
     * @f]
     * 
     * @param frame Animation frame (0-2).
     * @param dir Facing direction.
     * @return Sprite top-left corner in pixels.
     */
    glm::vec2 GetSpriteCoords(int frame, NPCDirection dir) const;

private:
    /// @name Private Helper Methods
    /// @{

    /**
     * @brief Update look-around behavior while standing still.
     * @param deltaTime Time elapsed since last frame.
     */
    void UpdateLookAround(float deltaTime);

    /**
     * @brief Enter standing still mode with optional duration.
     * @param isRandom If true, will resume after duration expires.
     * @param duration Time to stand still (seconds).
     */
    void EnterStandingStillMode(bool isRandom, float duration = 0.0f);

    /**
     * @brief Update facing direction based on movement delta.
     * @param dx Horizontal movement (-1, 0, or 1).
     * @param dy Vertical movement (-1, 0, or 1).
     */
    void UpdateDirectionFromMovement(int dx, int dy);

    /**
     * @brief Check if moving to a position would collide with the player.
     * @param newPosition Proposed new position.
     * @param playerPos Player position (can be nullptr).
     * @return true if collision would occur.
     */
    bool CheckPlayerCollision(const glm::vec2& newPosition, const glm::vec2* playerPos) const;

    /// @}
};
