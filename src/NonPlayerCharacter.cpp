#include "NonPlayerCharacter.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
    // Width of each NPC sprite frame in pixels.
    constexpr int NPC_SPRITE_WIDTH = 32;

    // Height of each NPC sprite frame in pixels.
    constexpr int NPC_SPRITE_HEIGHT = 32;

    // Number of walking animation frames per direction.
    constexpr int NPC_WALK_FRAMES = 3;

    // Time between animation frame changes (seconds).
    constexpr float NPC_ANIM_SPEED = 0.15f;

    // NPC hitbox half-width for collision detection.
    constexpr float NPC_HALF_WIDTH = 8.0f;

    // NPC hitbox height for collision detection.
    constexpr float NPC_HITBOX_HEIGHT = 16.0f;

    // Small epsilon for collision boundary adjustments.
    constexpr float COLLISION_EPS = 0.05f;

    // Distance threshold for reaching a waypoint (pixels).
    constexpr float WAYPOINT_REACH_THRESHOLD = 0.5f;

    // Minimum movement distance to avoid division by zero.
    constexpr float MIN_MOVEMENT_DIST = 0.001f;
}

NonPlayerCharacter::NonPlayerCharacter()
    : m_Position(0.0f, 0.0f)                   // World position in pixels (bottom-center anchor)
    , m_TileX(0)                               // Current tile X coordinate
    , m_TileY(0)                               // Current tile Y coordinate
    , m_Direction(NPCDirection::DOWN)          // Facing direction (affects sprite row selection)
    , m_CurrentFrame(0)                        // Current animation frame index (0-2)
    , m_AnimationTime(0.0f)                    // Time accumulator for frame advancement
    , m_WalkSequenceIndex(0)                   // Index into walk cycle sequence (0,1,2,1,0,...)
    , m_Speed(25.0f)                           // Movement speed in pixels per second
    , m_TargetTileX(0)                         // Destination tile X for pathfinding
    , m_TargetTileY(0)                         // Destination tile Y for pathfinding
    , m_WaitTimer(0.0f)                        // Countdown before next movement decision
    , m_IsStopped(false)                       // Externally stopped (e.g., collision with player)
    , m_StandingStill(false)                   // Voluntary idle state (random behavior)
    , m_LookAroundTimer(0.0f)                  // Time until next random direction change while idle
    , m_RandomStandStillCheckTimer(0.0f)       // Time until next roll for entering idle state
    , m_RandomStandStillTimer(0.0f)            // Remaining time in current idle period
    , m_Dialogue("Hello! How are you today?")  // Default dialogue text (fallback)
{
}

bool NonPlayerCharacter::Load(const std::string &relativePath)
{
    // Extract NPC type from filename
    size_t lastSlash = relativePath.find_last_of('/');
    std::string filename = (lastSlash != std::string::npos)
                               ? relativePath.substr(lastSlash + 1)
                               : relativePath;

    // Remove .png extension if present
    if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".png")
    {
        m_Type = filename.substr(0, filename.size() - 4);
    }
    else
    {
        m_Type = filename;
    }

    // Try loading from given path
    std::string path = relativePath;
    if (!m_SpriteSheet.LoadFromFile(path))
    {
        // Fallback: try parent directory
        std::string altPath = "../" + path;
        if (!m_SpriteSheet.LoadFromFile(altPath))
        {
            std::cerr << "Failed to load NPC sprite sheet: "
                      << path << " or " << altPath << std::endl;
            return false;
        }
    }
    return true;
}

void NonPlayerCharacter::UploadTextures(IRenderer &renderer)
{
    renderer.UploadTexture(m_SpriteSheet);
}

void NonPlayerCharacter::SetTilePosition(int tileX, int tileY, int tileSize, bool preserveRoute)
{
    m_TileX = tileX;
    m_TileY = tileY;

    // Position at bottom-center of tile
    m_Position.x = tileX * tileSize + tileSize * 0.5f;
    m_Position.y = tileY * tileSize + static_cast<float>(tileSize);

    m_TargetTileX = tileX;
    m_TargetTileY = tileY;

    if (!preserveRoute)
    {
        m_PatrolRoute.Reset();
    }
}

glm::vec2 NonPlayerCharacter::GetSpriteCoords(int frame, NPCDirection dir) const
{
    int spriteX = (frame % NPC_WALK_FRAMES) * NPC_SPRITE_WIDTH;
    int spriteY = 0;

    switch (dir)
    {
        case NPCDirection::DOWN:
            spriteY = 2 * NPC_SPRITE_HEIGHT;
            break;
        case NPCDirection::UP:
            spriteY = 3 * NPC_SPRITE_HEIGHT;
            break;
        case NPCDirection::LEFT:
            spriteY = 1 * NPC_SPRITE_HEIGHT;
            break;
        case NPCDirection::RIGHT:
            spriteY = 0 * NPC_SPRITE_HEIGHT;
            break;
    }

    return glm::vec2(static_cast<float>(spriteX), static_cast<float>(spriteY));
}

void NonPlayerCharacter::Update(float deltaTime, const Tilemap *tilemap, const glm::vec2 *playerPosition)
{
    if (!tilemap)
        return;

    // Smooth elevation transition (must run regardless of movement state)
    if (m_ElevationProgress < 1.0f)
    {
        constexpr float transitionDuration = 0.15f;
        m_ElevationProgress += deltaTime / transitionDuration;

        if (m_ElevationProgress >= 1.0f)
        {
            m_ElevationProgress = 1.0f;
            m_ElevationOffset = m_TargetElevation;
        }
        else
        {
            float t = m_ElevationProgress;
            float smoothT = t * t * (3.0f - 2.0f * t);
            m_ElevationOffset = m_ElevationStart + (m_TargetElevation - m_ElevationStart) * smoothT;
        }
    }

    bool isCollidingWithPlayer = false;
    if (playerPosition)
    {
        // Build NPC hitbox (16x16, centered on feet)
        float npcMinX = m_Position.x - NPC_HALF_WIDTH + COLLISION_EPS;
        float npcMaxX = m_Position.x + NPC_HALF_WIDTH - COLLISION_EPS;
        float npcMaxY = m_Position.y - COLLISION_EPS;
        float npcMinY = m_Position.y - NPC_HITBOX_HEIGHT + COLLISION_EPS;

        // Build player hitbox (16x16, centered on feet)
        float playerMinX = playerPosition->x - NPC_HALF_WIDTH + COLLISION_EPS;
        float playerMaxX = playerPosition->x + NPC_HALF_WIDTH - COLLISION_EPS;
        float playerMaxY = playerPosition->y - COLLISION_EPS;
        float playerMinY = playerPosition->y - NPC_HITBOX_HEIGHT + COLLISION_EPS;

        // AABB intersection test
        if (npcMinX < playerMaxX && npcMaxX > playerMinX &&
            npcMinY < playerMaxY && npcMaxY > playerMinY)
        {
            isCollidingWithPlayer = true;
            m_WaitTimer = 0.5f;
        }
    }

    if (m_IsStopped || isCollidingWithPlayer)
    {
        m_CurrentFrame = 0;
        m_WalkSequenceIndex = 0;
        m_AnimationTime = 0.0f;
        return;
    }

    if (m_StandingStill)
    {
        m_CurrentFrame = 0;
        m_WalkSequenceIndex = 0;
        m_AnimationTime = 0.0f;

        // Random pause: Count down timer
        if (m_RandomStandStillTimer > 0.0f)
        {
            m_RandomStandStillTimer -= deltaTime;
            if (m_RandomStandStillTimer <= 0.0f)
            {
                m_StandingStill = false;
                m_RandomStandStillTimer = 0.0f;
            }
            else
            {
                // Look around while paused
                UpdateLookAround(deltaTime);
                return;
            }
        }
        else
        {
            // No path available: Look around indefinitely
            UpdateLookAround(deltaTime);
            return;
        }
    }

    const int tileSize = tilemap->GetTileWidth();
    m_TileX = static_cast<int>(std::floor(m_Position.x / tileSize));
    m_TileY = static_cast<int>(std::floor((m_Position.y - 0.1f) / tileSize));

    if (m_WaitTimer > 0.0f)
    {
        m_WaitTimer -= deltaTime;
        if (m_WaitTimer < 0.0f)
            m_WaitTimer = 0.0f;
    }

    m_AnimationTime += deltaTime;
    if (m_AnimationTime >= NPC_ANIM_SPEED)
    {
        m_AnimationTime -= NPC_ANIM_SPEED;

        // Walk sequence: step-idle-step-idle pattern
        static const int walkSequence[] = {1, 0, 2, 0};
        m_WalkSequenceIndex = (m_WalkSequenceIndex + 1) % 4;
        m_CurrentFrame = walkSequence[m_WalkSequenceIndex];
    }

    if (m_WaitTimer > 0.0f)
        return;

    if (m_PatrolRoute.IsValid() && m_RandomStandStillCheckTimer > 0.0f)
    {
        m_RandomStandStillCheckTimer -= deltaTime;
    }

    glm::vec2 targetPos(
        m_TargetTileX * tileSize + tileSize * 0.5f,
        m_TargetTileY * tileSize + static_cast<float>(tileSize));

    glm::vec2 toTarget = targetPos - m_Position;
    float dist = glm::length(toTarget);

    // Check if we've reached the current waypoint
    if (dist < WAYPOINT_REACH_THRESHOLD)
    {
        m_Position = targetPos;

        // Initialize patrol route if needed
        if (!m_PatrolRoute.IsValid())
        {
            if (!m_PatrolRoute.Initialize(m_TileX, m_TileY, tilemap, 100))
            {
                EnterStandingStillMode(false);
                return;
            }
            else
            {
                m_StandingStill = false;
                m_RandomStandStillTimer = 0.0f;
                m_RandomStandStillCheckTimer = 5.0f + (std::rand() % 500) / 100.0f;
            }
        }

        // Random pause check (30% chance when timer expires at waypoint)
        if (m_PatrolRoute.IsValid() && m_RandomStandStillCheckTimer <= 0.0f)
        {
            m_RandomStandStillCheckTimer = 5.0f + (std::rand() % 500) / 100.0f;
            if ((std::rand() % 100) < 30)
            {
                float duration = 2.0f + (std::rand() % 300) / 100.0f;
                EnterStandingStillMode(true, duration);
                return;
            }
        }

        // Get next waypoint
        int nextX, nextY;
        if (m_PatrolRoute.GetNextWaypoint(nextX, nextY))
        {
            m_TargetTileX = nextX;
            m_TargetTileY = nextY;
            UpdateDirectionFromMovement(m_TargetTileX - m_TileX, m_TargetTileY - m_TileY);
        }
        else
        {
            m_WaitTimer = 1.0f;
        }
        return;
    }

    if (dist > MIN_MOVEMENT_DIST)
    {
        glm::vec2 dir = toTarget / dist;
        glm::vec2 newPosition = m_Position + dir * m_Speed * deltaTime;

        bool wouldCollide = CheckPlayerCollision(newPosition, playerPosition);

        if (!wouldCollide)
        {
            m_Position = newPosition;
            UpdateDirectionFromMovement(
                static_cast<int>(dir.x > 0) - static_cast<int>(dir.x < 0),
                static_cast<int>(dir.y > 0) - static_cast<int>(dir.y < 0));
        }
        else
        {
            m_WaitTimer = 0.5f;
        }
    }
}

void NonPlayerCharacter::UpdateLookAround(float deltaTime)
{
    m_LookAroundTimer -= deltaTime;
    if (m_LookAroundTimer <= 0.0f)
    {
        static const NPCDirection directions[] = {
            NPCDirection::LEFT, NPCDirection::RIGHT,
            NPCDirection::UP, NPCDirection::DOWN};
        m_Direction = directions[static_cast<size_t>(std::rand()) % 4];
        m_LookAroundTimer = 2.0f;
    }
}

void NonPlayerCharacter::EnterStandingStillMode(bool isRandom, float duration)
{
    m_StandingStill = true;
    m_RandomStandStillTimer = isRandom ? duration : 0.0f;
    m_LookAroundTimer = 2.0f;
    m_CurrentFrame = 0;
    m_WalkSequenceIndex = 0;
    m_AnimationTime = 0.0f;

    static const NPCDirection directions[] = {
        NPCDirection::LEFT, NPCDirection::RIGHT,
        NPCDirection::UP, NPCDirection::DOWN};
    m_Direction = directions[static_cast<size_t>(std::rand()) % 4];
}

void NonPlayerCharacter::UpdateDirectionFromMovement(int dx, int dy)
{
    if (std::abs(dx) > std::abs(dy))
    {
        m_Direction = (dx > 0) ? NPCDirection::RIGHT : NPCDirection::LEFT;
    }
    else if (dy != 0)
    {
        m_Direction = (dy > 0) ? NPCDirection::DOWN : NPCDirection::UP;
    }
}

bool NonPlayerCharacter::CheckPlayerCollision(const glm::vec2 &newPosition, const glm::vec2 *playerPos) const
{
    if (!playerPos)
        return false;

    float npcMinX = newPosition.x - NPC_HALF_WIDTH + COLLISION_EPS;
    float npcMaxX = newPosition.x + NPC_HALF_WIDTH - COLLISION_EPS;
    float npcMaxY = newPosition.y - COLLISION_EPS;
    float npcMinY = newPosition.y - NPC_HITBOX_HEIGHT + COLLISION_EPS;

    float playerMinX = playerPos->x - NPC_HALF_WIDTH + COLLISION_EPS;
    float playerMaxX = playerPos->x + NPC_HALF_WIDTH - COLLISION_EPS;
    float playerMaxY = playerPos->y - COLLISION_EPS;
    float playerMinY = playerPos->y - NPC_HITBOX_HEIGHT + COLLISION_EPS;

    return npcMinX < playerMaxX && npcMaxX > playerMinX &&
           npcMinY < playerMaxY && npcMaxY > playerMinY;
}

bool NonPlayerCharacter::ReinitializePatrolRoute(const Tilemap *tilemap)
{
    if (!tilemap)
        return false;

    m_PatrolRoute.Reset();
    bool success = m_PatrolRoute.Initialize(m_TileX, m_TileY, tilemap, 100);

    if (success)
    {
        m_StandingStill = false;
        m_RandomStandStillTimer = 0.0f;
        m_RandomStandStillCheckTimer = 5.0f + (std::rand() % 500) / 100.0f;
    }
    else
    {
        m_StandingStill = true;
        m_RandomStandStillTimer = 0.0f;
        m_LookAroundTimer = 2.0f;
    }

    return success;
}

void NonPlayerCharacter::ResetAnimationToIdle()
{
    m_CurrentFrame = 0;
    m_WalkSequenceIndex = 0;
    m_AnimationTime = 0.0f;
}

void NonPlayerCharacter::Render(IRenderer &renderer, glm::vec2 cameraPos) const
{
    constexpr float spriteWidth = static_cast<float>(NPC_SPRITE_WIDTH);
    constexpr float spriteHeight = static_cast<float>(NPC_SPRITE_HEIGHT);

    // Convert world position to screen space
    glm::vec2 bottomCenter = m_Position - cameraPos;
    bottomCenter = renderer.ProjectPoint(bottomCenter);

    // Position sprite with feet at projected point
    glm::vec2 renderPos = bottomCenter - glm::vec2(spriteWidth / 2.0f, spriteHeight);
    glm::vec2 spriteCoords = GetSpriteCoords(m_CurrentFrame, m_Direction);

    renderer.DrawSpriteRegion(
        m_SpriteSheet,
        renderPos,
        glm::vec2(spriteWidth, spriteHeight),
        spriteCoords,
        glm::vec2(spriteWidth, spriteHeight),
        0.0f,
        glm::vec3(1.0f),
        false);
}

void NonPlayerCharacter::RenderBottomHalf(IRenderer &renderer, glm::vec2 cameraPos) const
{
    constexpr float spriteWidth = static_cast<float>(NPC_SPRITE_WIDTH);
    constexpr float spriteHeight = static_cast<float>(NPC_SPRITE_HEIGHT);
    constexpr float halfHeight = 16.0f;

    // Apply elevation before projection
    glm::vec2 bottomCenter = m_Position - cameraPos;
    bottomCenter.y -= m_ElevationOffset;
    bottomCenter = renderer.ProjectPoint(bottomCenter);

    glm::vec2 renderPos = bottomCenter - glm::vec2(spriteWidth / 2.0f, spriteHeight);
    glm::vec2 spriteCoords = GetSpriteCoords(m_CurrentFrame, m_Direction);

    // Draw lower 16 pixels (feet area)
    renderer.SuspendPerspective(true);
    renderer.DrawSpriteRegion(
        m_SpriteSheet,
        renderPos + glm::vec2(0.0f, halfHeight),
        glm::vec2(spriteWidth, halfHeight),
        spriteCoords,
        glm::vec2(spriteWidth, halfHeight),
        0.0f,
        glm::vec3(1.0f),
        false);
    renderer.SuspendPerspective(false);
}

void NonPlayerCharacter::RenderTopHalf(IRenderer &renderer, glm::vec2 cameraPos) const
{
    constexpr float spriteWidth = static_cast<float>(NPC_SPRITE_WIDTH);
    constexpr float spriteHeight = static_cast<float>(NPC_SPRITE_HEIGHT);
    constexpr float halfHeight = 16.0f;

    // Apply elevation before projection
    glm::vec2 bottomCenter = m_Position - cameraPos;
    bottomCenter.y -= m_ElevationOffset;
    bottomCenter = renderer.ProjectPoint(bottomCenter);

    glm::vec2 renderPos = bottomCenter - glm::vec2(spriteWidth / 2.0f, spriteHeight);
    glm::vec2 spriteCoords = GetSpriteCoords(m_CurrentFrame, m_Direction);

    // Draw upper 16 pixels (head/torso area)
    glm::vec2 topHalfCoords = spriteCoords + glm::vec2(0.0f, halfHeight);

    renderer.SuspendPerspective(true);
    renderer.DrawSpriteRegion(
        m_SpriteSheet,
        renderPos,
        glm::vec2(spriteWidth, halfHeight),
        topHalfCoords,
        glm::vec2(spriteWidth, halfHeight),
        0.0f,
        glm::vec3(1.0f),
        false);
    renderer.SuspendPerspective(false);
}
