#include "Game.h"

#include <algorithm>
#include <cmath>

void Game::RenderCollisionOverlays()
{
    // Render red transparent overlays on tiles with collision
    // Use the same calculation as Tilemap::Render to ensure perfect alignment
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    // Calculate visible tile range
    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    // Render red overlay for each collision tile
    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            if (m_Tilemap.GetTileCollision(x, y))
            {
                // Calculate tile position in screen space
                // Tilemap::Render uses: tilePos(x * m_TileWidth - cameraPos.x, y * m_TileHeight - cameraPos.y)
                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x, y * tileHeight - m_CameraPosition.y);

                // DrawColoredRect uses bottom-left origin like DrawSpriteRegion
                m_Renderer->DrawColoredRect(tilePos, glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                                            glm::vec4(1.0f, 0.0f, 0.0f, 0.5f));
            }
        }
    }

    // Render player hitbox
    glm::vec2 playerPos = m_Player.GetPosition();

    // Calculate hitbox position from bottom-center
    // Box is [feet.x - w/2, feet.y - h] -> [feet.x + w/2, feet.y]
    glm::vec2 playerHitboxPos(playerPos.x - PlayerCharacter::HITBOX_WIDTH * 0.5f - m_CameraPosition.x,
                              playerPos.y - PlayerCharacter::HITBOX_HEIGHT - m_CameraPosition.y);
    glm::vec2 playerHitboxSize(PlayerCharacter::HITBOX_WIDTH, PlayerCharacter::HITBOX_HEIGHT);

    // Only render if player hitbox is visible on screen
    if (playerHitboxPos.x + playerHitboxSize.x >= 0 && playerHitboxPos.x <= screenSize.x &&
        playerHitboxPos.y + playerHitboxSize.y >= 0 && playerHitboxPos.y <= screenSize.y)
    {
        // Render player hitbox with yellow overlay to distinguish from collision tiles
        m_Renderer->DrawColoredRect(playerHitboxPos, playerHitboxSize,
                                    glm::vec4(1.0f, 1.0f, 0.0f, 0.6f));
    }

    // Render NPC hitboxes in editor mode
    const float NPC_HITBOX_SIZE = PlayerCharacter::HITBOX_HEIGHT;
    for (const auto &npc : m_NPCs)
    {
        glm::vec2 npcFeet = npc.GetPosition();
        glm::vec2 npcHitboxPos(npcFeet.x - NPC_HITBOX_SIZE * 0.5f - m_CameraPosition.x,
                               npcFeet.y - NPC_HITBOX_SIZE - m_CameraPosition.y);
        glm::vec2 npcHitboxSize(NPC_HITBOX_SIZE, NPC_HITBOX_SIZE);

        if (npcHitboxPos.x + npcHitboxSize.x >= 0 && npcHitboxPos.x <= screenSize.x &&
            npcHitboxPos.y + npcHitboxSize.y >= 0 && npcHitboxPos.y <= screenSize.y)
        {
            // Render NPC hitbox with magenta overlay
            m_Renderer->DrawColoredRect(
                npcHitboxPos,
                npcHitboxSize,
                glm::vec4(1.0f, 0.0f, 1.0f, 0.6f));
        }
    }
}

void Game::RenderNavigationOverlays()
{
    // Use same world size as main render (with zoom applied)
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            if (!m_Tilemap.GetNavigation(x, y))
                continue;

            glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x,
                              y * tileHeight - m_CameraPosition.y);

            // Cyan overlay for navigation tiles
            m_Renderer->DrawColoredRect(
                tilePos,
                glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                glm::vec4(0.0f, 1.0f, 1.0f, 0.3f));
        }
    }
}

void Game::RenderElevationOverlays()
{
    // Use same world size as main render
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    // Check if 3D mode is enabled
    bool perspectiveEnabled = m_Renderer->GetPerspectiveState().enabled;

    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            int elevation = m_Tilemap.GetElevation(x, y);
            if (elevation <= 0)
                continue;

            glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x,
                              y * tileHeight - m_CameraPosition.y);

            // Alpha increases with elevation .15f up to max .5f
            float alpha = std::min(0.5f, static_cast<float>(elevation) / 32.0f * 0.5f + 0.15f);

            // Purple overlay for elevation tiles
            m_Renderer->DrawColoredRect(
                tilePos,
                glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                glm::vec4(0.8f, 0.2f, 0.8f, alpha));

            // Draw elevation number centered in tile
            if (!perspectiveEnabled)
            {
                std::string elevText = std::to_string(elevation);
                float textScale = 0.2f; // Smaller text
                // Approximate text width for centering
                float textWidth = elevText.length() * 8.0f * textScale;
                float textX = tilePos.x + (tileWidth - textWidth) * 0.5f;
                float textY = tilePos.y + tileHeight * 0.6f;
                m_Renderer->DrawText(elevText, glm::vec2(textX, textY), textScale,
                                     glm::vec3(1.0f, 1.0f, 0.2f), 0.0f, 0.15f);
            }
        }
    }
}

void Game::RenderNoProjectionOverlays()
{
    // Use same world size as main render (with zoom applied)
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();
    int mapWidth = m_Tilemap.GetMapWidth();
    int mapHeight = m_Tilemap.GetMapHeight();

    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(mapWidth, (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(mapHeight, (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    // Track processed tiles for anchor finding
    // TODO: cache structure bounds/anchors once per frame (shared with RenderNoProjectionAnchors) to avoid repeated flood-fills.
    std::vector<bool> processed(mapWidth * mapHeight, false);
    size_t layerCount = m_Tilemap.GetLayerCount();

    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            // In no-projection edit mode, only show flags for current layer
            if (m_NoProjectionEditMode)
            {
                if (!m_Tilemap.GetLayerNoProjection(x, y, m_CurrentLayer))
                    continue;

                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x,
                                  y * tileHeight - m_CameraPosition.y);

                // Orange overlay for no-projection tiles
                m_Renderer->DrawColoredRect(
                    tilePos,
                    glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                    glm::vec4(1.0f, 0.6f, 0.0f, 0.5f));
            }
            else
            {
                // Check tile for all layers
                int count = 0;
                for (size_t layer = 0; layer < layerCount; ++layer)
                {
                    if (m_Tilemap.GetLayerNoProjection(x, y, layer))
                        count++;
                }

                if (count == 0)
                    continue;

                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x,
                                  y * tileHeight - m_CameraPosition.y);

                // Alpha based on number of layers with flag
                float alpha = 0.15f + (static_cast<float>(count) / static_cast<float>(layerCount)) * 0.35f;

                // Orange overlay for no-projection tiles
                m_Renderer->DrawColoredRect(
                    tilePos,
                    glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                    glm::vec4(1.0f, 0.6f, 0.0f, alpha));

                // Find and draw anchor for this structure (if not already processed)
                int idx = y * mapWidth + x;
                if (!processed[idx])
                {
                    // Flood-fill to find structure bounding box
                    int minX = x, maxX = x, minY = y, maxY = y;
                    std::vector<std::pair<int, int>> stack;
                    stack.push_back({x, y});

                    while (!stack.empty())
                    {
                        auto [cx, cy] = stack.back();
                        stack.pop_back();

                        if (cx < 0 || cx >= mapWidth || cy < 0 || cy >= mapHeight)
                            continue;

                        int cIdx = cy * mapWidth + cx;
                        if (processed[cIdx])
                            continue;

                        // Check if this tile is no-projection in any layer
                        bool isNoProj = false;
                        for (size_t li = 0; li < layerCount; ++li)
                        {
                            if (m_Tilemap.GetLayerNoProjection(cx, cy, li))
                            {
                                isNoProj = true;
                                break;
                            }
                        }
                        if (!isNoProj)
                            continue;

                        processed[cIdx] = true;
                        minX = std::min(minX, cx);
                        maxX = std::max(maxX, cx);
                        minY = std::min(minY, cy);
                        maxY = std::max(maxY, cy);

                        // 4-way connectivity
                        stack.push_back({cx - 1, cy});
                        stack.push_back({cx + 1, cy});
                        stack.push_back({cx, cy - 1});
                        stack.push_back({cx, cy + 1});
                    }

                    // Calculate anchor positions in world pixels
                    int leftPixelX = minX * tileWidth;
                    int rightPixelX = (maxX + 1) * tileWidth;
                    int bottomPixelY = (maxY + 1) * tileHeight;

                    // Check if 3D perspective is enabled
                    auto perspState = m_Renderer->GetPerspectiveState();
                    bool is3DMode = perspState.enabled;

                    // Skip drawing in 3D mode - RenderNoProjectionAnchors handles that
                    if (is3DMode)
                        continue;

                    // 2D mode: simple screen-space coordinates
                    glm::vec2 anchorLeft(static_cast<float>(leftPixelX) - m_CameraPosition.x,
                                         static_cast<float>(bottomPixelY) - m_CameraPosition.y);
                    glm::vec2 anchorRight(static_cast<float>(rightPixelX) - m_CameraPosition.x,
                                          static_cast<float>(bottomPixelY) - m_CameraPosition.y);

                    // Draw anchor markers (green crosses)
                    float markerSize = 6.0f;
                    glm::vec4 anchorColor(0.0f, 1.0f, 0.0f, 1.0f);

                    // Bottom-left anchor
                    m_Renderer->DrawColoredRect(
                        glm::vec2(anchorLeft.x - markerSize, anchorLeft.y - 1.0f),
                        glm::vec2(markerSize * 2.0f, 2.0f),
                        anchorColor);
                    m_Renderer->DrawColoredRect(
                        glm::vec2(anchorLeft.x - 1.0f, anchorLeft.y - markerSize),
                        glm::vec2(2.0f, markerSize * 2.0f),
                        anchorColor);

                    // Bottom-right anchor
                    m_Renderer->DrawColoredRect(
                        glm::vec2(anchorRight.x - markerSize, anchorRight.y - 1.0f),
                        glm::vec2(markerSize * 2.0f, 2.0f),
                        anchorColor);
                    m_Renderer->DrawColoredRect(
                        glm::vec2(anchorRight.x - 1.0f, anchorRight.y - markerSize),
                        glm::vec2(2.0f, markerSize * 2.0f),
                        anchorColor);
                }
            }
        }
    }
}

void Game::RenderNoProjectionAnchors()
{
    if (!m_ShowNoProjectionAnchors)
        return;

    // Check if 3D perspective is enabled
    auto perspState = m_Renderer->GetPerspectiveState();
    bool is3DMode = perspState.enabled;

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();
    int mapWidth = m_Tilemap.GetMapWidth();
    int mapHeight = m_Tilemap.GetMapHeight();
    size_t layerCount = m_Tilemap.GetLayerCount();

    // Track processed tiles to avoid drawing anchors multiple times
    // TODO: reuse cached structure bounds instead of scanning the full map every frame.
    std::vector<bool> processed(static_cast<size_t>(mapWidth * mapHeight), false);

    // Scan entire map for no-projection structures
    for (int y = 0; y < mapHeight; ++y)
    {
        for (int x = 0; x < mapWidth; ++x)
        {
            int idx = y * mapWidth + x;
            if (processed[idx])
                continue;

            // Check if this tile is no-projection in any layer
            bool isNoProj = false;
            for (size_t li = 0; li < layerCount; ++li)
            {
                if (m_Tilemap.GetLayerNoProjection(x, y, li))
                {
                    isNoProj = true;
                    break;
                }
            }
            if (!isNoProj)
                continue;

            // Flood-fill to find structure bounding box
            int minX = x, maxX = x, minY = y, maxY = y;
            std::vector<std::pair<int, int>> stack;
            stack.push_back({x, y});

            while (!stack.empty())
            {
                auto [cx, cy] = stack.back();
                stack.pop_back();

                if (cx < 0 || cx >= mapWidth || cy < 0 || cy >= mapHeight)
                    continue;

                int cIdx = cy * mapWidth + cx;
                if (processed[cIdx])
                    continue;

                bool cIsNoProj = false;
                for (size_t li = 0; li < layerCount; ++li)
                {
                    if (m_Tilemap.GetLayerNoProjection(cx, cy, li))
                    {
                        cIsNoProj = true;
                        break;
                    }
                }
                if (!cIsNoProj)
                    continue;

                processed[cIdx] = true;
                minX = std::min(minX, cx);
                maxX = std::max(maxX, cx);
                minY = std::min(minY, cy);
                maxY = std::max(maxY, cy);

                stack.push_back({cx - 1, cy});
                stack.push_back({cx + 1, cy});
                stack.push_back({cx, cy - 1});
                stack.push_back({cx, cy + 1});
            }

            // Calculate anchor positions in world pixels
            int leftPixelX = minX * tileWidth;
            int rightPixelX = (maxX + 1) * tileWidth;
            int bottomPixelY = (maxY + 1) * tileHeight;

            // Screen-space positions
            glm::vec2 screenLeft(static_cast<float>(leftPixelX) - m_CameraPosition.x,
                                 static_cast<float>(bottomPixelY) - m_CameraPosition.y);
            glm::vec2 screenRight(static_cast<float>(rightPixelX) - m_CameraPosition.x,
                                  static_cast<float>(bottomPixelY) - m_CameraPosition.y);

            // In 3D mode, project through perspective
            glm::vec2 anchorLeft = is3DMode ? m_Renderer->ProjectPoint(screenLeft) : screenLeft;
            glm::vec2 anchorRight = is3DMode ? m_Renderer->ProjectPoint(screenRight) : screenRight;

            // Draw anchor markers (green crosses)
            float markerSize = 6.0f;
            glm::vec4 anchorColor(0.0f, 1.0f, 0.0f, 1.0f);

            // Bottom-left anchor
            m_Renderer->DrawColoredRect(
                glm::vec2(anchorLeft.x - markerSize, anchorLeft.y - 1.0f),
                glm::vec2(markerSize * 2.0f, 2.0f),
                anchorColor);
            m_Renderer->DrawColoredRect(
                glm::vec2(anchorLeft.x - 1.0f, anchorLeft.y - markerSize),
                glm::vec2(2.0f, markerSize * 2.0f),
                anchorColor);

            // Bottom-right anchor
            m_Renderer->DrawColoredRect(
                glm::vec2(anchorRight.x - markerSize, anchorRight.y - 1.0f),
                glm::vec2(markerSize * 2.0f, 2.0f),
                anchorColor);
            m_Renderer->DrawColoredRect(
                glm::vec2(anchorRight.x - 1.0f, anchorRight.y - markerSize),
                glm::vec2(2.0f, markerSize * 2.0f),
                anchorColor);
        }
    }
}

void Game::RenderYSortPlusOverlays()
{
    // Use same world size as main render
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            // In Y-sort-plus edit mode, only show flags for current layer
            if (m_YSortPlusEditMode)
            {
                if (!m_Tilemap.GetLayerYSortPlus(x, y, m_CurrentLayer))
                    continue;

                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x,
                                  y * tileHeight - m_CameraPosition.y);

                // Cyan overlay for Y-sort-plus tiles
                m_Renderer->DrawColoredRect(
                    tilePos,
                    glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                    glm::vec4(0.0f, 0.8f, 0.8f, 0.5f));
            }
            else
            {
                // Check tile for all layers for Y-sort-plus flag
                int count = 0;
                size_t layerCount = m_Tilemap.GetLayerCount();
                for (size_t layer = 0; layer < layerCount; ++layer)
                {
                    if (m_Tilemap.GetLayerYSortPlus(x, y, layer))
                        count++;
                }

                if (count == 0)
                    continue;

                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x,
                                  y * tileHeight - m_CameraPosition.y);

                // Alpha based on number of layers with flag
                float alpha = 0.15f + (static_cast<float>(count) / static_cast<float>(layerCount)) * 0.35f;

                // Cyan overlay for Y-sort-plus tiles
                m_Renderer->DrawColoredRect(
                    tilePos,
                    glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                    glm::vec4(0.0f, 0.8f, 0.8f, alpha));
            }
        }
    }
}

void Game::RenderYSortMinusOverlays()
{
    // Use same world size as main render
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            // In Y-sort-minus edit mode, only show flags for current layer
            if (m_YSortMinusEditMode)
            {
                if (!m_Tilemap.GetLayerYSortMinus(x, y, m_CurrentLayer))
                    continue;

                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x,
                                  y * tileHeight - m_CameraPosition.y);

                // Magenta overlay for Y-sort-minus tiles
                m_Renderer->DrawColoredRect(
                    tilePos,
                    glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                    glm::vec4(0.9f, 0.2f, 0.9f, 0.5f));
            }
            else
            {
                // Check tile for all layers for Y-sort-minus flag
                int count = 0;
                size_t layerCount = m_Tilemap.GetLayerCount();
                for (size_t layer = 0; layer < layerCount; ++layer)
                {
                    if (m_Tilemap.GetLayerYSortMinus(x, y, layer))
                        count++;
                }

                if (count == 0)
                    continue;

                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x,
                                  y * tileHeight - m_CameraPosition.y);

                // Alpha based on number of layers with flag
                float alpha = 0.15f + (static_cast<float>(count) / static_cast<float>(layerCount)) * 0.35f;

                // Magenta overlay for Y-sort-minus tiles
                m_Renderer->DrawColoredRect(
                    tilePos,
                    glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                    glm::vec4(0.9f, 0.2f, 0.9f, alpha));
            }
        }
    }
}

void Game::RenderParticleZoneOverlays()
{
    // Use same world size as main render
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;

    const auto *zones = m_Tilemap.GetParticleZones();
    if (!zones)
        return;

    for (const auto &zone : *zones)
    {
        // Calculate screen position
        glm::vec2 screenPos = zone.position - m_CameraPosition;

        // Cull zones outside view
        if (screenPos.x + zone.size.x < 0 || screenPos.x > worldWidth ||
            screenPos.y + zone.size.y < 0 || screenPos.y > worldHeight)
            continue;

        // Color based on particle type
        glm::vec4 color;
        switch (zone.type)
        {
        case ParticleType::Firefly:
            color = glm::vec4(1.0f, 0.9f, 0.2f, 0.3f); // Yellow
            break;
        case ParticleType::Rain:
            color = glm::vec4(0.3f, 0.5f, 1.0f, 0.3f); // Blue
            break;
        case ParticleType::Snow:
            color = glm::vec4(0.9f, 0.9f, 1.0f, 0.3f); // White
            break;
        case ParticleType::Fog:
            color = glm::vec4(0.7f, 0.7f, 0.8f, 0.3f); // Grey
            break;
        case ParticleType::Sparkles:
            color = glm::vec4(1.0f, 1.0f, 0.5f, 0.3f); // Light yellow
            break;
        case ParticleType::Wisp:
            color = glm::vec4(0.5f, 0.8f, 1.0f, 0.3f); // Cyan/ethereal blue
            break;
        default:
            color = glm::vec4(1.0f, 1.0f, 1.0f, 0.3f); // White fallback
            break;
        }

        if (!zone.enabled)
            color.a *= 0.3f; // Dimmer if disabled

        m_Renderer->DrawColoredRect(screenPos, zone.size, color);

        // Draw border for clarity
        float borderWidth = 2.0f;
        glm::vec4 borderColor = color;
        borderColor.a = 0.6f;

        // Top border
        m_Renderer->DrawColoredRect(screenPos, glm::vec2(zone.size.x, borderWidth), borderColor);
        // Bottom border
        m_Renderer->DrawColoredRect(glm::vec2(screenPos.x, screenPos.y + zone.size.y - borderWidth),
                                    glm::vec2(zone.size.x, borderWidth), borderColor);
        // Left border
        m_Renderer->DrawColoredRect(screenPos, glm::vec2(borderWidth, zone.size.y), borderColor);
        // Right border
        m_Renderer->DrawColoredRect(glm::vec2(screenPos.x + zone.size.x - borderWidth, screenPos.y),
                                    glm::vec2(borderWidth, zone.size.y), borderColor);
    }

    // Draw preview of zone being placed
    if (m_PlacingParticleZone)
    {
        // Get current mouse position in world coordinates
        double mouseX, mouseY;
        glfwGetCursorPos(m_Window, &mouseX, &mouseY);

        float worldX = (static_cast<float>(mouseX) / static_cast<float>(m_ScreenWidth)) * worldWidth + m_CameraPosition.x;
        float worldY = (static_cast<float>(mouseY) / static_cast<float>(m_ScreenHeight)) * worldHeight + m_CameraPosition.y;

        // Get start and end tile indices
        int startTileX = static_cast<int>(m_ParticleZoneStart.x / m_Tilemap.GetTileWidth());
        int startTileY = static_cast<int>(m_ParticleZoneStart.y / m_Tilemap.GetTileHeight());
        int endTileX = static_cast<int>(std::floor(worldX / m_Tilemap.GetTileWidth()));
        int endTileY = static_cast<int>(std::floor(worldY / m_Tilemap.GetTileHeight()));

        // Calculate min & max tile indices to handle any drag direction
        int minTileX = std::min(startTileX, endTileX);
        int maxTileX = std::max(startTileX, endTileX);
        int minTileY = std::min(startTileY, endTileY);
        int maxTileY = std::max(startTileY, endTileY);

        // Zone spans from left edge of min tile to right edge of max tile
        float zoneX = static_cast<float>(minTileX * m_Tilemap.GetTileWidth());
        float zoneY = static_cast<float>(minTileY * m_Tilemap.GetTileHeight());
        float zoneW = static_cast<float>((maxTileX - minTileX + 1) * m_Tilemap.GetTileWidth());
        float zoneH = static_cast<float>((maxTileY - minTileY + 1) * m_Tilemap.GetTileHeight());

        // Preview color based on type
        // TODO: Lantern should have its own color
        glm::vec4 previewColor;
        switch (m_CurrentParticleType)
        {
        case ParticleType::Firefly:
            previewColor = glm::vec4(1.0f, 0.9f, 0.2f, 0.5f);
            break;
        case ParticleType::Rain:
            previewColor = glm::vec4(0.3f, 0.5f, 1.0f, 0.5f);
            break;
        case ParticleType::Snow:
            previewColor = glm::vec4(0.9f, 0.9f, 1.0f, 0.5f);
            break;
        case ParticleType::Fog:
            previewColor = glm::vec4(0.7f, 0.7f, 0.8f, 0.5f);
            break;
        case ParticleType::Sparkles:
            previewColor = glm::vec4(1.0f, 1.0f, 0.5f, 0.5f);
            break;
        case ParticleType::Wisp:
            previewColor = glm::vec4(0.5f, 0.8f, 1.0f, 0.5f);
            break;
        default:
            previewColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f);
            break;
        }

        glm::vec2 previewPos(zoneX - m_CameraPosition.x, zoneY - m_CameraPosition.y);
        m_Renderer->DrawColoredRect(previewPos, glm::vec2(zoneW, zoneH), previewColor);
    }
}

void Game::RenderNPCDebugInfo()
{
    // Use same world size as main render
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    const float NPC_HITBOX_SIZE = PlayerCharacter::HITBOX_HEIGHT;

    for (const auto &npc : m_NPCs)
    {
        glm::vec2 npcAnchor = npc.GetPosition();

        // Render NPC hitbox
        glm::vec2 npcHitboxPos(npcAnchor.x - NPC_HITBOX_SIZE * 0.5f - m_CameraPosition.x,
                               npcAnchor.y - NPC_HITBOX_SIZE - m_CameraPosition.y);
        glm::vec2 npcHitboxSize(NPC_HITBOX_SIZE, NPC_HITBOX_SIZE);

        if (npcHitboxPos.x + npcHitboxSize.x >= 0 && npcHitboxPos.x <= screenSize.x &&
            npcHitboxPos.y + npcHitboxSize.y >= 0 && npcHitboxPos.y <= screenSize.y)
        {
            // Filled purple rect
            m_Renderer->DrawColoredRect(npcHitboxPos,
                                        npcHitboxSize,
                                        glm::vec4(1.0f, 0.0f, 1.0f, 0.3f));
        }

        // Render next waypoint as green dot
        int targetX = npc.m_TargetTileX;
        int targetY = npc.m_TargetTileY;

        glm::vec2 targetPos(targetX * tileWidth - m_CameraPosition.x + tileWidth * 0.5f,
                            targetY * tileHeight - m_CameraPosition.y + tileHeight * 0.5f);

        // Check if target is on screen
        if (targetPos.x >= -tileWidth && targetPos.x <= screenSize.x + tileWidth &&
            targetPos.y >= -tileHeight && targetPos.y <= screenSize.y + tileHeight)
        {
            // Draw a small 6x6 green square for waypoint
            float dotSize = 6.0f;
            m_Renderer->DrawColoredRect(targetPos - glm::vec2(dotSize * 0.5f),
                                        glm::vec2(dotSize),
                                        glm::vec4(0.0f, 1.0f, 0.0f, 0.8f));
        }
    }
}

void Game::RenderCornerCuttingOverlays()
{
    // Use same world size as main render
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    // Calculate visible tile range
    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    // Player hitbox 16x16 pixels
    const float HITBOX_SIZE = PlayerCharacter::HITBOX_WIDTH;
    const float HITBOX_HALF = HITBOX_SIZE * 0.5f; // 8 pixels from center
    const float TILE_SIZE = m_Tilemap.GetTileWidth();

    // Walking allows 20% overlap threshold on diagonal corners only
    const float CORNER_OVERLAP_THRESHOLD = 0.20f;
    const float HITBOX_AREA = HITBOX_SIZE * HITBOX_SIZE;                   // 256 sq pixels
    const float MAX_OVERLAP_AREA = HITBOX_AREA * CORNER_OVERLAP_THRESHOLD; // 51.2 sq pixels
    float walkingCornerPenetration = std::sqrt(MAX_OVERLAP_AREA);          // ~7.155 pixels

    // Running allows center-point collision penetration up to hitbox edge
    float runningEdgePenetration = HITBOX_HALF; // 8 pixels

    // Render collision tolerance zones for all collision tiles
    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            if (!m_Tilemap.GetTileCollision(x, y))
                continue;

            glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x, y * tileHeight - m_CameraPosition.y);

            // Check adjacency for this tile to determine valid exposed corners and edges
            bool freeLeft = (x > 0) && !m_Tilemap.GetTileCollision(x - 1, y);
            bool freeRight = (x < m_Tilemap.GetMapWidth() - 1) && !m_Tilemap.GetTileCollision(x + 1, y);
            bool freeTop = (y > 0) && !m_Tilemap.GetTileCollision(x, y - 1);
            bool freeBottom = (y < m_Tilemap.GetMapHeight() - 1) && !m_Tilemap.GetTileCollision(x, y + 1);

            // Left Edge
            if (freeLeft)
            {
                m_Renderer->DrawColoredRect(
                    glm::vec2(tilePos.x, tilePos.y),
                    glm::vec2(runningEdgePenetration, TILE_SIZE),
                    glm::vec4(1.0f, 0.6f, 0.2f, 0.5f));
            }
            // Right Edge
            if (freeRight)
            {
                m_Renderer->DrawColoredRect(
                    glm::vec2(tilePos.x + TILE_SIZE - runningEdgePenetration, tilePos.y),
                    glm::vec2(runningEdgePenetration, TILE_SIZE),
                    glm::vec4(1.0f, 0.6f, 0.2f, 0.5f));
            }
            // Top Edge
            if (freeTop)
            {
                m_Renderer->DrawColoredRect(
                    glm::vec2(tilePos.x, tilePos.y),
                    glm::vec2(TILE_SIZE, runningEdgePenetration),
                    glm::vec4(1.0f, 0.6f, 0.2f, 0.5f));
            }
            // Bottom Edge
            if (freeBottom)
            {
                m_Renderer->DrawColoredRect(
                    glm::vec2(tilePos.x, tilePos.y + TILE_SIZE - runningEdgePenetration),
                    glm::vec2(TILE_SIZE, runningEdgePenetration),
                    glm::vec4(1.0f, 0.6f, 0.2f, 0.5f));
            }

            struct CornerInfo
            {
                int dx, dy;                 // Diagonal direction to check
                float x, y;                 // Screen position of overlap zone
                bool isValid;               // Is this a valid exposed corner?
                Tilemap::Corner cornerEnum; // Which corner this is
            };

            // Check which corners have cutting blocked
            bool tlBlocked = m_Tilemap.IsCornerCutBlocked(x, y, Tilemap::CORNER_TL);
            bool trBlocked = m_Tilemap.IsCornerCutBlocked(x, y, Tilemap::CORNER_TR);
            bool blBlocked = m_Tilemap.IsCornerCutBlocked(x, y, Tilemap::CORNER_BL);
            bool brBlocked = m_Tilemap.IsCornerCutBlocked(x, y, Tilemap::CORNER_BR);

            CornerInfo corners[4] = {
                // Top-Left: Valid if Left & Top are free
                {-1, -1, tilePos.x, tilePos.y, freeLeft && freeTop, Tilemap::CORNER_TL},
                // Top-Right: Valid if Right & Top are free
                {1, -1, tilePos.x + TILE_SIZE, tilePos.y, freeRight && freeTop, Tilemap::CORNER_TR},
                // Bottom-Left: Valid if Left & Bottom are free
                {-1, 1, tilePos.x, tilePos.y + TILE_SIZE, freeLeft && freeBottom, Tilemap::CORNER_BL},
                // Bottom-Right: Valid if Right & Bottom are free
                {1, 1, tilePos.x + TILE_SIZE, tilePos.y + TILE_SIZE, freeRight && freeBottom, Tilemap::CORNER_BR}};

            bool cornerBlocked[4] = {tlBlocked, trBlocked, blBlocked, brBlocked};

            for (int i = 0; i < 4; ++i)
            {
                const auto &corner = corners[i];

                // Straight walls and internal corners have strictly no penetration
                if (!corner.isValid)
                    continue;

                int nx = x + corner.dx;
                int ny = y + corner.dy;

                // Only render if diagonal neighbor is walkable otherwise no escape path
                if (nx >= 0 && ny >= 0 &&
                    nx < m_Tilemap.GetMapWidth() &&
                    ny < m_Tilemap.GetMapHeight() &&
                    !m_Tilemap.GetTileCollision(nx, ny))
                {
                    // Calculate positions based on corner direction
                    float walkX = (corner.dx == -1) ? corner.x : corner.x - walkingCornerPenetration;
                    float walkY = (corner.dy == -1) ? corner.y : corner.y - walkingCornerPenetration;

                    if (cornerBlocked[i])
                    {
                        // Draw red indicator for blocked corner cutting
                        m_Renderer->DrawColoredRect(
                            glm::vec2(walkX, walkY),
                            glm::vec2(walkingCornerPenetration, walkingCornerPenetration),
                            glm::vec4(1.0f, 0.2f, 0.2f, 0.9f));
                    }
                    else
                    {
                        // Draw green walking corner penetration zone (normal)
                        m_Renderer->DrawColoredRect(
                            glm::vec2(walkX, walkY),
                            glm::vec2(walkingCornerPenetration, walkingCornerPenetration),
                            glm::vec4(0.5f, 1.0f, 0.0f, 0.8f));
                    }
                }
            }
        }
    }
}

void Game::RenderLayer2Overlays()
{
    // Render blue transparent overlays on layer 2 tiles
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    // Calculate visible tile range
    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    // Render blue overlay for each layer 2 tile
    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            int tileID = m_Tilemap.GetTile2(x, y);
            if (tileID >= 0)
            {
                // Calculate tile position in screen space
                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x, y * tileHeight - m_CameraPosition.y);

                // DrawColoredRect uses bottom-left origin like DrawSpriteRegion
                // Blue overlay (Ground Detail - Layer 1)
                m_Renderer->DrawColoredRect(tilePos, glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                                            glm::vec4(0.2f, 0.5f, 1.0f, 0.4f));
            }
        }
    }
}

void Game::RenderLayer3Overlays()
{
    // Render green transparent overlays on layer 3 tiles
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    // Calculate visible tile range
    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    // Render green overlay for each layer 3 tile
    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            int tileID = m_Tilemap.GetTile3(x, y);
            if (tileID >= 0)
            {
                // Calculate tile position in screen space
                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x, y * tileHeight - m_CameraPosition.y);

                // DrawColoredRect uses bottom-left origin like DrawSpriteRegion
                // Green overlay (Objects - Layer 2)
                m_Renderer->DrawColoredRect(tilePos, glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                                            glm::vec4(0.2f, 1.0f, 0.2f, 0.4f));
            }
        }
    }
}

void Game::RenderLayer4Overlays()
{
    // Render purple transparent overlays on layer 4 tiles
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    // Calculate visible tile range
    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    // Render purple overlay for each layer 4 tile
    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            int tileID = m_Tilemap.GetTile4(x, y);
            if (tileID >= 0)
            {
                // Calculate tile position in screen space
                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x, y * tileHeight - m_CameraPosition.y);

                // DrawColoredRect uses bottom-left origin like DrawSpriteRegion
                // Magenta overlay (Objects2 - Layer 3)
                m_Renderer->DrawColoredRect(tilePos, glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                                            glm::vec4(1.0f, 0.2f, 0.8f, 0.4f));
            }
        }
    }
}

void Game::RenderLayer5Overlays()
{
    // Render orange transparent overlays on layer 5 tiles
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    // Calculate visible tile range
    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    // Render orange overlay for each layer 5 tile
    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            int tileID = m_Tilemap.GetTile5(x, y);
            if (tileID >= 0)
            {
                // Calculate tile position in screen space
                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x, y * tileHeight - m_CameraPosition.y);

                // DrawColoredRect uses bottom-left origin like DrawSpriteRegion
                // Orange overlay (Foreground - Layer 4)
                m_Renderer->DrawColoredRect(tilePos, glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                                            glm::vec4(1.0f, 0.5f, 0.0f, 0.4f));
            }
        }
    }
}

void Game::RenderLayer6Overlays()
{
    // Render yellow transparent overlays on layer 6 tiles
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    // Calculate visible tile range
    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    // Render yellow overlay for each layer 6 tile
    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            int tileID = m_Tilemap.GetTile6(x, y);
            if (tileID >= 0)
            {
                // Calculate tile position in screen space
                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x, y * tileHeight - m_CameraPosition.y);

                // DrawColoredRect uses bottom-left origin like DrawSpriteRegion
                // Yellow overlay (Foreground2 - Layer 5)
                m_Renderer->DrawColoredRect(tilePos, glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                                            glm::vec4(1.0f, 1.0f, 0.2f, 0.4f));
            }
        }
    }
}

void Game::RenderLayer7Overlays()
{
    // Render cyan transparent overlays on layer 7 tiles
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            int tileID = m_Tilemap.GetLayerTile(x, y, 6);
            if (tileID >= 0)
            {
                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x, y * tileHeight - m_CameraPosition.y);
                // Cyan overlay (Overlay - Layer 6)
                m_Renderer->DrawColoredRect(tilePos, glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                                            glm::vec4(0.2f, 1.0f, 1.0f, 0.4f));
            }
        }
    }
}

void Game::RenderLayer8Overlays()
{
    // Render red transparent overlays on layer 8 tiles
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;
    glm::vec2 screenSize(worldWidth, worldHeight);

    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    int startX = std::max(0, (int)(m_CameraPosition.x / tileWidth) - 1);
    int endX = std::min(m_Tilemap.GetMapWidth(), (int)((m_CameraPosition.x + screenSize.x) / tileWidth) + 1);
    int startY = std::max(0, (int)(m_CameraPosition.y / tileHeight) - 1);
    int endY = std::min(m_Tilemap.GetMapHeight(), (int)((m_CameraPosition.y + screenSize.y) / tileHeight) + 1);

    for (int y = startY; y < endY; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            int tileID = m_Tilemap.GetLayerTile(x, y, 7);
            if (tileID >= 0)
            {
                glm::vec2 tilePos(x * tileWidth - m_CameraPosition.x, y * tileHeight - m_CameraPosition.y);
                // Red overlay (Overlay2 - Layer 7)
                m_Renderer->DrawColoredRect(tilePos, glm::vec2(static_cast<float>(tileWidth), static_cast<float>(tileHeight)),
                                            glm::vec4(1.0f, 0.3f, 0.3f, 0.4f));
            }
        }
    }
}

void Game::RenderEditorUI()
{
    // Set tile picker projection and use base world dimensions without camera zoom
    float tilePickerWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float tilePickerWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    glm::mat4 tilePickerProjection = glm::ortho(0.0f, tilePickerWorldWidth, tilePickerWorldHeight, 0.0f, -1.0f, 1.0f);
    m_Renderer->SetProjection(tilePickerProjection);

    int dataTilesPerRow = m_Tilemap.GetTilesetDataWidth() / m_Tilemap.GetTileWidth();
    int dataTilesPerCol = m_Tilemap.GetTilesetDataHeight() / m_Tilemap.GetTileHeight();
    int totalTiles = dataTilesPerRow * dataTilesPerCol;

    int tilesPerRow = dataTilesPerRow;

    float baseTileSizePixels = (static_cast<float>(m_ScreenWidth) / static_cast<float>(tilesPerRow)) * 1.5f;
    float tileSizePixels = baseTileSizePixels * m_TilePickerZoom;

    float worldWidth = tilePickerWorldWidth;
    float worldHeight = tilePickerWorldHeight;

    float tilePickerWidth = static_cast<float>(m_ScreenWidth);
    float tilePickerHeight = static_cast<float>(m_ScreenHeight);

    float worldPickerWidth = (tilePickerWidth / m_ScreenWidth) * worldWidth;
    float worldPickerHeight = (tilePickerHeight / m_ScreenHeight) * worldHeight;

    // Background
    m_Renderer->DrawColoredRect(glm::vec2(0.0f, 0.0f),
                                glm::vec2(worldPickerWidth, worldPickerHeight),
                                glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    // Render only visible tiles, cull off-screen tiles
    int startCol = std::max(0, static_cast<int>(std::floor((-m_TilePickerOffsetX) / tileSizePixels)));
    int endCol = std::min(tilesPerRow - 1, static_cast<int>(std::floor((m_ScreenWidth - m_TilePickerOffsetX) / tileSizePixels)));
    int startRow = std::max(0, static_cast<int>(std::floor((-m_TilePickerOffsetY) / tileSizePixels)));
    int endRow = std::min(dataTilesPerCol - 1, static_cast<int>(std::floor((m_ScreenHeight - m_TilePickerOffsetY) / tileSizePixels)));

    for (int row = startRow; row <= endRow; ++row)
    {
        for (int col = startCol; col <= endCol; ++col)
        {
            int tileID = row * tilesPerRow + col;
            if (tileID < 0 || tileID >= totalTiles)
                continue;

            if (m_Tilemap.IsTileTransparent(tileID))
                continue;

            float screenX = col * tileSizePixels + m_TilePickerOffsetX;
            float screenY = row * tileSizePixels + m_TilePickerOffsetY;

            float worldX = (screenX / m_ScreenWidth) * worldWidth;
            float worldY = (screenY / m_ScreenHeight) * worldHeight;
            float worldTileSize = (tileSizePixels / m_ScreenWidth) * worldWidth;

            int tilesetX = col * m_Tilemap.GetTileWidth();
            int tilesetY = row * m_Tilemap.GetTileHeight();

            glm::vec2 texCoord(static_cast<float>(tilesetX), static_cast<float>(tilesetY));
            glm::vec2 texSize(m_Tilemap.GetTileWidth(), m_Tilemap.GetTileHeight());

            glm::vec3 color = (tileID == m_SelectedTileID) ? glm::vec3(1.5f, 1.5f, 1.0f) : glm::vec3(1.0f);

            // Query renderer at runtime for Y-flip (OpenGL=true, Vulkan=false)
            bool flipY = m_Renderer->RequiresYFlip();

            m_Renderer->DrawSpriteRegion(m_Tilemap.GetTilesetTexture(), glm::vec2(worldX, worldY),
                                         glm::vec2(worldTileSize, worldTileSize),
                                         texCoord, texSize, 0.0f, color, flipY);
        }
    }

    // Selection rectangle
    if (m_IsSelectingTiles && m_SelectionStartTileID >= 0)
    {
        int startTileID = m_SelectionStartTileID;
        int endTileID = m_SelectedTileID;

        int startX = startTileID % tilesPerRow;
        int startY = startTileID / tilesPerRow;
        int endX = endTileID % tilesPerRow;
        int endY = endTileID / tilesPerRow;

        int minX = std::min(startX, endX);
        int maxX = std::max(startX, endX);
        int minY = std::min(startY, endY);
        int maxY = std::max(startY, endY);

        float selStartX = minX * tileSizePixels + m_TilePickerOffsetX;
        float selStartY = minY * tileSizePixels + m_TilePickerOffsetY;
        float selWidth = (maxX - minX + 1) * tileSizePixels;
        float selHeight = (maxY - minY + 1) * tileSizePixels;

        float worldSelX = (selStartX / m_ScreenWidth) * worldWidth;
        float worldSelY = (selStartY / m_ScreenHeight) * worldHeight;
        float worldSelWidth = (selWidth / m_ScreenWidth) * worldWidth;
        float worldSelHeight = (selHeight / m_ScreenHeight) * worldHeight;

        float outlineThickness = 2.0f;
        // TODO: consider drawing selection overlay in screen space to avoid scaling thickness with DPI/zoom.
        // Top
        m_Renderer->DrawColoredRect(glm::vec2(worldSelX, worldSelY),
                                    glm::vec2(worldSelWidth, outlineThickness),
                                    glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
        // Bottom
        m_Renderer->DrawColoredRect(glm::vec2(worldSelX, worldSelY + worldSelHeight - outlineThickness),
                                    glm::vec2(worldSelWidth, outlineThickness),
                                    glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
        // Left
        m_Renderer->DrawColoredRect(glm::vec2(worldSelX, worldSelY),
                                    glm::vec2(outlineThickness, worldSelHeight),
                                    glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
        // Right
        m_Renderer->DrawColoredRect(glm::vec2(worldSelX + worldSelWidth - outlineThickness, worldSelY),
                                    glm::vec2(outlineThickness, worldSelHeight),
                                    glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
    }

    // Draw animation frame highlights in animation edit mode
    if (m_AnimationEditMode && !m_AnimationFrames.empty())
    {
        for (size_t i = 0; i < m_AnimationFrames.size(); ++i)
        {
            int frameID = m_AnimationFrames[i];
            int frameX = frameID % tilesPerRow;
            int frameY = frameID / tilesPerRow;

            float frameScreenX = frameX * tileSizePixels + m_TilePickerOffsetX;
            float frameScreenY = frameY * tileSizePixels + m_TilePickerOffsetY;

            float worldFrameX = (frameScreenX / m_ScreenWidth) * worldWidth;
            float worldFrameY = (frameScreenY / m_ScreenHeight) * worldHeight;
            float worldTileSize = (tileSizePixels / m_ScreenWidth) * worldWidth;

            // Draw numbered highlight
            float outlineThickness = 2.0f;
            glm::vec4 highlightColor(0.0f, 1.0f, 0.0f, 1.0f);
            m_Renderer->DrawColoredRect(glm::vec2(worldFrameX, worldFrameY),
                                        glm::vec2(worldTileSize, outlineThickness), highlightColor);
            m_Renderer->DrawColoredRect(glm::vec2(worldFrameX, worldFrameY + worldTileSize - outlineThickness),
                                        glm::vec2(worldTileSize, outlineThickness), highlightColor);
            m_Renderer->DrawColoredRect(glm::vec2(worldFrameX, worldFrameY),
                                        glm::vec2(outlineThickness, worldTileSize), highlightColor);
            m_Renderer->DrawColoredRect(glm::vec2(worldFrameX + worldTileSize - outlineThickness, worldFrameY),
                                        glm::vec2(outlineThickness, worldTileSize), highlightColor);

            // Draw frame number
            std::string frameNum = std::to_string(i + 1);
            m_Renderer->DrawText(frameNum, glm::vec2(worldFrameX + 2.0f, worldFrameY + 2.0f), 0.3f, glm::vec3(0.0f, 1.0f, 0.0f));
        }
    }

    // Draw animation mode status
    if (m_AnimationEditMode)
    {
        std::string animStatus;
        if (m_SelectedAnimationId >= 0)
        {
            animStatus = "Animation tile: Click map to apply #" + std::to_string(m_SelectedAnimationId);
        }
        else if (!m_AnimationFrames.empty())
        {
            animStatus = "Animation tile: " + std::to_string(m_AnimationFrames.size()) + " frames (" +
                         std::to_string(static_cast<int>(m_AnimationFrameDuration * 1000)) + "ms) - Enter to create";
        }
        else
        {
            animStatus = "Animation tile: Click tiles to add frames";
        }
        m_Renderer->DrawText(animStatus, glm::vec2(20.0f, 20.0f), 0.4f, glm::vec3(0.0f, 1.0f, 0.0f));
    }
}

void Game::RenderPlacementPreview()
{
    // Draw animation mode status when not in tile picker
    if (m_AnimationEditMode && !m_ShowTilePicker && m_SelectedAnimationId >= 0)
    {
        std::string animStatus = "Animation tile: Click map to apply #" + std::to_string(m_SelectedAnimationId) + " (Esc to cancel, K to exit)";
        m_Renderer->DrawText(animStatus, glm::vec2(20.0f, 20.0f), 0.4f, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // Only show preview if we have a selection and are not in tile picker
    if (m_ShowTilePicker)
        return;
    if (m_SelectedTileStartID < 0)
        return;

    double mouseX, mouseY;
    glfwGetCursorPos(m_Window, &mouseX, &mouseY);

    // Convert screen coordinates to world coordinates
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;

    // Convert mouse position to world coordinates
    float worldX = (static_cast<float>(mouseX) / static_cast<float>(m_ScreenWidth)) * worldWidth + m_CameraPosition.x;
    float worldY = (static_cast<float>(mouseY) / static_cast<float>(m_ScreenHeight)) * worldHeight + m_CameraPosition.y;

    // Convert world coordinates to tile coordinates
    int tileX = static_cast<int>(std::floor(worldX / m_Tilemap.GetTileWidth()));
    int tileY = static_cast<int>(std::floor(worldY / m_Tilemap.GetTileHeight()));

    int dataTilesPerRow = m_Tilemap.GetTilesetDataWidth() / m_Tilemap.GetTileWidth();
    int tileWidth = m_Tilemap.GetTileWidth();
    int tileHeight = m_Tilemap.GetTileHeight();

    if (m_MultiTileSelectionMode)
    {
        // Calculate rotated dimensions
        int rotatedWidth = (m_MultiTileRotation == 90 || m_MultiTileRotation == 270) ? m_SelectedTileHeight : m_SelectedTileWidth;
        int rotatedHeight = (m_MultiTileRotation == 90 || m_MultiTileRotation == 270) ? m_SelectedTileWidth : m_SelectedTileHeight;

        // Render preview of multi-tile selection with rotation
        for (int dy = 0; dy < rotatedHeight; ++dy)
        {
            for (int dx = 0; dx < rotatedWidth; ++dx)
            {
                // Calculate source tile coordinates based on rotation
                int sourceDx, sourceDy;
                if (m_MultiTileRotation == 0)
                {
                    sourceDx = dx;
                    sourceDy = dy;
                }
                else if (m_MultiTileRotation == 90)
                {
                    sourceDx = m_SelectedTileWidth - 1 - dy;
                    sourceDy = dx;
                }
                else if (m_MultiTileRotation == 180)
                {
                    sourceDx = m_SelectedTileWidth - 1 - dx;
                    sourceDy = m_SelectedTileHeight - 1 - dy;
                }
                else // 270 degrees
                {
                    sourceDx = dy;
                    sourceDy = m_SelectedTileHeight - 1 - dx;
                }

                int previewX = tileX + dx;
                int previewY = tileY + dy;
                int sourceTileID = m_SelectedTileStartID + sourceDy * dataTilesPerRow + sourceDx;

                // Calculate tile position in camera-relative coordinates
                // Tiles are rendered at: (x * tileWidth) - cameraPos.x
                glm::vec2 tilePos((previewX * tileWidth) - m_CameraPosition.x,
                                  (previewY * tileHeight) - m_CameraPosition.y);

                // Render semi-transparent preview
                int tilesetX = (sourceTileID % dataTilesPerRow) * tileWidth;
                int tilesetY = (sourceTileID / dataTilesPerRow) * tileHeight;

                glm::vec2 texCoord(static_cast<float>(tilesetX), static_cast<float>(tilesetY));
                glm::vec2 texSize(m_Tilemap.GetTileWidth(), m_Tilemap.GetTileHeight());

                // Query renderer at runtime for Y-flip (OpenGL=true, Vulkan=false)
                bool flipY = m_Renderer->RequiresYFlip();

                // Render with reduced opacity for preview, rotated by the multi-tile rotation angle
                // For 90 and 270, flip the texture rotation by 180 to compensate for coordinate system
                float tileRotation = static_cast<float>(m_MultiTileRotation);
                if (m_MultiTileRotation == 90 || m_MultiTileRotation == 270)
                {
                    tileRotation = static_cast<float>((m_MultiTileRotation + 180) % 360);
                }
                m_Renderer->DrawSpriteRegion(m_Tilemap.GetTilesetTexture(), tilePos,
                                             glm::vec2(m_Tilemap.GetTileWidth(), m_Tilemap.GetTileHeight()),
                                             texCoord, texSize, tileRotation, glm::vec3(1.0f, 1.0f, 0.5f), flipY);

                // Render outline
                m_Renderer->DrawColoredRect(tilePos, glm::vec2(16.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.8f));                                 // Top
                m_Renderer->DrawColoredRect(glm::vec2(tilePos.x, tilePos.y + 15.0f), glm::vec2(16.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.8f)); // Bottom
                m_Renderer->DrawColoredRect(tilePos, glm::vec2(1.0f, 16.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.8f));                                 // Left
                m_Renderer->DrawColoredRect(glm::vec2(tilePos.x + 15.0f, tilePos.y), glm::vec2(1.0f, 16.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.8f)); // Right
            }
        }
    }
    else
    {
        // Render preview of single tile
        if (tileX >= 0 && tileX < m_Tilemap.GetMapWidth() &&
            tileY >= 0 && tileY < m_Tilemap.GetMapHeight())
        {
            glm::vec2 tilePos((tileX * tileWidth) - m_CameraPosition.x,
                              (tileY * tileHeight) - m_CameraPosition.y);

            int tilesetX = (m_SelectedTileStartID % dataTilesPerRow) * tileWidth;
            int tilesetY = (m_SelectedTileStartID / dataTilesPerRow) * tileHeight;

            glm::vec2 texCoord(static_cast<float>(tilesetX), static_cast<float>(tilesetY));
            glm::vec2 texSize(16.0f, 16.0f);

            // Query renderer at runtime for Y-flip (OpenGL=true, Vulkan=false)
            bool flipY = m_Renderer->RequiresYFlip();

            // Apply rotation to single tile preview
            // For 90 and 270, flip the texture rotation by 180 to compensate for coordinate system
            float tileRotation = static_cast<float>(m_MultiTileRotation);
            if (m_MultiTileRotation == 90 || m_MultiTileRotation == 270)
            {
                tileRotation = static_cast<float>((m_MultiTileRotation + 180) % 360);
            }

            m_Renderer->DrawSpriteRegion(m_Tilemap.GetTilesetTexture(), tilePos,
                                         glm::vec2(16.0f, 16.0f),
                                         texCoord, texSize, tileRotation, glm::vec3(1.0f, 1.0f, 0.5f), flipY);

            // Render outline
            // TODO: use tileWidth/tileHeight instead of hardcoded 16s to support variable tile sizes.
            m_Renderer->DrawColoredRect(tilePos, glm::vec2(16.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.8f));                                 // Top
            m_Renderer->DrawColoredRect(glm::vec2(tilePos.x, tilePos.y + 15.0f), glm::vec2(16.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.8f)); // Bottom
            m_Renderer->DrawColoredRect(tilePos, glm::vec2(1.0f, 16.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.8f));                                 // Left
            m_Renderer->DrawColoredRect(glm::vec2(tilePos.x + 15.0f, tilePos.y), glm::vec2(1.0f, 16.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.8f)); // Right
        }
    }
}
