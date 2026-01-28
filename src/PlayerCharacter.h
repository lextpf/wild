#pragma once

#include "Texture.h"
#include "IRenderer.h"
#include <glm/glm.hpp>
#include <vector>
#include <map>
#include <string>

/**
 * @enum Direction
 * @brief Cardinal direction the player is facing.
 * @author Alex (https://github.com/lextpf)
 * 
 * Maps directly to sprite sheet row indices for animation lookup.
 * In Vulkan: row 0=Down, 1=Up, 2=Left, 3=Right.
 * In OpenGL: rows are inverted due to different texture coordinate systems.
 */
enum class Direction
{
    DOWN = 0,  ///< Facing down (towards camera, +Y direction)
    UP = 1,    ///< Facing up (away from camera, -Y direction)
    LEFT = 2,  ///< Facing left (-X direction)
    RIGHT = 3  ///< Facing right (+X direction)
};

/**
 * @enum AnimationType
 * @brief Animation state machine states.
 * @author Alex (https://github.com/lextpf)
 * 
 * Determines which sprite sheet to use and animation timing:
 * - IDLE: Standing still, uses walking sheet frame 0
 * - WALK: Walking animation at base speed
 * - RUN: Running animation at 60% of normal frame duration
 */
enum class AnimationType
{
    IDLE = 0,  ///< Standing still (single frame)
    WALK = 1,  ///< Walking animation (3-frame cycle)
    RUN = 2    ///< Running/sprinting animation (faster 3-frame cycle)
};

/**
 * @enum CharacterType
 * @brief Available player character sprite variants.
 * @author Alex (https://github.com/lextpf)
 * 
 * Each character type has its own set of sprite sheets:
 * - Walking sprite sheet (idle + walk animations)
 * - Running sprite sheet (sprint animation)
 * - Bicycle sprite sheet (cycling animation)
 */
enum class CharacterType
{
    BW1_MALE = 0,    ///< Black & White 1 Male protagonist
    BW1_FEMALE = 1,  ///< Black & White 1 Female protagonist
    BW2_MALE = 2,    ///< Black & White 2 Male protagonist
    BW2_FEMALE = 3,  ///< Black & White 2 Female protagonist
    CC_FEMALE = 4    ///< Crystal Clear Female character
};

/**
 * @class PlayerCharacter
 * @brief Player-controlled character with movement, animation, and collision.
 * @author Alex (https://github.com/lextpf)
 *
 * @par Position (Bottom-Center)
 * Position is the bottom-center of the sprite (where the feet touch the ground):
 * @code
 *     +--------+
 *     |        |
 *     | Sprite |  32x32 pixels
 *     |        |
 *     +---oo---+
 *         ^^
 *      Position (bottom-center)
 * @endcode
 *
 * @par Hitbox
 * 16x16 pixels, extending upward from the bottom-center:
 * @code
 *     +---+---+---+
 *     |   |   |   |
 *     +---+===+---+   === = hitbox (16x16)
 *     |   |===|   |
 *     +---+-o-+---+   o = position (bottom-center)
 * @endcode
 *
 * @par Movement
 * - Walking: 100 px/s (1.0x)
 * - Running: 150 px/s (1.5x)
 * - Bicycle: 200 px/s (2.0x)
 *
 * @par Collision
 * - Strict mode: Full 16x16 hitbox check
 * - Center mode: Center-point only, allows corner cutting
 * - Corner cutting: Automatic sliding around obstacles
 * - Lane snapping: Aligns to tile centers during cardinal movement
 *
 * @par Animation
 * Walk cycle: [1, 0, 2, 0] at 0.15s/frame (walk) or 0.09s/frame (run)
 */
class PlayerCharacter
{
public:
    /**
     * @brief Construct a new PlayerCharacter with default values.
     * 
     * Initializes the player at position (200, 150) facing down.
     * No sprite sheets are loaded; call LoadSpriteSheet() before rendering.
     */
    PlayerCharacter();

    /**
     * @brief Destructor (default).
     */
    ~PlayerCharacter();

    /**
     * @brief Load the walking/idle sprite sheet.
     * 
     * The sprite sheet should be a 4x4 grid of 32x32 pixel sprites:
     * @code
     * +----+----+----+
     * | D0 | D1 | D2 |  Row 0: Down
     * +----+----+----+
     * | U0 | U1 | U2 |  Row 1: Up
     * +----+----+----+
     * | L0 | L1 | L2 |  Row 2: Left
     * +----+----+----+
     * | R0 | R1 | R2 |  Row 3: Right
     * +----+----+----+
     * @endcode
     * 
     * @param path Path to the sprite sheet PNG file.
     * @return true if loaded successfully, false on error.
     */
    bool LoadSpriteSheet(const std::string &path);

    /**
     * @brief Load the running sprite sheet.
     * 
     * Same format as walking sprite sheet but with running poses.
     * Required for running/sprinting animations.
     * 
     * @param path Path to the running sprite sheet PNG file.
     * @return true if loaded successfully, false on error.
     */
    bool LoadRunningSpriteSheet(const std::string &path);

    /**
     * @brief Load the bicycle sprite sheet.
     * 
     * Same format as walking sprite sheet but with cycling poses.
     * Optional; bicycle mode falls back to running if not available.
     * 
     * @param path Path to the bicycle sprite sheet PNG file.
     * @return true if loaded successfully, false on error.
     */
    bool LoadBicycleSpriteSheet(const std::string &path);

    /**
     * @brief Upload all sprite textures to the renderer.
     * 
     * Called when switching renderers to ensure textures are properly
     * recreated in the new graphics context.
     * 
     * @param renderer Reference to the active renderer.
     */
    void UploadTextures(IRenderer &renderer);

    /**
     * @brief Update player animation state.
     * 
     * Advances the animation timer and updates the current frame.
     * Should be called once per frame, typically from Game::Update().
     * 
     * @par Animation Timing
     * Frame duration depends on animation type:
     * @f[
     * t_{frame} = \begin{cases}
     *   0.15s & \text{if walking} \\
     *   0.15s \times 0.6 = 0.09s & \text{if running}
     * \end{cases}
     * @f]
     * 
     * @param deltaTime Time elapsed since last update in seconds.
     */
    void Update(float deltaTime);

    /**
     * @brief Render the player sprite at current position.
     * 
     * Draws the appropriate sprite frame based on current animation state.
     * Position is converted from world space to screen space using camera.
     * 
     * @par Screen Position Calculation
     * @f[
     * \vec{p}_{screen} = \vec{p}_{feet} - \vec{p}_{camera} - (\frac{w}{2}, h)
     * @f]
     * 
     * Where @f$ w = 32 @f$ and @f$ h = 32 @f$ are sprite dimensions.
     * The offset centers the sprite horizontally and places feet at position.
     * 
     * @param renderer Reference to the active renderer.
     * @param cameraPos Current camera position in world space.
     */
    void Render(IRenderer &renderer, glm::vec2 cameraPos);

    /**
     * @brief Render only the bottom half of the player sprite (feet area).
     *
     * Used for Y-sorted rendering where the player needs to be split into
     * two halves for proper depth sorting with tiles.
     *
     * @param renderer Reference to the active renderer.
     * @param cameraPos Current camera position in world space.
     */
    void RenderBottomHalf(IRenderer &renderer, glm::vec2 cameraPos);

    /**
     * @brief Render only the top half of the player sprite (head/torso area).
     *
     * Used for Y-sorted rendering where the player needs to be split into
     * two halves for proper depth sorting with tiles.
     *
     * @param renderer Reference to the active renderer.
     * @param cameraPos Current camera position in world space.
     */
    void RenderTopHalf(IRenderer &renderer, glm::vec2 cameraPos);

    /**
     * @brief Process movement input and update position.
     * 
     * This is the main movement function that handles:
     * 1. Direction normalization (prevents fast diagonals)
     * 2. Facing direction update
     * 3. Collision detection with tiles and NPCs
     * 4. Wall sliding and corner cutting
     * 5. Lane snapping for cardinal movement
     * 
     * @par Movement Algorithm
     * @code
     * 1. If no input: HandleIdleSnap() and return
     * 2. Normalize input direction
     * 3. Update facing direction based on dominant axis
     * 4. Calculate desired movement: v = dir * speed * dt
     * 5. Test for collisions at new position
     * 6. If blocked: try wall sliding
     * 7. Apply lane snapping for cardinal movement
     * 8. Final collision check and axis separation
     * 9. Apply movement to position
     * @endcode
     * 
     * @par Collision Response
     * When movement is blocked, the system tries several strategies:
     * - **Wall sliding**: Move along the perpendicular axis
     * - **Corner cutting**: Slide around corners using GetCornerSlideDirection()
     * - **Axis separation**: Move on X or Y only if one axis is clear
     * 
     * @param direction Input direction vector (should be normalized or zero).
     * @param deltaTime Time elapsed since last frame in seconds.
     * @param tilemap Optional tilemap for collision detection.
     * @param npcPositions Optional list of NPC feet positions for collision.
     */
    void Move(glm::vec2 direction, float deltaTime, const class Tilemap *tilemap = nullptr, const std::vector<glm::vec2> *npcPositions = nullptr);

    /**
     * @brief Immediately stop movement and reset to idle animation.
     * 
     * Called when the player releases movement keys or enters dialogue.
     * Resets animation state machine to IDLE.
     */
    void Stop();

    /**
     * @brief Enable or disable running mode.
     * 
     * When running is enabled, movement speed is multiplied by 1.5x
     * and the running sprite sheet is used for animation.
     * 
     * @param running true to enable running, false for walking.
     */
    void SetRunning(bool running);

    /**
     * @brief Enable or disable bicycle mode.
     * 
     * When bicycling is enabled, movement speed is multiplied by 2.0x
     * and the bicycle sprite sheet is used for animation.
     * Bicycle mode takes precedence over running mode.
     * 
     * @param bicycling true to enable bicycle, false to disable.
     */
    void SetBicycling(bool bicycling);

    /**
     * @brief Check if player is currently on bicycle.
     * @return true if bicycle mode is active.
     */
    bool IsBicycling() const { return m_IsBicycling; }

    /**
     * @brief Check if player is currently moving.
     * 
     * Used to determine which animation to play and whether to
     * apply idle snapping.
     * 
     * @return true if player is in motion, false if idle.
     */
    bool IsMoving() const { return m_IsMoving; }

    /**
     * @brief Switch to a different character appearance.
     * 
     * Loads all sprite sheets (walking, running, bicycle) for the
     * specified character type. Uses custom asset paths if set via
     * SetCharacterAsset(), otherwise uses default naming convention.
     * 
     * @par Asset Resolution
     * @code
     * 1. Check s_CharacterAssets for custom path
     * 2. If not found: use "assets/player/{BaseName}_{Type}.png"
     * 3. Try loading from current directory
     * 4. If fails: try parent directory ("../assets/...")
     * @endcode
     * 
     * @param characterType The character type to switch to.
     * @return true if all required sprites loaded successfully.
     */
    bool SwitchCharacter(CharacterType characterType);

    /**
     * @brief Get the current character type.
     * @return The active CharacterType.
     */
    CharacterType GetCharacterType() const { return m_CharacterType; }

    /**
     * @brief Copy appearance from an NPC sprite sheet.
     *
     * Loads the NPC's sprite sheet as the player's walking, running,
     * and bicycle sprites, transforming the player's appearance.
     *
     * @param spritePath Path to the NPC sprite sheet.
     * @return true if sprite loaded successfully.
     */
    bool CopyAppearanceFrom(const std::string &spritePath);

    /**
     * @brief Restore original character appearance.
     *
     * Resets the player to their original character type sprites.
     */
    void RestoreOriginalAppearance();

    /**
     * @brief Check if player is using a copied NPC appearance.
     * @return true if player has transformed into an NPC appearance.
     */
    bool IsUsingCopiedAppearance() const { return m_IsUsingCopiedAppearance; }

    /**
     * @brief Get player position (feet position).
     * 
     * Returns the bottom-center point of the player sprite,
     * representing where the character's feet are on the ground.
     * 
     * @return Position vector in world coordinates.
     */
    inline glm::vec2 GetPosition() const { return m_Position; }

    /**
     * @brief Set target elevation offset for stairs/ramps.
     *
     * The visual elevation will smoothly interpolate toward this target
     * for a graceful lift animation.
     *
     * @param offset Target Y offset in pixels (positive = rendered higher).
     */
    inline void SetElevationOffset(float offset)
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
     * @return Y offset in pixels (the smoothed value, not target).
     */
    inline float GetElevationOffset() const { return m_ElevationOffset; }

    /**
     * @brief Get target elevation offset.
     * @return Target Y offset in pixels.
     */
    inline float GetTargetElevation() const { return m_TargetElevation; }

    /**
     * @brief Set player position with tile snapping.
     * 
     * Converts the input position to tile coordinates and snaps
     * the player feet to the bottom-center of that tile.
     * 
     * @par Tile Snapping Formula
     * @f[
     * tile_x = \lfloor \frac{pos_x}{16} \rfloor, \quad
     * tile_y = \lfloor \frac{pos_y}{16} \rfloor
     * @f]
     * @f[
     * feet_x = tile_x \times 16 + 8, \quad
     * feet_y = tile_y \times 16 + 16
     * @f]
     * 
     * @param pos Desired position (will be snapped to tile center).
     */
    inline void SetPosition(glm::vec2 pos)
    {
        int tileX = static_cast<int>(std::floor(pos.x / 16.0f));
        int tileY = static_cast<int>(std::floor(pos.y / 16.0f));
        m_Position.x = tileX * 16.0f + 8.0f;  // horizontal center of tile
        m_Position.y = tileY * 16.0f + 16.0f; // bottom of tile (feet position)
    }

    /**
     * @brief Set player position directly by tile coordinates.
     * 
     * Places the player feet at the canonical position for the given tile:
     * horizontally centered, vertically at the tile's bottom edge.
     * 
     * @par Position Calculation
     * @f[
     * feet_x = tileX \times 16 + 8
     * @f]
     * @f[
     * feet_y = tileY \times 16 + 16
     * @f]
     * 
     * @param tileX Tile column (0-based).
     * @param tileY Tile row (0-based).
     */
    inline void SetTilePosition(int tileX, int tileY)
    {
        m_Position.x = tileX * 16.0f + 8.0f;  // horizontal center of tile
        m_Position.y = tileY * 16.0f + 16.0f; // bottom of tile (feet position)
    }

    /**
     * @brief Get current facing direction.
     * @return Current Direction value.
     */
    inline Direction GetDirection() const { return m_Direction; }

    /**
     * @brief Set facing direction.
     * 
     * Changes which row of the sprite sheet is used for rendering.
     * Does not affect movement.
     * 
     * @param dir Direction to face.
     */
    inline void SetDirection(Direction dir) { m_Direction = dir; }

    /**
     * @brief Get the canonical feet position for the current tile.
     * 
     * Calculates the tile the player is currently on and returns
     * the ideal feet position (bottom-center of that tile).
     * Used for idle snapping and lane correction.
     * 
     * @par Tile Determination
     * Uses the hitbox center (not feet) to determine tile ownership:
     * @f[
     * tile_x = \lfloor \frac{feet_x}{tileSize} \rfloor
     * @f]
     * @f[
     * tile_y = \lfloor \frac{feet_y - \frac{tileSize}{2} - \epsilon}{tileSize} \rfloor
     * @f]
     * 
     * @param tileSize Size of tiles in pixels (default: 16).
     * @return Canonical feet position for the current tile.
     */
    glm::vec2 GetCurrentTileCenter(float tileSize = 16.0f) const;

    /**
     * @brief Register a custom asset path for a character sprite.
     * 
     * Allows using obfuscated or non-standard asset names (e.g., UUIDs)
     * instead of the default naming convention.
     * 
     * @par Example
     * @code
     * PlayerCharacter::SetCharacterAsset(
     *     CharacterType::BW1_MALE,
     *     "Walking",
     *     "assets/player/962b0852-691e-416c-ad9b-78c0c0257a07.png"
     * );
     * @endcode
     * 
     * @param characterType The character type this asset belongs to.
     * @param spriteType One of: "Walking", "Running", or "Bicycle".
     * @param path Full path to the asset file.
     */
    static void SetCharacterAsset(CharacterType characterType, const std::string &spriteType, const std::string &path);

    /**
     * @name Render Constants
     * @brief Constants defining sprite rendering dimensions.
     * @{
     */
    static const int RENDER_WIDTH = 16;   ///< Sprite width in pixels (1 tile wide)
    static const int RENDER_HEIGHT = 32;  ///< Sprite height in pixels (2 tiles tall)
    /** @} */

    /**
     * @name Collision Constants
     * @brief Constants defining the collision hitbox dimensions.
     * 
     * The hitbox is a 16x16 pixel square, matching exactly one tile.
     * This provides intuitive tile-based collision while allowing
     * the larger sprite to overlap neighboring tiles visually.
     * @{
     */
    inline static const float HITBOX_WIDTH = 16.0f;   ///< Collision box width (1 tile)
    inline static const float HITBOX_HEIGHT = 16.0f;  ///< Collision box height (1 tile)

    inline static const float HALF_HITBOX_WIDTH = HITBOX_WIDTH / 2;    ///< Half of the collision box width
    inline static const float HALF_HITBOX_HEIGHT = HITBOX_HEIGHT / 2;  ///< Half of the collision box height
    /** @} */

private:
    /**
     * @name Sprite Sheet Textures
     * @brief Loaded sprite sheet textures for each animation type.
     * @{
     */
    Texture m_SpriteSheet;         ///< Walking/idle sprite sheet
    Texture m_RunningSpriteSheet;  ///< Running sprite sheet
    Texture m_BicycleSpriteSheet;  ///< Bicycle sprite sheet
    /** @} */

    /**
     * @name Movement State
     * @brief Variables tracking movement mode and position.
     * @{
     */
    bool m_IsRunning;                   ///< Running mode flag (1.5x speed)
    bool m_IsBicycling;                 ///< Bicycle mode flag (2.0x speed)
    bool m_IsUsingCopiedAppearance;     ///< True if using copied NPC appearance
    glm::vec2 m_Position;               ///< Current feet position in world space
    float m_ElevationOffset;            ///< Current visual Y offset (smoothed, positive = up)
    float m_TargetElevation;            ///< Target elevation to smoothly transition to
    float m_ElevationStart;             ///< Starting elevation for smoothstep interpolation
    float m_ElevationProgress;          ///< Progress 0->1 for smoothstep elevation transition
    glm::vec2 m_LastSafeTileCenter;     ///< Last valid tile center (for stuck recovery)
    glm::vec2 m_LastMovementDirection;  ///< Previous frame's movement direction
    glm::vec2 m_SlideHysteresisDir;     ///< Last chosen slide dir to avoid jitter
    float m_SlideCommitTimer;           ///< Time remaining before slide direction can change
    int m_AxisPreference;               ///< -1 = prefer Y, +1 = prefer X, 0 = no preference
    float m_AxisCommitTimer;            ///< Time remaining before axis preference can change
    glm::vec2 m_SnapStartPos;           ///< Position when idle snap began
    glm::vec2 m_SnapTargetPos;          ///< Target position for idle snap
    float m_SnapProgress;               ///< Progress 0->1 for smoothstep snap
    float m_Speed;                      ///< Base movement speed (100 px/s)
    bool m_IsMoving;                    ///< True if currently in motion
    int m_LastInputX = 0;               ///< Last non-zero A/D sign: -1 left, +1 right
    int m_LastInputY = 0;               ///< Last non-zero W/S sign: -1 up, +1 down
    /** @} */

    /**
     * @name Animation State
     * @brief Variables controlling sprite animation.
     * @{
     */
    Direction m_Direction;          ///< Current facing direction (sprite row)
    AnimationType m_AnimationType;  ///< Current animation type (sprite sheet)
    CharacterType m_CharacterType;  ///< Current character appearance
    float m_AnimationTime;          ///< Accumulated time for frame timing
    int m_CurrentFrame;             ///< Current sprite frame index (0-2)
    int m_WalkSequenceIndex;        ///< Position in walk cycle [1,0,2,0]
    /** @} */

    /**
     * @name Sprite Sheet Layout Constants
     * @brief Constants defining the sprite sheet format.
     * @{
     */
    static const int SPRITE_WIDTH = 32;    ///< Individual sprite width
    static const int SPRITE_HEIGHT = 32;   ///< Individual sprite height
    static const int SPRITES_PER_ROW = 4;  ///< Sprites per row in sheet
    static const int IDLE_FRAMES = 3;      ///< Frames in idle animation
    static const int WALK_FRAMES = 3;      ///< Frames in walk animation
    static const float ANIMATION_SPEED;    ///< Base frame duration (0.15s)
    /** @} */

    /**
     * @brief Calculate sprite sheet UV coordinates for a frame.
     * 
     * Maps animation state to sprite sheet position.
     * 
     * @par Coordinate Calculation
     * @f[
     * u = (frame \mod 3) \times 32
     * @f]
     * @f[
     * v = direction \times 32
     * @f]
     * 
     * @param frame Animation frame index (0-2).
     * @param dir Facing direction (determines row).
     * @param anim Animation type (unused, for future expansion).
     * @param requiresYFlip True if renderer uses Y-flipped textures (OpenGL)
     * @return Top-left corner of sprite in texture coordinates (pixels).
     */
    glm::vec2 GetSpriteCoords(int frame, Direction dir, AnimationType anim, bool requiresYFlip = false);

    /**
     * @name Collision Detection Helpers
     * @brief Internal methods for collision detection and response.
     * 
     * These methods implement the collision detection system using
     * Axis-Aligned Bounding Boxes (AABB) against the tile grid.
     * @{
     */
    
    /**
     * @brief Check collision between player hitbox and any NPC.
     * 
     * Tests if the player's hitbox at the given position overlaps
     * with any NPC hitbox. Uses AABB intersection with small epsilon
     * to prevent floating-point edge cases.
     * 
     * @par AABB Intersection Test
     * @f[
     * overlap = (P_{min,x} < N_{max,x}) \land (P_{max,x} > N_{min,x}) \land
     *           (P_{min,y} < N_{max,y}) \land (P_{max,y} > N_{min,y})
     * @f]
     * 
     * @param feetPos Test position (player feet).
     * @param npcPositions Vector of NPC feet positions.
     * @return true if collision detected with any NPC.
     */
    bool CollidesWithNPC(const glm::vec2 &feetPos, const std::vector<glm::vec2> *npcPositions) const;
    
    /**
     * @brief Check tile collision using full hitbox (strict mode).
     * 
     * Tests all tiles overlapping the player's hitbox for collision.
     * Implements corner cutting by allowing small overlaps with diagonal tiles.
     * 
     * @par Corner Cutting Logic
     * For each overlapping tile:
     * 1. Calculate overlap area ratio: @f$ r = \frac{A_{overlap}}{A_{hitbox}} @f$
     * 2. Determine if tile is at a corner (offset on both axes)
     * 3. If corner tile and @f$ r < 0.20 @f$, allow if escape route exists
     * 4. Non-corner tiles block at any significant overlap
     * 
     * @param feetPos Test position (player feet).
     * @param tilemap Tilemap for collision queries.
     * @param moveDx Movement direction X (-1, 0, or 1).
     * @param moveDy Movement direction Y (-1, 0, or 1).
     * @param diagonalInput True if diagonal input is active.
     * @return true if collision detected.
     */
    bool CollidesWithTilesStrict(const glm::vec2 &feetPos, const class Tilemap *tilemap, 
                                  int moveDx, int moveDy, bool diagonalInput) const;
    
    /**
     * @brief Check tile collision using center point only (sprint mode).
     * 
     * Only tests the single tile containing the hitbox center.
     * Used for high-speed movement to allow tighter navigation.
     * 
     * @par Center Point Calculation
     * @f[
     * center = (feet_x, feet_y - \frac{hitbox_h}{2})
     * @f]
     * @f[
     * tile = (\lfloor \frac{center_x}{16} \rfloor, \lfloor \frac{center_y}{16} \rfloor)
     * @f]
     * 
     * @param feetPos Test position (player feet).
     * @param tilemap Tilemap for collision queries.
     * @return true if center tile has collision.
     */
    bool CollidesWithTilesCenter(const glm::vec2 &feetPos, const class Tilemap *tilemap) const;
    
    /**
     * @brief Combined collision check (tiles + NPCs) with mode awareness.
     * 
     * Dispatches to appropriate tile collision method based on sprint mode,
     * then adds NPC collision check.
     * 
     * @param feetPos Test position.
     * @param tilemap Tilemap for collision.
     * @param npcPositions NPC positions.
     * @param sprintMode If true, use center-point collision.
     * @param moveDx Movement direction X.
     * @param moveDy Movement direction Y.
     * @param diagonalInput Diagonal input flag.
     * @return true if any collision detected.
     */
    bool CollidesAt(const glm::vec2 &feetPos, const class Tilemap *tilemap,
                    const std::vector<glm::vec2> *npcPositions, bool sprintMode,
                    int moveDx = 0, int moveDy = 0, bool diagonalInput = false) const;

    /**
     * @brief Detect if the hitbox overlaps blocked tiles across multiple rows and columns (corner pocket).
     * @param feetPos Player feet position to test.
     * @param tilemap Tilemap for collision queries.
     * @return true if overlapping a perpendicular pair of collision tiles.
     */
    bool IsCornerPenetration(const glm::vec2 &feetPos, const class Tilemap *tilemap) const;

    /**
     * @brief Compute a minimal shove out of a sprint corner penetration using strict collision.
     * @param tilemap Tilemap for collision queries.
     * @param npcPositions NPC positions to avoid during the shove.
     * @param normalizedDir Current input direction (normalized).
     * @return Movement offset that exits the corner pocket, or zero if none found.
     */
    glm::vec2 ComputeSprintCornerEject(const class Tilemap *tilemap,
                                       const std::vector<glm::vec2> *npcPositions,
                                       glm::vec2 normalizedDir) const;

    /**
     * @brief Determine slide direction when blocked at a corner.
     * 
     * When movement is blocked, this method finds which direction to slide
     * to navigate around the obstacle. Returns a direction vector pointing
     * away from the blocking tiles.
     * 
     * @par Algorithm
     * 1. Find all blocking tiles overlapping the test position
     * 2. Calculate average position of blocking tiles
     * 3. Return direction from average block position to player
     * 
     * @param testPos Position where collision occurred.
     * @param tilemap Tilemap for collision queries.
     * @param moveDirX Attempted movement direction X.
     * @param moveDirY Attempted movement direction Y.
     * @return Normalized slide direction, or zero if no slide possible.
     */
    glm::vec2 GetCornerSlideDirection(const glm::vec2& testPos, const class Tilemap* tilemap, 
                                       int moveDirX, int moveDirY);
    
    /**
     * @brief Find the nearest non-colliding tile center.
     * 
     * Searches a 5x5 tile area around the player to find the closest
     * tile center that doesn't cause collision. Used for stuck recovery.
     * 
     * @par Search Algorithm
     * @code
     * for dy in [-2, 2]:
     *     for dx in [-2, 2]:
     *         tile = (baseTile.x + dx, baseTile.y + dy)
     *         if not colliding at tile center:
     *             if distance < bestDistance:
     *                 bestTile = tile
     * @endcode
     * 
     * @param tilemap Tilemap for collision queries.
     * @param npcPositions NPC positions for collision.
     * @return Feet position at the closest safe tile center.
     */
    glm::vec2 FindClosestSafeTileCenter(const class Tilemap *tilemap, 
                                        const std::vector<glm::vec2> *npcPositions) const;
    
    /**
     * @brief Calculate exponential smoothing alpha for frame-rate-independent interpolation.
     * 
     * Computes the interpolation factor for smooth position following.
     * The formula ensures consistent behavior regardless of frame rate.
     * 
     * @par Mathematical Derivation
     * For exponential decay with time constant @f$ \tau @f$:
     * @f[
     * x(t) = x_0 \cdot e^{-t/\tau}
     * @f]
     * 
     * We want to reach @f$ \epsilon @f$ of the target in @f$ t_s @f$ seconds:
     * @f[
     * \epsilon = e^{-t_s/\tau} \Rightarrow \tau = \frac{-t_s}{\ln(\epsilon)}
     * @f]
     * 
     * Per-frame alpha:
     * @f[
     * \alpha = 1 - e^{-\Delta t/\tau} = 1 - \epsilon^{\Delta t/t_s}
     * @f]
     * 
     * @param deltaTime Time step in seconds.
     * @param settleTime Time to reach 99% of target (seconds).
     * @param epsilon Remaining fraction at settleTime (default: 0.01).
     * @return Smoothing alpha in range [0, 1].
     */
    static float CalculateFollowAlpha(float deltaTime, float settleTime, float epsilon = 0.01f);
    
    /**
     * @brief Attempt wall sliding when direct movement is blocked.
     * 
     * Tries to find valid movement along walls when the desired direction
     * is blocked. Uses binary search to find maximum valid movement.
     * 
     * @par Sliding Algorithm
     * 1. Detect which direction to slide (GetCornerSlideDirection)
     * 2. Try increasing slide amounts (1-16 pixels)
     * 3. For each slide amount, test slide + forward movement
     * 4. If valid, use binary search to maximize forward component
     * 5. Return the best valid movement found
     * 
     * @param desiredMovement Original intended movement.
     * @param normalizedDir Normalized movement direction.
     * @param deltaTime Frame time.
     * @param currentSpeed Current movement speed.
     * @param tilemap Tilemap for collision.
     * @param npcPositions NPC positions.
     * @param sprintMode Sprint collision mode flag.
     * @param moveDx Direction X.
     * @param moveDy Direction Y.
     * @param diagonalInput Diagonal input flag.
     * @return Valid movement vector (may be zero if completely blocked).
     */
    glm::vec2 TrySlideMovement(glm::vec2 desiredMovement, glm::vec2 normalizedDir,
                                float deltaTime, float currentSpeed,
                                const class Tilemap *tilemap, const std::vector<glm::vec2> *npcPositions,
                                bool sprintMode, int moveDx, int moveDy, bool diagonalInput);
    
    /**
     * @brief Apply perpendicular lane snapping for cardinal movement.
     * 
     * When moving cardinally (along one axis), gently corrects the
     * perpendicular axis to align with tile centers. This creates
     * the classic "lane-based" movement feel.
     * 
     * @par Snapping Logic
     * If moving horizontally (|dir.x| > |dir.y|):
     * - Calculate Y offset to tile center
     * - Apply exponential smoothing with settleTime = 0.15s
     * 
     * If moving vertically:
     * - Same logic for X offset
     * 
     * @param desiredMovement Current movement vector.
     * @param normalizedDir Normalized movement direction.
     * @param deltaTime Frame time.
     * @param tilemap Tilemap for collision checks.
     * @param npcPositions NPC positions.
     * @param sprintMode True if sprinting (relaxed collision).
     * @param moveDx Horizontal input direction (-1, 0, or 1).
     * @param moveDy Vertical input direction (-1, 0, or 1).
     * @return Movement with lane correction applied.
     */
    glm::vec2 ApplyLaneSnapping(glm::vec2 desiredMovement, glm::vec2 normalizedDir,
        float deltaTime, const Tilemap *tilemap,
        const std::vector<glm::vec2> *npcPositions,
        bool sprintMode, int moveDx, int moveDy);
    
    /**
     * @brief Handle idle state snapping to tile center.
     * 
     * When not moving, smoothly snaps the player to the center of the
     * current tile. Also handles stuck recovery if player is inside a wall.
     * 
     * @par Snapping Behavior
     * 1. If currently in collision: teleport to FindClosestSafeTileCenter()
     * 2. Calculate target tile center
     * 3. Apply smoothstep easing: @f$ 3t^2 - 2t^3 @f$
     * 4. Move towards center, testing each axis independently
     * 5. If within 0.5px of center: snap exactly
     * 
     * @param deltaTime Frame time.
     * @param tilemap Tilemap for collision.
     * @param npcPositions NPC positions.
     */
    void HandleIdleSnap(float deltaTime, const class Tilemap *tilemap,
                        const std::vector<glm::vec2> *npcPositions);
    /** @} */

    /**
     * @brief Custom asset path registry.
     * 
     * Static map storing custom asset paths for character sprites.
     * Key: (CharacterType, spriteType string)
     * Value: Full asset path
     */
    static std::map<std::pair<CharacterType, std::string>, std::string> s_CharacterAssets;
};
