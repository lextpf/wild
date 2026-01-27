#include "Game.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include <glad/glad.h>

void Game::ProcessInput(float deltaTime)
{
    glm::vec2 moveDirection(0.0f);

    // Check if shift is pressed for running (1.5x movement speed)
    bool isRunning = (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                      glfwGetKey(m_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    // Reset copied NPC appearance when starting to run
    if (isRunning && m_Player.IsUsingCopiedAppearance())
    {
        m_Player.RestoreOriginalAppearance();
        m_Player.UploadTextures(*m_Renderer);
    }

    m_Player.SetRunning(isRunning);

    // Standard WASD layout for 8-directional movement
    // Y increases downward in screen space (top-left origin), so W = -Y, S = +Y
    if (glfwGetKey(m_Window, GLFW_KEY_W) == GLFW_PRESS)
    {
        moveDirection.y -= 1.0f; // Up
    }
    if (glfwGetKey(m_Window, GLFW_KEY_A) == GLFW_PRESS)
    {
        moveDirection.x -= 1.0f; // Left
    }
    if (glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_PRESS)
    {
        moveDirection.y += 1.0f; // Down
    }
    if (glfwGetKey(m_Window, GLFW_KEY_D) == GLFW_PRESS)
    {
        moveDirection.x += 1.0f; // Right
    }

    // Toggles between gameplay and editor mode.
    // the tile picker is automatically shown and initialized.
    static bool eKeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_E) == GLFW_PRESS && !eKeyPressed)
    {
        m_EditorMode = !m_EditorMode;
        if (m_EditorMode)
        {
            // Show tile picker immediately when entering editor mode
            m_ShowTilePicker = true;

            // Sync smooth scrolling target with current position
            m_TilePickerTargetOffsetX = m_TilePickerOffsetX;
            m_TilePickerTargetOffsetY = m_TilePickerOffsetY;

            // Initialize selected tile to first valid tile if needed
            std::vector<int> validTiles = m_Tilemap.GetValidTileIDs();
            if (!validTiles.empty() && m_SelectedTileID == 0)
            {
                bool tile0Valid = false;
                for (int id : validTiles)
                {
                    if (id == 0)
                    {
                        tile0Valid = true;
                        break;
                    }
                }
                if (!tile0Valid)
                {
                    m_SelectedTileID = validTiles[0];
                    std::cout << "Initialized selected tile to ID: " << m_SelectedTileID << std::endl;
                }
            }
        }
        else
        {
            m_ShowTilePicker = false;
        }
        eKeyPressed = true;
        std::cout << "Editor mode: " << (m_EditorMode ? "ON" : "OFF") << std::endl;
        if (m_EditorMode)
        {
            std::cout << "Tile picker is now: " << (m_ShowTilePicker ? "SHOWN" : "HIDDEN") << std::endl;
            std::cout << "Press T to toggle tile picker visibility" << std::endl;
        }
    }
    if (glfwGetKey(m_Window, GLFW_KEY_E) == GLFW_RELEASE)
    {
        eKeyPressed = false;
    }

    // Shows or hides the tile picker overlay. When shown, the entire tileset is
    // displayed and the user can click to select tiles for placement.
    static bool tKeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_T) == GLFW_PRESS && !tKeyPressed && m_EditorMode)
    {
        m_ShowTilePicker = !m_ShowTilePicker;
        tKeyPressed = true;
        std::cout << "Tile picker: " << (m_ShowTilePicker ? "SHOWN" : "HIDDEN") << std::endl;

        if (m_ShowTilePicker)
        {
            // Sync smooth scrolling state to prevent jump
            m_TilePickerTargetOffsetX = m_TilePickerOffsetX;
            m_TilePickerTargetOffsetY = m_TilePickerOffsetY;
            std::vector<int> validTiles = m_Tilemap.GetValidTileIDs();
            std::cout << "Total valid tiles available: " << validTiles.size() << std::endl;
            std::cout << "Currently selected tile ID: " << m_SelectedTileID << std::endl;
        }
    }
    if (glfwGetKey(m_Window, GLFW_KEY_T) == GLFW_RELEASE)
    {
        tKeyPressed = false;
    }

    // Rotates the selected tile(s) by 90 increments (0 -> 90 -> 180 -> 270).
    // Works for both single tiles and multi-tile selections when tile picker is closed.
    static bool tileRotateKeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_R) == GLFW_PRESS && !tileRotateKeyPressed && m_EditorMode && !m_ShowTilePicker)
    {
        m_MultiTileRotation = (m_MultiTileRotation + 90) % 360;
        tileRotateKeyPressed = true;
        std::cout << "Tile rotation: " << m_MultiTileRotation << " degrees" << std::endl;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_R) == GLFW_RELEASE)
    {
        tileRotateKeyPressed = false;
    }

    // Pans the tile picker view using arrow keys. Shift increases speed 2.5x.
    // Uses smooth scrolling with target-based interpolation.
    if (m_EditorMode && m_ShowTilePicker)
    {
        float scrollSpeed = 1000.0f * deltaTime;

        // Shift modifier for faster navigation (2.5x speed)
        if (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(m_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
        {
            scrollSpeed *= 2.5f;
        }

        // Arrow key input
        if (glfwGetKey(m_Window, GLFW_KEY_UP) == GLFW_PRESS)
        {
            m_TilePickerTargetOffsetY += scrollSpeed; // Scroll down (view up)
        }
        if (glfwGetKey(m_Window, GLFW_KEY_DOWN) == GLFW_PRESS)
        {
            m_TilePickerTargetOffsetY -= scrollSpeed; // Scroll up (view down)
        }
        if (glfwGetKey(m_Window, GLFW_KEY_LEFT) == GLFW_PRESS)
        {
            m_TilePickerTargetOffsetX += scrollSpeed; // Scroll right (view left)
        }
        if (glfwGetKey(m_Window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        {
            m_TilePickerTargetOffsetX -= scrollSpeed; // Scroll left (view right)
        }

        // Calculate tile picker layout dimensions
        int dataTilesPerRow = m_Tilemap.GetTilesetDataWidth() / m_Tilemap.GetTileWidth();
        int dataTilesPerCol = m_Tilemap.GetTilesetDataHeight() / m_Tilemap.GetTileHeight();

        // Tile display size: base size * zoom factor
        // Base size is calculated to fit all tiles horizontally with 1.5x padding
        float baseTileSizePixels = (static_cast<float>(m_ScreenWidth) / static_cast<float>(dataTilesPerRow)) * 1.5f;
        float tileSizePixels = baseTileSizePixels * m_TilePickerZoom;

        // Total content dimensions
        float totalTilesWidth = tileSizePixels * dataTilesPerRow;
        float totalTilesHeight = tileSizePixels * dataTilesPerCol;

        // Clamp offset bounds to prevent scrolling beyond content edges
        float minOffsetX = std::min(0.0f, m_ScreenWidth - totalTilesWidth);
        float maxOffsetX = 0.0f;
        float minOffsetY = std::min(0.0f, m_ScreenHeight - totalTilesHeight);
        float maxOffsetY = 0.0f;

        m_TilePickerTargetOffsetX = std::max(minOffsetX, std::min(maxOffsetX, m_TilePickerTargetOffsetX));
        m_TilePickerTargetOffsetY = std::max(minOffsetY, std::min(maxOffsetY, m_TilePickerTargetOffsetY));
    }

    // Toggles navigation map editing. When active:
    //   - Right-click toggles navigation flags on tiles
    //   - NPC placement mode is disabled (mutually exclusive)
    //   - Cyan overlay shows navigable tiles in debug view
    //
    // Navigation tiles determine where NPCs can walk for pathfinding.
    static bool mKeyPressed = false;
    if (m_EditorMode && glfwGetKey(m_Window, GLFW_KEY_M) == GLFW_PRESS && !mKeyPressed)
    {
        m_EditNavigationMode = !m_EditNavigationMode;
        if (m_EditNavigationMode)
        {
            m_NPCPlacementMode = false; // Mutually exclusive modes
            m_ElevationEditMode = false;
            m_NoProjectionEditMode = false;
            m_YSortPlusEditMode = false;
            m_YSortMinusEditMode = false;
            m_ParticleZoneEditMode = false;
            m_StructureEditMode = false;
            m_AnimationEditMode = false;
        }
        std::cout << "Navigation edit mode: " << (m_EditNavigationMode ? "ON" : "OFF") << std::endl;
        mKeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_M) == GLFW_RELEASE)
    {
        mKeyPressed = false;
    }

    // Toggles NPC placement mode. When active:
    //   - Left-click places/removes NPCs on navigation tiles
    //   - Navigation edit mode is disabled (mutually exclusive)
    //   - Use , and . keys to cycle through available NPC types
    static bool nKeyPressed = false;
    if (m_EditorMode && glfwGetKey(m_Window, GLFW_KEY_N) == GLFW_PRESS && !nKeyPressed)
    {
        m_NPCPlacementMode = !m_NPCPlacementMode;
        if (m_NPCPlacementMode)
        {
            m_EditNavigationMode = false; // Mutually exclusive modes
            m_ElevationEditMode = false;
            m_NoProjectionEditMode = false;
            m_YSortPlusEditMode = false;
            m_YSortMinusEditMode = false;
            m_ParticleZoneEditMode = false;
            m_StructureEditMode = false;
            m_AnimationEditMode = false;
            if (!m_AvailableNPCTypes.empty())
            {
                std::cout << "NPC placement mode: ON - Selected NPC: " << m_AvailableNPCTypes[m_SelectedNPCTypeIndex] << std::endl;
                std::cout << "Press , (comma) and . (period) to cycle through NPC types" << std::endl;
            }
        }
        else
        {
            std::cout << "NPC placement mode: OFF" << std::endl;
        }
        nKeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_N) == GLFW_RELEASE)
    {
        nKeyPressed = false;
    }

    // Toggles elevation editing mode. When active:
    //   - Left-click paints elevation values (for stairs)
    //   - Right-click removes elevation (sets to 0)
    //   - Use scroll to adjust elevation value
    static bool hKeyPressed = false;
    if (m_EditorMode && glfwGetKey(m_Window, GLFW_KEY_H) == GLFW_PRESS && !hKeyPressed)
    {
        m_ElevationEditMode = !m_ElevationEditMode;
        if (m_ElevationEditMode)
        {
            m_EditNavigationMode = false; // Mutually exclusive modes
            m_NPCPlacementMode = false;
            m_NoProjectionEditMode = false;
            m_YSortPlusEditMode = false;
            m_YSortMinusEditMode = false;
            m_ParticleZoneEditMode = false;
            m_StructureEditMode = false;
            m_AnimationEditMode = false;
            std::cout << "Elevation edit mode: ON - Current elevation: " << m_CurrentElevation << " pixels" << std::endl;
            std::cout << "Use scroll wheel to adjust elevation value" << std::endl;
        }
        else
        {
            std::cout << "Elevation edit mode: OFF" << std::endl;
        }
        hKeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_H) == GLFW_RELEASE)
    {
        hKeyPressed = false;
    }

    // Toggles no-projection editing mode. When active:
    //   - Left-click sets no-projection flag (tile renders without 3D effect)
    //   - Right-click clears no-projection flag
    //   - Used for buildings that should appear to have height in 3D mode
    static bool bKeyPressedNoProj = false;
    if (m_EditorMode && glfwGetKey(m_Window, GLFW_KEY_B) == GLFW_PRESS && !bKeyPressedNoProj)
    {
        m_NoProjectionEditMode = !m_NoProjectionEditMode;
        if (m_NoProjectionEditMode)
        {
            m_EditNavigationMode = false; // Mutually exclusive modes
            m_NPCPlacementMode = false;
            m_ElevationEditMode = false;
            m_YSortPlusEditMode = false;
            m_YSortMinusEditMode = false;
            m_ParticleZoneEditMode = false;
            m_StructureEditMode = false;
            m_AnimationEditMode = false;
            std::cout << "No-projection edit mode: ON (Layer " << m_CurrentLayer << ") - Click to mark tiles that bypass 3D projection" << std::endl;
            std::cout << "Use 1-6 keys to change layer" << std::endl;
        }
        else
        {
            std::cout << "No-projection edit mode: OFF" << std::endl;
        }
        bKeyPressedNoProj = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_B) == GLFW_RELEASE)
    {
        bKeyPressedNoProj = false;
    }

    // Toggles Y-sort-plus editing mode. When active:
    //   - Left-click sets Y-sort-plus flag (tile sorts with entities by Y position)
    //   - Right-click clears Y-sort-plus flag
    //   - Used for tiles that should appear in front/behind player based on Y
    static bool yKeyPressedYSort = false;
    if (m_EditorMode && glfwGetKey(m_Window, GLFW_KEY_Y) == GLFW_PRESS && !yKeyPressedYSort)
    {
        m_YSortPlusEditMode = !m_YSortPlusEditMode;
        if (m_YSortPlusEditMode)
        {
            m_EditNavigationMode = false; // Mutually exclusive modes
            m_NPCPlacementMode = false;
            m_ElevationEditMode = false;
            m_NoProjectionEditMode = false;
            m_YSortMinusEditMode = false;
            m_ParticleZoneEditMode = false;
            m_StructureEditMode = false;
            m_AnimationEditMode = false;
            std::cout << "Y-sort+1 edit mode: ON (Layer " << m_CurrentLayer << ") - Click to mark tiles for Y-sorting with entities" << std::endl;
            std::cout << "Use 1-6 keys to change layer" << std::endl;
        }
        else
        {
            std::cout << "Y-sort-plus edit mode: OFF" << std::endl;
        }
        yKeyPressedYSort = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_Y) == GLFW_RELEASE)
    {
        yKeyPressedYSort = false;
    }

    // Toggles Y-sort-minus editing mode. When active:
    //   - Left-click sets Y-sort-minus flag (tile renders in front of player at same Y)
    //   - Right-click clears Y-sort-minus flag
    //   - Only affects tiles that are already Y-sort-plus
    static bool oKeyPressedYSortMinus = false;
    if (m_EditorMode && glfwGetKey(m_Window, GLFW_KEY_O) == GLFW_PRESS && !oKeyPressedYSortMinus)
    {
        m_YSortMinusEditMode = !m_YSortMinusEditMode;
        if (m_YSortMinusEditMode)
        {
            m_EditNavigationMode = false; // Mutually exclusive modes
            m_NPCPlacementMode = false;
            m_ElevationEditMode = false;
            m_NoProjectionEditMode = false;
            m_YSortPlusEditMode = false;
            m_ParticleZoneEditMode = false;
            m_StructureEditMode = false;
            m_AnimationEditMode = false;
            std::cout << "========================================" << std::endl;
            std::cout << "Y-SORT-1 EDIT MODE: ON (Layer " << m_CurrentLayer << ")" << std::endl;
            std::cout << "Click the BOTTOM tile of a structure to mark it" << std::endl;
            std::cout << "(All tiles above will inherit the setting)" << std::endl;
            std::cout << "========================================" << std::endl;
        }
        else
        {
            std::cout << "Y-sort-minus edit mode: OFF" << std::endl;
        }
        oKeyPressedYSortMinus = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_O) == GLFW_RELEASE)
    {
        oKeyPressedYSortMinus = false;
    }

    // Toggles particle zone editing mode. When active:
    //   - Left-click and drag to create a particle zone
    //   - Right-click to remove zone under cursor
    //   - Use , and . keys to cycle particle type
    static bool jKeyPressedParticle = false;
    if (m_EditorMode && glfwGetKey(m_Window, GLFW_KEY_J) == GLFW_PRESS && !jKeyPressedParticle)
    {
        m_ParticleZoneEditMode = !m_ParticleZoneEditMode;
        if (m_ParticleZoneEditMode)
        {
            m_EditNavigationMode = false; // Mutually exclusive modes
            m_NPCPlacementMode = false;
            m_ElevationEditMode = false;
            m_NoProjectionEditMode = false;
            m_YSortPlusEditMode = false;
            m_YSortMinusEditMode = false;
            m_StructureEditMode = false;
            m_AnimationEditMode = false;
            const char *typeNames[] = {"Firefly", "Rain", "Snow", "Fog", "Sparkles", "Wisp", "Lantern", "Sunshine"};
            std::cout << "Particle zone edit mode: ON - Type: " << typeNames[static_cast<int>(m_CurrentParticleType)] << std::endl;
            std::cout << "Click and drag to place zones, use , and . to change type" << std::endl;
        }
        else
        {
            std::cout << "Particle zone edit mode: OFF" << std::endl;
        }
        jKeyPressedParticle = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_J) == GLFW_RELEASE)
    {
        jKeyPressedParticle = false;
    }

    // Particle type cycling
    if (m_EditorMode && m_ParticleZoneEditMode)
    {
        static bool commaParticle = false;
        static bool periodParticle = false;

        if (glfwGetKey(m_Window, GLFW_KEY_COMMA) == GLFW_PRESS && !commaParticle)
        {
            int type = static_cast<int>(m_CurrentParticleType);
            type = (type + 7) % 8; // Decrement with wrap-around
            m_CurrentParticleType = static_cast<ParticleType>(type);
            const char *typeNames[] = {"Firefly", "Rain", "Snow", "Fog", "Sparkles", "Wisp", "Lantern", "Sunshine"};
            std::cout << "Particle type: " << typeNames[type] << std::endl;
            commaParticle = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_COMMA) == GLFW_RELEASE)
            commaParticle = false;

        if (glfwGetKey(m_Window, GLFW_KEY_PERIOD) == GLFW_PRESS && !periodParticle)
        {
            int type = static_cast<int>(m_CurrentParticleType);
            type = (type + 1) % 8; // Next with wrap-around
            m_CurrentParticleType = static_cast<ParticleType>(type);
            const char *typeNames[] = {"Firefly", "Rain", "Snow", "Fog", "Sparkles", "Wisp", "Lantern", "Sunshine"};
            std::cout << "Particle type: " << typeNames[type] << std::endl;
            periodParticle = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_PERIOD) == GLFW_RELEASE)
            periodParticle = false;

        // Toggles manual noProjection override for new particle zones
        // Auto-detection from tiles is always active, this is for forcing noProjection on/off
        static bool nKeyParticle = false;
        if (glfwGetKey(m_Window, GLFW_KEY_N) == GLFW_PRESS && !nKeyParticle)
        {
            m_ParticleNoProjection = !m_ParticleNoProjection;
            std::cout << "Particle noProjection override: " << (m_ParticleNoProjection ? "ON (forced)" : "OFF (auto-detect)") << std::endl;
            nKeyParticle = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_N) == GLFW_RELEASE)
            nKeyParticle = false;
    }

    // Toggles structure definition mode. When active:
    //   - Click to place left anchor, click again to place right anchor
    //   - Enter to create structure from anchors
    //   - , and . to cycle through existing structures
    //   - Shift+click to assign tiles to current structure
    //   - Right-click to clear structure assignment from tiles
    //   - Delete to remove current structure
    static bool gKeyPressedStruct = false;
    if (m_EditorMode && glfwGetKey(m_Window, GLFW_KEY_G) == GLFW_PRESS && !gKeyPressedStruct)
    {
        m_StructureEditMode = !m_StructureEditMode;
        if (m_StructureEditMode)
        {
            m_EditNavigationMode = false;
            m_NPCPlacementMode = false;
            m_ElevationEditMode = false;
            m_NoProjectionEditMode = false;
            m_YSortPlusEditMode = false;
            m_YSortMinusEditMode = false;
            m_ParticleZoneEditMode = false;
            m_AnimationEditMode = false;
            m_PlacingAnchor = 0;
            m_TempLeftAnchor = glm::vec2(-1.0f, -1.0f);
            m_TempRightAnchor = glm::vec2(-1.0f, -1.0f);
            std::cout << "========================================" << std::endl;
            std::cout << "STRUCTURE EDIT MODE: ON (Layer " << (m_CurrentLayer + 1) << ")" << std::endl;
            std::cout << "Click = toggle no-projection" << std::endl;
            std::cout << "Shift+click = flood-fill no-projection" << std::endl;
            std::cout << "Ctrl+click = place anchors (left, then right)" << std::endl;
            std::cout << ", . = select existing structures" << std::endl;
            std::cout << "Delete = remove selected structure" << std::endl;
            std::cout << "Structures: " << m_Tilemap.GetNoProjectionStructureCount() << std::endl;
            std::cout << "========================================" << std::endl;
        }
        else
        {
            m_PlacingAnchor = 0;
            std::cout << "Structure edit mode: OFF" << std::endl;
        }
        gKeyPressedStruct = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_G) == GLFW_RELEASE)
    {
        gKeyPressedStruct = false;
    }

    // Structure mode controls
    if (m_EditorMode && m_StructureEditMode)
    {
        // Cycle through structures with , and .
        static bool commaPressed = false;
        static bool periodPressed = false;

        if (glfwGetKey(m_Window, GLFW_KEY_COMMA) == GLFW_PRESS && !commaPressed)
        {
            size_t count = m_Tilemap.GetNoProjectionStructureCount();
            if (count > 0)
            {
                if (m_CurrentStructureId < 0)
                    m_CurrentStructureId = static_cast<int>(count) - 1;
                else
                    m_CurrentStructureId = (m_CurrentStructureId - 1 + static_cast<int>(count)) % static_cast<int>(count);

                const NoProjectionStructure* s = m_Tilemap.GetNoProjectionStructure(m_CurrentStructureId);
                if (s)
                {
                    std::cout << "Selected structure " << m_CurrentStructureId << ": \"" << s->name
                              << "\" anchors: (" << s->leftAnchor.x << "," << s->leftAnchor.y << ") - ("
                              << s->rightAnchor.x << "," << s->rightAnchor.y << ")" << std::endl;
                }
            }
            commaPressed = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_COMMA) == GLFW_RELEASE)
            commaPressed = false;

        if (glfwGetKey(m_Window, GLFW_KEY_PERIOD) == GLFW_PRESS && !periodPressed)
        {
            size_t count = m_Tilemap.GetNoProjectionStructureCount();
            if (count > 0)
            {
                m_CurrentStructureId = (m_CurrentStructureId + 1) % static_cast<int>(count);

                const NoProjectionStructure* s = m_Tilemap.GetNoProjectionStructure(m_CurrentStructureId);
                if (s)
                {
                    std::cout << "Selected structure " << m_CurrentStructureId << ": \"" << s->name
                              << "\" anchors: (" << s->leftAnchor.x << "," << s->leftAnchor.y << ") - ("
                              << s->rightAnchor.x << "," << s->rightAnchor.y << ")" << std::endl;
                }
            }
            periodPressed = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_PERIOD) == GLFW_RELEASE)
            periodPressed = false;

        // Escape to cancel anchor placement
        static bool escapePressedAnchor = false;
        if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !escapePressedAnchor && m_PlacingAnchor != 0)
        {
            m_PlacingAnchor = 0;
            m_TempLeftAnchor = glm::vec2(-1.0f, -1.0f);
            m_TempRightAnchor = glm::vec2(-1.0f, -1.0f);
            std::cout << "Anchor placement cancelled" << std::endl;
            escapePressedAnchor = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_RELEASE)
            escapePressedAnchor = false;

        // Delete to remove current structure
        static bool deletePressedStruct = false;
        if (glfwGetKey(m_Window, GLFW_KEY_DELETE) == GLFW_PRESS && !deletePressedStruct)
        {
            if (m_CurrentStructureId >= 0)
            {
                std::cout << "Removed structure " << m_CurrentStructureId << std::endl;
                m_Tilemap.RemoveNoProjectionStructure(m_CurrentStructureId);
                m_CurrentStructureId = -1;
            }
            deletePressedStruct = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_DELETE) == GLFW_RELEASE)
            deletePressedStruct = false;

    }

    // Toggles animated tile creation mode. When active:
    //   - Click tiles in the tile picker to add frames to animation
    //   - Press Enter to create the animation and apply to selected map tile
    //   - Press Escape to cancel/clear frames
    //   - Use , and . to adjust frame duration
    static bool kKeyPressedAnim = false;
    if (m_EditorMode && glfwGetKey(m_Window, GLFW_KEY_K) == GLFW_PRESS && !kKeyPressedAnim)
    {
        m_AnimationEditMode = !m_AnimationEditMode;
        if (m_AnimationEditMode)
        {
            m_EditNavigationMode = false;
            m_NPCPlacementMode = false;
            m_ElevationEditMode = false;
            m_NoProjectionEditMode = false;
            m_YSortPlusEditMode = false;
            m_YSortMinusEditMode = false;
            m_ParticleZoneEditMode = false;
            m_StructureEditMode = false;
            m_AnimationFrames.clear();
            std::cout << "Animation edit mode: ON" << std::endl;
            std::cout << "Click tiles in picker to add frames, Enter to create, Esc to cancel" << std::endl;
            std::cout << "Left-click map to apply animation, Right-click to remove animation" << std::endl;
            std::cout << "Use , and . to adjust frame duration (current: " << m_AnimationFrameDuration << "s)" << std::endl;
        }
        else
        {
            m_AnimationFrames.clear();
            m_SelectedAnimationId = -1;
            std::cout << "Animation edit mode: OFF" << std::endl;
        }
        kKeyPressedAnim = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_K) == GLFW_RELEASE)
    {
        kKeyPressedAnim = false;
    }

    // Animation frame duration adjustment and controls
    if (m_EditorMode && m_AnimationEditMode)
    {
        static bool commaAnim = false;
        static bool periodAnim = false;

        if (glfwGetKey(m_Window, GLFW_KEY_COMMA) == GLFW_PRESS && !commaAnim)
        {
            m_AnimationFrameDuration = std::max(0.05f, m_AnimationFrameDuration - 0.05f);
            std::cout << "Animation frame duration: " << m_AnimationFrameDuration << "s" << std::endl;
            commaAnim = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_COMMA) == GLFW_RELEASE)
            commaAnim = false;

        if (glfwGetKey(m_Window, GLFW_KEY_PERIOD) == GLFW_PRESS && !periodAnim)
        {
            m_AnimationFrameDuration = std::min(2.0f, m_AnimationFrameDuration + 0.05f);
            std::cout << "Animation frame duration: " << m_AnimationFrameDuration << "s" << std::endl;
            periodAnim = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_PERIOD) == GLFW_RELEASE)
            periodAnim = false;

        // Escape to clear frames and deselect animation
        static bool escAnim = false;
        if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !escAnim)
        {
            m_AnimationFrames.clear();
            m_SelectedAnimationId = -1;
            std::cout << "Animation frames/selection cleared" << std::endl;
            escAnim = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_RELEASE)
            escAnim = false;

        // Enter to create animation
        static bool enterAnim = false;
        if (glfwGetKey(m_Window, GLFW_KEY_ENTER) == GLFW_PRESS && !enterAnim)
        {
            if (m_AnimationFrames.size() >= 2)
            {
                AnimatedTile anim(m_AnimationFrames, m_AnimationFrameDuration);
                int animId = m_Tilemap.AddAnimatedTile(anim);
                m_SelectedAnimationId = animId;
                std::cout << "Created animation #" << animId << " with " << m_AnimationFrames.size()
                          << " frames at " << m_AnimationFrameDuration << "s per frame" << std::endl;
                std::cout << "Click on map tiles to apply this animation (Esc to cancel)" << std::endl;
                m_AnimationFrames.clear();
                m_ShowTilePicker = false; // Close tile picker to allow map clicking
            }
            else
            {
                std::cout << "Need at least 2 frames to create animation" << std::endl;
            }
            enterAnim = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_ENTER) == GLFW_RELEASE)
            enterAnim = false;
    }

    // Cycles through available NPC types when in NPC placement mode.
    // Comma (,) previous type, Period (.) next type
    // Wraps around at list boundaries.
    if (m_EditorMode && m_NPCPlacementMode && !m_AvailableNPCTypes.empty())
    {
        static bool commaPressed = false;
        static bool periodPressed = false;

        // Comma key cycles to previous NPC type
        if (glfwGetKey(m_Window, GLFW_KEY_COMMA) == GLFW_PRESS && !commaPressed)
        {
            if (m_SelectedNPCTypeIndex > 0)
            {
                m_SelectedNPCTypeIndex--;
            }
            else
            {
                m_SelectedNPCTypeIndex = m_AvailableNPCTypes.size() - 1; // Wrap to end
            }
            std::cout << "Selected NPC type: " << m_AvailableNPCTypes[m_SelectedNPCTypeIndex]
                      << " (" << (m_SelectedNPCTypeIndex + 1) << "/" << m_AvailableNPCTypes.size() << ")" << std::endl;
            commaPressed = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_COMMA) == GLFW_RELEASE)
        {
            commaPressed = false;
        }

        // Period key cycles to next NPC type
        if (glfwGetKey(m_Window, GLFW_KEY_PERIOD) == GLFW_PRESS && !periodPressed)
        {
            m_SelectedNPCTypeIndex = (m_SelectedNPCTypeIndex + 1) % m_AvailableNPCTypes.size(); // Wrap to start
            std::cout << "Selected NPC type: " << m_AvailableNPCTypes[m_SelectedNPCTypeIndex]
                      << " (" << (m_SelectedNPCTypeIndex + 1) << "/" << m_AvailableNPCTypes.size() << ")" << std::endl;
            periodPressed = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_PERIOD) == GLFW_RELEASE)
        {
            periodPressed = false;
        }
    }

    // Resets camera zoom to 1.0x and recenters on player.
    // In editor mode, also resets tile picker zoom and pan.
    static bool zKeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_Z) == GLFW_PRESS && !zKeyPressed)
    {
        m_CameraZoom = 1.0f;
        std::cout << "Camera zoom reset to 1.0x" << std::endl;

        // Recenter camera on player in gameplay mode
        if (!m_EditorMode)
        {
            // Calculate viewport dimensions at 1.0x zoom
            float worldWidth = static_cast<float>(m_TilesVisibleWidth * 16);
            float worldHeight = static_cast<float>(m_TilesVisibleHeight * 16);

            // Calculate player's visual center
            glm::vec2 playerAnchorTileCenter = m_Player.GetCurrentTileCenter();
            glm::vec2 playerVisualCenter = glm::vec2(playerAnchorTileCenter.x, playerAnchorTileCenter.y - 16.0f);

            // Position camera so player is centered
            m_CameraPosition = playerVisualCenter - glm::vec2(worldWidth / 2.0f, worldHeight / 2.0f);

            // Clamp to map bounds (skip in editor free-camera mode)
            if (!(m_EditorMode && m_FreeCameraMode))
            {
                float mapWidth = static_cast<float>(m_Tilemap.GetMapWidth() * m_Tilemap.GetTileWidth());
                float mapHeight = static_cast<float>(m_Tilemap.GetMapHeight() * m_Tilemap.GetTileHeight());
                m_CameraPosition.x = std::max(0.0f, std::min(m_CameraPosition.x, mapWidth - worldWidth));
                m_CameraPosition.y = std::max(0.0f, std::min(m_CameraPosition.y, mapHeight - worldHeight));
            }

            // Disable smooth follow to prevent drift after reset
            m_HasCameraFollowTarget = false;
        }

        // Reset tile picker state in editor mode
        if (m_EditorMode)
        {
            m_TilePickerZoom = 2.0f;
            m_TilePickerOffsetX = 0.0f;
            m_TilePickerOffsetY = 0.0f;
            m_TilePickerTargetOffsetX = 0.0f;
            m_TilePickerTargetOffsetY = 0.0f;
            std::cout << "Tile picker zoom and offset reset to defaults" << std::endl;
        }
        zKeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_Z) == GLFW_RELEASE)
    {
        zKeyPressed = false;
    }

    // Toggle between OpenGL and Vulkan renderers at runtime
    static bool f1KeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_F1) == GLFW_PRESS && !f1KeyPressed)
    {
        // Toggle between OpenGL and Vulkan
        RendererAPI newApi = (m_RendererAPI == RendererAPI::OpenGL)
                                 ? RendererAPI::Vulkan
                                 : RendererAPI::OpenGL;
        SwitchRenderer(newApi);
        f1KeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_F1) == GLFW_RELEASE)
    {
        f1KeyPressed = false;
    }

    // Toggles FPS and position information display
    static bool f2KeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_F2) == GLFW_PRESS && !f2KeyPressed)
    {
        m_ShowDebugInfo = !m_ShowDebugInfo;
        std::cout << "Debug info display: " << (m_ShowDebugInfo ? "ON" : "OFF") << std::endl;
        f2KeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_F2) == GLFW_RELEASE)
    {
        f2KeyPressed = false;
    }

    // Enables visual debug overlays including:
    //   - Collision tiles
    //   - Player collision tolerance zones
    //   - Navigation tiles
    //   - NPC information
    //   - All tile layers visible
    static bool f3KeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_F3) == GLFW_PRESS && !f3KeyPressed)
    {
        m_DebugMode = !m_DebugMode;
        m_ShowNoProjectionAnchors = m_DebugMode; // Include anchor visualization in debug mode
        std::cout << "Debug mode: " << (m_DebugMode ? "ON" : "OFF") << std::endl;
        f3KeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_F3) == GLFW_RELEASE)
    {
        f3KeyPressed = false;
    }

    // Cycle through time of day: day -> evening -> night -> morning -> day...
    static bool f4KeyPressed = false;
    static int timeOfDayCycle = 0; // 0=day, 1=evening, 2=night, 3=morning
    if (glfwGetKey(m_Window, GLFW_KEY_F4) == GLFW_PRESS && !f4KeyPressed)
    {
        timeOfDayCycle = (timeOfDayCycle + 1) % 4;
        const char *periodName = "";
        switch (timeOfDayCycle)
        {
        case 0:
            m_TimeManager.SetTime(12.0f);
            periodName = "Day (12:00)";
            break;
        case 1:
            m_TimeManager.SetTime(20.0f);
            periodName = "Evening (20:00)";
            break;
        case 2:
            m_TimeManager.SetTime(0.0f);
            periodName = "Night (00:00)";
            break;
        case 3:
            m_TimeManager.SetTime(6.0f);
            periodName = "Morning (06:00)";
            break;
        }
        std::cout << "Time of day: " << periodName << std::endl;
        f4KeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_F4) == GLFW_RELEASE)
    {
        f4KeyPressed = false;
    }

    // Toggles the 3D globe effect for an isometric-like view
    static bool f5KeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_F5) == GLFW_PRESS && !f5KeyPressed)
    {
        Toggle3DEffect();
        f5KeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_F5) == GLFW_RELEASE)
    {
        f5KeyPressed = false;
    }

    // Toggle FPS cap (0 = uncapped, 60 = capped)
    static bool f6KeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_F6) == GLFW_PRESS && !f6KeyPressed)
    {
        if (m_TargetFps <= 0.0f)
        {
            m_TargetFps = 500.0f;
            std::cout << "FPS capped at 500" << std::endl;
        }
        else
        {
            m_TargetFps = 0.0f;
            std::cout << "FPS uncapped" << std::endl;
        }
        f6KeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_F6) == GLFW_RELEASE)
    {
        f6KeyPressed = false;
    }

    // Toggle free camera mode (Space) - camera stops following player
    // WASD/Arrows can then pan camera while player still moves with WASD
    static bool spaceKeyFreeCamera = false;
    if (!m_InDialogue && !m_DialogueManager.IsActive() && !m_EditorMode)
    {
        if (glfwGetKey(m_Window, GLFW_KEY_SPACE) == GLFW_PRESS && !spaceKeyFreeCamera)
        {
            m_FreeCameraMode = !m_FreeCameraMode;
            std::cout << "Free Camera Mode: " << (m_FreeCameraMode ? "ON" : "OFF") << std::endl;
            spaceKeyFreeCamera = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_SPACE) == GLFW_RELEASE)
        {
            spaceKeyFreeCamera = false;
        }
    }

    // Adjusts 3D effect parameters when enabled:
    //   - Page Up/Down adjusts globe radius and tilt
    static bool pageUpPressed = false;
    static bool pageDownPressed = false;
    if (m_Enable3DEffect)
    {
        // Globe effect parameter adjustment
        if (glfwGetKey(m_Window, GLFW_KEY_PAGE_UP) == GLFW_PRESS && !pageUpPressed)
        {
            m_GlobeSphereRadius = std::min(500.0f, m_GlobeSphereRadius + 10.0f);
            m_CameraTilt = std::max(0.0f, m_CameraTilt - 0.05f);
            std::cout << "3D Effect - Radius: " << m_GlobeSphereRadius << ", Tilt: " << m_CameraTilt << std::endl;
            pageUpPressed = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_PAGE_UP) == GLFW_RELEASE)
        {
            pageUpPressed = false;
        }

        if (glfwGetKey(m_Window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS && !pageDownPressed)
        {
            m_GlobeSphereRadius = std::max(50.0f, m_GlobeSphereRadius - 10.0f);
            m_CameraTilt = std::min(1.0f, m_CameraTilt + 0.05f);
            std::cout << "3D Effect - Radius: " << m_GlobeSphereRadius << ", Tilt: " << m_CameraTilt << std::endl;
            pageDownPressed = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_PAGE_DOWN) == GLFW_RELEASE)
        {
            pageDownPressed = false;
        }
    }

    // Saves the current game to save.json including:
    //   - All tile layers with rotations
    //   - Collision map
    //   - Navigation map
    //   - NPC positions, dialogues and types
    //   - Player spawn position and character type
    static bool sKeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_PRESS && !sKeyPressed && m_EditorMode)
    {
        // Calculate player's current tile for spawn point
        glm::vec2 playerPos = m_Player.GetPosition();
        int playerTileX = static_cast<int>(std::floor(playerPos.x / m_Tilemap.GetTileWidth()));
        int playerTileY = static_cast<int>(std::floor((playerPos.y - 0.1f) / m_Tilemap.GetTileHeight()));
        int characterType = static_cast<int>(m_Player.GetCharacterType());

        if (m_Tilemap.SaveMapToJSON("save.json", &m_NPCs, playerTileX, playerTileY, characterType))
        {
            std::cout << "Save successful! Player at tile (" << playerTileX << ", " << playerTileY << "), character type: " << characterType << std::endl;
        }
        sKeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_RELEASE)
    {
        sKeyPressed = false;
    }

    // Reloads the game state from save.json, replacing all current state.
    // Also restores player position, character type, and recenters camera.
    static bool lKeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_L) == GLFW_PRESS && !lKeyPressed && m_EditorMode)
    {
        int loadedPlayerTileX = -1;
        int loadedPlayerTileY = -1;
        int loadedCharacterType = -1;
        if (m_Tilemap.LoadMapFromJSON("save.json", &m_NPCs, &loadedPlayerTileX, &loadedPlayerTileY, &loadedCharacterType))
        {
            std::cout << "Save loaded successfully!" << std::endl;

            // Restore character type if saved
            if (loadedCharacterType >= 0)
            {
                m_Player.SwitchCharacter(static_cast<CharacterType>(loadedCharacterType));
                std::cout << "Player character restored to type " << loadedCharacterType << std::endl;
            }

            // Restore player position if spawn point was saved
            if (loadedPlayerTileX >= 0 && loadedPlayerTileY >= 0)
            {
                m_Player.SetTilePosition(loadedPlayerTileX, loadedPlayerTileY);

                // Recenter camera on player
                glm::vec2 playerPos = m_Player.GetPosition();
                float camWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
                float camWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
                glm::vec2 playerVisualCenter = glm::vec2(playerPos.x, playerPos.y - 16.0f);
                m_CameraPosition = playerVisualCenter - glm::vec2(camWorldWidth / 2.0f, camWorldHeight / 2.0f);
                m_CameraFollowTarget = m_CameraPosition;
                m_HasCameraFollowTarget = false;
                std::cout << "Player position restored to tile (" << loadedPlayerTileX << ", " << loadedPlayerTileY << ")" << std::endl;
            }
        }
        else
        {
            std::cout << "Failed to reload map!" << std::endl;
        }
        lKeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_L) == GLFW_RELEASE)
    {
        lKeyPressed = false;
    }

    // Removes tiles under the mouse cursor on the currently selected layer.
    // Hold DEL and drag to delete multiple tiles continuously.
    static bool deleteKeyHeld = false;
    static int lastDeletedTileX = -1;
    static int lastDeletedTileY = -1;

    if (glfwGetKey(m_Window, GLFW_KEY_DELETE) == GLFW_PRESS && m_EditorMode && !m_ShowTilePicker)
    {
        // Get cursor position in screen coordinates
        double mouseX, mouseY;
        glfwGetCursorPos(m_Window, &mouseX, &mouseY);

        // Calculate zoomed viewport dimensions
        float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * 16);
        float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * 16);
        float worldWidth = baseWorldWidth / m_CameraZoom;
        float worldHeight = baseWorldHeight / m_CameraZoom;

        // Transform screen -> world coordinates
        float worldX = (static_cast<float>(mouseX) / static_cast<float>(m_ScreenWidth)) * worldWidth + m_CameraPosition.x;
        float worldY = (static_cast<float>(mouseY) / static_cast<float>(m_ScreenHeight)) * worldHeight + m_CameraPosition.y;

        // Transform world -> tile coordinates
        int tileX = static_cast<int>(std::floor(worldX / m_Tilemap.GetTileWidth()));
        int tileY = static_cast<int>(std::floor(worldY / m_Tilemap.GetTileHeight()));

        // Only delete if cursor moved to a new tile
        bool isNewTile = (tileX != lastDeletedTileX || tileY != lastDeletedTileY);

        // Bounds check before deletion
        if (isNewTile && tileX >= 0 && tileX < m_Tilemap.GetMapWidth() &&
            tileY >= 0 && tileY < m_Tilemap.GetMapHeight())
        {
            // Delete tile on selected layer (set to -1 = empty) and clear animation
            m_Tilemap.SetLayerTile(tileX, tileY, m_CurrentLayer, -1);
            m_Tilemap.SetTileAnimation(tileX, tileY, static_cast<int>(m_CurrentLayer), -1);
            lastDeletedTileX = tileX;
            lastDeletedTileY = tileY;
        }
        deleteKeyHeld = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_DELETE) == GLFW_RELEASE)
    {
        deleteKeyHeld = false;
        lastDeletedTileX = -1;
        lastDeletedTileY = -1;
    }

    // Rotates the tile under the mouse cursor by 90 on the current layer.
    // Note: This is different from multi-tile rotation which uses R when
    //       m_MultiTileSelectionMode is true.
    static bool rKeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_R) == GLFW_PRESS && !rKeyPressed && m_EditorMode && !m_ShowTilePicker)
    {
        // Get mouse position
        double mouseX, mouseY;
        glfwGetCursorPos(m_Window, &mouseX, &mouseY);

        // Convert screen coordinates to world coordinates
        float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * 16);
        float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * 16);
        float worldWidth = baseWorldWidth / m_CameraZoom;
        float worldHeight = baseWorldHeight / m_CameraZoom;

        float worldX = (static_cast<float>(mouseX) / static_cast<float>(m_ScreenWidth)) * worldWidth + m_CameraPosition.x;
        float worldY = (static_cast<float>(mouseY) / static_cast<float>(m_ScreenHeight)) * worldHeight + m_CameraPosition.y;

        // Convert world coordinates to tile coordinates
        int tileX = static_cast<int>(std::floor(worldX / m_Tilemap.GetTileWidth()));
        int tileY = static_cast<int>(std::floor(worldY / m_Tilemap.GetTileHeight()));

        if (tileX >= 0 && tileX < m_Tilemap.GetMapWidth() &&
            tileY >= 0 && tileY < m_Tilemap.GetMapHeight())
        {
            // Rotate tile by 90 degrees on selected layer
            float currentRotation = m_Tilemap.GetLayerRotation(tileX, tileY, m_CurrentLayer);
            float newRotation = currentRotation + 90.0f;
            m_Tilemap.SetLayerRotation(tileX, tileY, m_CurrentLayer, newRotation);
            std::cout << "Rotated Layer " << (m_CurrentLayer + 1) << " tile at (" << tileX << ", " << tileY << ") to " << newRotation << " degrees" << std::endl;
        }
        rKeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_R) == GLFW_RELEASE)
    {
        rKeyPressed = false;
    }

    // Selects which tile layer to edit.
    static bool key1Pressed = false;
    static bool key2Pressed = false;
    static bool key3Pressed = false;
    static bool key4Pressed = false;
    static bool key5Pressed = false;
    static bool key6Pressed = false;
    static bool key7Pressed = false;
    static bool key8Pressed = false;
    static bool key9Pressed = false;
    static bool key0Pressed = false;

    // Layer switching: Keys 1-9,0 map to dynamic layers 0-9
    if (glfwGetKey(m_Window, GLFW_KEY_1) == GLFW_PRESS && !key1Pressed && m_EditorMode)
    {
        m_CurrentLayer = 0;
        std::cout << "Switched to Layer 1: Ground (background)" << std::endl;
        key1Pressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_1) == GLFW_RELEASE)
        key1Pressed = false;

    if (glfwGetKey(m_Window, GLFW_KEY_2) == GLFW_PRESS && !key2Pressed && m_EditorMode)
    {
        m_CurrentLayer = 1;
        std::cout << "Switched to Layer 2: Ground Detail (background)" << std::endl;
        key2Pressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_2) == GLFW_RELEASE)
        key2Pressed = false;

    if (glfwGetKey(m_Window, GLFW_KEY_3) == GLFW_PRESS && !key3Pressed && m_EditorMode)
    {
        m_CurrentLayer = 2;
        std::cout << "Switched to Layer 3: Objects (background)" << std::endl;
        key3Pressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_3) == GLFW_RELEASE)
        key3Pressed = false;

    if (glfwGetKey(m_Window, GLFW_KEY_4) == GLFW_PRESS && !key4Pressed && m_EditorMode)
    {
        m_CurrentLayer = 3;
        std::cout << "Switched to Layer 4: Objects2 (background)" << std::endl;
        key4Pressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_4) == GLFW_RELEASE)
        key4Pressed = false;

    if (glfwGetKey(m_Window, GLFW_KEY_5) == GLFW_PRESS && !key5Pressed && m_EditorMode)
    {
        m_CurrentLayer = 4;
        std::cout << "Switched to Layer 5: Objects3 (background)" << std::endl;
        key5Pressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_5) == GLFW_RELEASE)
        key5Pressed = false;

    if (glfwGetKey(m_Window, GLFW_KEY_6) == GLFW_PRESS && !key6Pressed && m_EditorMode)
    {
        m_CurrentLayer = 5;
        std::cout << "Switched to Layer 6: Foreground (foreground)" << std::endl;
        key6Pressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_6) == GLFW_RELEASE)
        key6Pressed = false;

    if (glfwGetKey(m_Window, GLFW_KEY_7) == GLFW_PRESS && !key7Pressed && m_EditorMode)
    {
        m_CurrentLayer = 6;
        std::cout << "Switched to Layer 7: Foreground2 (foreground)" << std::endl;
        key7Pressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_7) == GLFW_RELEASE)
        key7Pressed = false;

    if (glfwGetKey(m_Window, GLFW_KEY_8) == GLFW_PRESS && !key8Pressed && m_EditorMode)
    {
        m_CurrentLayer = 7;
        std::cout << "Switched to Layer 8: Overlay (foreground)" << std::endl;
        key8Pressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_8) == GLFW_RELEASE)
        key8Pressed = false;

    if (glfwGetKey(m_Window, GLFW_KEY_9) == GLFW_PRESS && !key9Pressed && m_EditorMode)
    {
        m_CurrentLayer = 8;
        std::cout << "Switched to Layer 9: Overlay2 (foreground)" << std::endl;
        key9Pressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_9) == GLFW_RELEASE)
        key9Pressed = false;

    if (glfwGetKey(m_Window, GLFW_KEY_0) == GLFW_PRESS && !key0Pressed && m_EditorMode)
    {
        m_CurrentLayer = 9;
        std::cout << "Switched to Layer 10: Overlay3 (foreground)" << std::endl;
        key0Pressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_0) == GLFW_RELEASE)
        key0Pressed = false;

    // Cycles through available player character sprites.
    // Each character type has its own sprite sheet loaded from assets.
    static bool cKeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_C) == GLFW_PRESS && !cKeyPressed)
    {
        CharacterType currentType = m_Player.GetCharacterType();
        CharacterType newType;

        // Cycle to next character type
        switch (currentType)
        {
        case CharacterType::BW1_MALE:
            newType = CharacterType::BW1_FEMALE;
            break;
        case CharacterType::BW1_FEMALE:
            newType = CharacterType::BW2_MALE;
            break;
        case CharacterType::BW2_MALE:
            newType = CharacterType::BW2_FEMALE;
            break;
        case CharacterType::BW2_FEMALE:
            newType = CharacterType::CC_FEMALE;
            break;
        case CharacterType::CC_FEMALE:
            newType = CharacterType::BW1_MALE; // Wrap to start
            break;
        default:
            newType = CharacterType::BW1_MALE;
            break;
        }

        // Attempt to load and switch to new character
        if (m_Player.SwitchCharacter(newType))
        {
            const char *name = "BW1_MALE";
            switch (newType)
            {
            case CharacterType::BW1_MALE:
                name = "BW1_MALE";
                break;
            case CharacterType::BW1_FEMALE:
                name = "BW1_FEMALE";
                break;
            case CharacterType::BW2_MALE:
                name = "BW2_MALE";
                break;
            case CharacterType::BW2_FEMALE:
                name = "BW2_FEMALE";
                break;
            case CharacterType::CC_FEMALE:
                name = "CC_FEMALE";
                break;
            }
            std::cout << "Character switched to: " << name << std::endl;
        }

        cKeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_C) == GLFW_RELEASE)
    {
        cKeyPressed = false;
    }

    // Toggles bicycle mode on/off. When bicycling:
    //   - Movement speed is 2.0x base speed
    //   - Uses center-only collision detection
    //   - Different sprite sheet may be used
    static bool bKeyPressed = false;
    if (glfwGetKey(m_Window, GLFW_KEY_B) == GLFW_PRESS && !bKeyPressed && !m_EditorMode)
    {
        bool currentBicycling = m_Player.IsBicycling();
        bool newBicycling = !currentBicycling;

        // Reset copied NPC appearance when starting to bicycle
        if (newBicycling && m_Player.IsUsingCopiedAppearance())
        {
            m_Player.RestoreOriginalAppearance();
            m_Player.UploadTextures(*m_Renderer);
        }

        m_Player.SetBicycling(newBicycling);
        std::cout << "Bicycle: " << (newBicycling ? "ON" : "OFF") << std::endl;
        bKeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_B) == GLFW_RELEASE)
    {
        bKeyPressed = false;
    }

    // Copies the appearance of a nearby NPC, transforming the player.
    // Press X again to restore original appearance.
    // Note: Running or bicycling will automatically restore original appearance
    //       since NPCs don't have running/bicycle sprites.
    static bool xKeyPressed = false;
    if (!m_EditorMode && !m_InDialogue && glfwGetKey(m_Window, GLFW_KEY_X) == GLFW_PRESS && !xKeyPressed)
    {
        if (m_Player.IsUsingCopiedAppearance())
        {
            // Restore original appearance
            m_Player.RestoreOriginalAppearance();
            m_Player.UploadTextures(*m_Renderer);
            std::cout << "Restored original appearance (X)" << std::endl;
        }
        else
        {
            // Try to copy appearance from nearby NPC
            glm::vec2 playerPos = m_Player.GetPosition();
            const float COPY_RANGE = 32.0f; // 2 tiles

            NonPlayerCharacter *nearestNPC = nullptr;
            float nearestDist = COPY_RANGE + 1.0f;

            for (auto &npc : m_NPCs)
            {
                glm::vec2 npcPos = npc.GetPosition();
                float dist = glm::length(npcPos - playerPos);
                if (dist < nearestDist && dist <= COPY_RANGE)
                {
                    nearestDist = dist;
                    nearestNPC = &npc;
                }
            }

            if (nearestNPC != nullptr)
            {
                std::string spritePath = nearestNPC->GetSpritePath();
                if (m_Player.CopyAppearanceFrom(spritePath))
                {
                    m_Player.UploadTextures(*m_Renderer);
                    std::cout << "Copied appearance from: " << nearestNPC->GetType() << " (X)" << std::endl;
                }
            }
            else
            {
                std::cout << "No NPC nearby to copy (X)" << std::endl;
            }
        }
        xKeyPressed = true;
    }
    // In debug mode, X key toggles corner cutting on the collision tile under cursor
    // The corner nearest to the mouse cursor within the tile is toggled
    if (m_DebugMode && glfwGetKey(m_Window, GLFW_KEY_X) == GLFW_PRESS && !xKeyPressed)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(m_Window, &mouseX, &mouseY);

        // Calculate world coordinates from mouse position
        float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
        float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
        float worldWidth = baseWorldWidth / m_CameraZoom;
        float worldHeight = baseWorldHeight / m_CameraZoom;

        float worldX = (static_cast<float>(mouseX) / static_cast<float>(m_ScreenWidth)) * worldWidth + m_CameraPosition.x;
        float worldY = (static_cast<float>(mouseY) / static_cast<float>(m_ScreenHeight)) * worldHeight + m_CameraPosition.y;

        int tileWidth = m_Tilemap.GetTileWidth();
        int tileHeight = m_Tilemap.GetTileHeight();
        int tileX = static_cast<int>(worldX / tileWidth);
        int tileY = static_cast<int>(worldY / tileHeight);

        if (tileX >= 0 && tileY >= 0 && tileX < m_Tilemap.GetMapWidth() && tileY < m_Tilemap.GetMapHeight())
        {
            if (m_Tilemap.GetTileCollision(tileX, tileY))
            {
                // Determine which corner is nearest to mouse position within the tile
                float localX = worldX - (tileX * tileWidth);
                float localY = worldY - (tileY * tileHeight);
                float halfTile = tileWidth * 0.5f;

                Tilemap::Corner corner;
                const char *cornerName;
                if (localX < halfTile && localY < halfTile)
                {
                    corner = Tilemap::CORNER_TL;
                    cornerName = "top-left";
                }
                else if (localX >= halfTile && localY < halfTile)
                {
                    corner = Tilemap::CORNER_TR;
                    cornerName = "top-right";
                }
                else if (localX < halfTile && localY >= halfTile)
                {
                    corner = Tilemap::CORNER_BL;
                    cornerName = "bottom-left";
                }
                else
                {
                    corner = Tilemap::CORNER_BR;
                    cornerName = "bottom-right";
                }

                bool currentlyBlocked = m_Tilemap.IsCornerCutBlocked(tileX, tileY, corner);
                m_Tilemap.SetCornerCutBlocked(tileX, tileY, corner, !currentlyBlocked);
                std::cout << "Corner cutting " << cornerName << " at (" << tileX << ", " << tileY << "): "
                          << (!currentlyBlocked ? "BLOCKED" : "ALLOWED") << std::endl;
            }
            else
            {
                std::cout << "Tile (" << tileX << ", " << tileY << ") has no collision - corner cutting N/A" << std::endl;
            }
        }
        xKeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_X) == GLFW_RELEASE)
    {
        xKeyPressed = false;
    }

    // Initiates dialogue with an NPC when
    //   1. Player is within INTERACTION_RANGE and
    //   2. NPC is in front of player or
    //   3. NPC hitbox is overlapping player hitbox
    static bool fKeyPressed = false;
    if (!m_EditorMode && !m_InDialogue && glfwGetKey(m_Window, GLFW_KEY_F) == GLFW_PRESS && !fKeyPressed)
    {
        glm::vec2 playerPos = m_Player.GetPosition();
        Direction playerDir = m_Player.GetDirection();

        // Calculate player's tile position
        const float EPS = 0.1f;
        int playerTileX = static_cast<int>(std::floor(playerPos.x / 16.0f));
        int playerTileY = static_cast<int>(std::floor((playerPos.y - EPS) / 16.0f));

        // Calculate tile position in front of player
        int frontTileX = playerTileX;
        int frontTileY = playerTileY;

        switch (playerDir)
        {
        case Direction::DOWN:
            frontTileY += 1;
            break;
        case Direction::UP:
            frontTileY -= 1;
            break;
        case Direction::LEFT:
            frontTileX -= 1;
            break;
        case Direction::RIGHT:
            frontTileX += 1;
            break;
        }

        // Interaction thresholds
        const float INTERACTION_RANGE = 32.0f;  // 2 tiles for easier interaction
        const float COLLISION_DISTANCE = 20.0f; // Very close = colliding

        // Hitbox dimensions for AABB collision
        const float PLAYER_HALF_W = 16.0f * 0.5f; // Player: 16x16 px hitbox
        const float PLAYER_BOX_H = 16.0f;
        const float NPC_HALF_W = 16.0f * 0.5f; // NPC: 16x16 px hitbox
        const float NPC_BOX_H = 16.0f;
        const float COLLISION_EPS = 0.05f; // Small margin for floating-point

        for (auto &npc : m_NPCs)
        {
            glm::vec2 npcPos = npc.GetPosition();
            float distance = glm::length(npcPos - playerPos);

            // Check if NPC is within interaction range
            if (distance <= INTERACTION_RANGE)
            {
                // Calculate NPC's tile position
                int npcTileX = static_cast<int>(std::floor(npcPos.x / 16.0f));
                int npcTileY = static_cast<int>(std::floor((npcPos.y - EPS) / 16.0f));

                // Check for AABB collision between player and NPC
                float playerMinX = playerPos.x - PLAYER_HALF_W + COLLISION_EPS;
                float playerMaxX = playerPos.x + PLAYER_HALF_W - COLLISION_EPS;
                float playerMaxY = playerPos.y - COLLISION_EPS;
                float playerMinY = playerPos.y - PLAYER_BOX_H + COLLISION_EPS;

                float npcMinX = npcPos.x - NPC_HALF_W + COLLISION_EPS;
                float npcMaxX = npcPos.x + NPC_HALF_W - COLLISION_EPS;
                float npcMaxY = npcPos.y - COLLISION_EPS;
                float npcMinY = npcPos.y - NPC_BOX_H + COLLISION_EPS;

                bool isColliding = (playerMinX < npcMaxX && playerMaxX > npcMinX &&
                                    playerMinY < npcMaxY && playerMaxY > npcMinY);

                // Check if NPC is on the tile in front of the player
                bool isOnFrontTile = (npcTileX == frontTileX && npcTileY == frontTileY);

                // Also check if NPC is on cardinal-adjacent tiles
                int tileDistX = std::abs(playerTileX - npcTileX);
                int tileDistY = std::abs(playerTileY - npcTileY);
                bool isCardinalAdjacent = (tileDistX == 1 && tileDistY == 0) || (tileDistX == 0 && tileDistY == 1);
                bool isSameTile = (tileDistX == 0 && tileDistY == 0);

                // Verify NPC is in the correct direction
                bool isInCorrectDirection = false;
                if (isCardinalAdjacent)
                {
                    switch (playerDir)
                    {
                    case Direction::DOWN:
                        isInCorrectDirection = (npcTileY > playerTileY && npcTileX == playerTileX);
                        break;
                    case Direction::UP:
                        isInCorrectDirection = (npcTileY < playerTileY && npcTileX == playerTileX);
                        break;
                    case Direction::LEFT:
                        isInCorrectDirection = (npcTileX < playerTileX && npcTileY == playerTileY);
                        break;
                    case Direction::RIGHT:
                        isInCorrectDirection = (npcTileX > playerTileX && npcTileY == playerTileY);
                        break;
                    }
                }

                // Check if NPC is very close and roughly in front
                bool isVeryClose = (distance <= COLLISION_DISTANCE);
                glm::vec2 toNPC = npcPos - playerPos;
                bool isRoughlyInFront = false;
                if (isVeryClose)
                {
                    // When very close, be more lenient with direction check
                    switch (playerDir)
                    {
                    case Direction::DOWN:
                        isRoughlyInFront = (toNPC.y > -8.0f); // NPC is below or at same level
                        break;
                    case Direction::UP:
                        isRoughlyInFront = (toNPC.y < 8.0f); // NPC is above or at same level
                        break;
                    case Direction::LEFT:
                        isRoughlyInFront = (toNPC.x < 8.0f); // NPC is to the left or at same level
                        break;
                    case Direction::RIGHT:
                        isRoughlyInFront = (toNPC.x > -8.0f); // NPC is to the right or at same level
                        break;
                    }
                }

                // Start dialogue if:
                // 1. NPC is colliding with player or
                // 2. NPC is on front tile or
                // 3. NPC is cardinal-adjacent in correct direction or
                // 4. NPC is very close and roughly in front
                if (isColliding || isOnFrontTile || isInCorrectDirection || (isVeryClose && isRoughlyInFront))
                {
                    // Check if NPC has a dialogue tree
                    if (npc.HasDialogueTree() && m_DialogueManager.StartDialogue(&npc))
                    {
                        // Using branching dialogue system
                        m_DialogueNPC = &npc;
                        m_DialoguePage = 0; // Reset pagination
                    }
                    else
                    {
                        // Fall back to simple dialogue
                        m_InDialogue = true;
                        m_DialogueNPC = &npc;
                        m_DialogueText = npc.GetDialogue();
                    }

                    // Get current positions
                    playerPos = m_Player.GetPosition();
                    npcPos = npc.GetPosition();

                    // Snap NPC to center of current tile
                    // X anchor is horizontally centered, so floor(x/16) already gives correct tile
                    // Y anchor is at bottom, so subtract 16 to find the tile the NPC stands on
                    int snapTileY = static_cast<int>(std::round((npcPos.y - 16.0f) / 16.0f));

                    // Pass true to preserve patrol route so NPC continues where it left off after dialogue
                    npc.SetTilePosition(npcTileX, snapTileY, 16, true);
                    npcPos = npc.GetPosition();

                    // Recalculate player tile position after getting fresh position
                    playerTileX = static_cast<int>(std::floor(playerPos.x / 16.0f));
                    playerTileY = static_cast<int>(std::floor((playerPos.y - EPS) / 16.0f));

                    // Use the new NPC tile coordinates for direction calculation
                    // This ensures we look for a spot relative to where the NPC ended up
                    npcTileY = snapTileY;

                    // Calculate direction from NPC to player
                    int dx = playerTileX - npcTileX;
                    int dy = playerTileY - npcTileY;

                    // If diagonal, snap to nearest cardinal direction
                    // Prefer the direction with larger absolute value
                    // TODO: Refactor, this logic is crazy messy
                    int finalDx = 0;
                    int finalDy = 0;
                    if (dx != 0 && dy != 0)
                    {
                        // Diagonal, snap to nearest cardinal
                        if (std::abs(dx) > std::abs(dy))
                        {
                            finalDx = (dx > 0) ? 1 : -1;
                            finalDy = 0;
                        }
                        else
                        {
                            finalDx = 0;
                            finalDy = (dy > 0) ? 1 : -1;
                        }
                    }
                    else if (dx != 0)
                    {
                        // Horizontal only
                        finalDx = (dx > 0) ? 1 : -1;
                        finalDy = 0;
                    }
                    else if (dy != 0)
                    {
                        // Vertical only
                        finalDx = 0;
                        finalDy = (dy > 0) ? 1 : -1;
                    }
                    else
                    {
                        // Same tile, default to down
                        finalDx = 0;
                        finalDy = 1;
                    }

                    // Find nearest tile (round instead of floor so player doesn't snap when slightly off-center)
                    int currentPlayerTileX = static_cast<int>(std::round((playerPos.x - 8.0f) / 16.0f));
                    int currentPlayerTileY = static_cast<int>(std::round((playerPos.y - 16.0f) / 16.0f));

                    // Check if player is already on a valid cardinal-adjacent tile
                    bool playerAlreadyValid = false;
                    if (currentPlayerTileX != npcTileX || currentPlayerTileY != npcTileY)
                    {
                        // Check if current position is valid
                        if (currentPlayerTileX >= 0 && currentPlayerTileY >= 0 &&
                            currentPlayerTileX < m_Tilemap.GetMapWidth() &&
                            currentPlayerTileY < m_Tilemap.GetMapHeight() &&
                            !m_Tilemap.GetTileCollision(currentPlayerTileX, currentPlayerTileY))
                        {
                            // Check if it's cardinal-adjacent to NPC
                            int tileDistX = std::abs(currentPlayerTileX - npcTileX);
                            int tileDistY = std::abs(currentPlayerTileY - npcTileY);
                            bool isCardinalAdjacent = (tileDistX == 1 && tileDistY == 0) || (tileDistX == 0 && tileDistY == 1);
                            if (isCardinalAdjacent)
                            {
                                playerAlreadyValid = true;
                            }
                        }
                    }

                    int playerTileXFinal = currentPlayerTileX;
                    int playerTileYFinal = currentPlayerTileY;

                    // Only snap if player is not already in a valid position
                    if (!playerAlreadyValid)
                    {
                        // Ensure finalDx and finalDy are not both zero
                        // This handles the edge case where player and NPC might be on same tile due to floating point precision
                        if (finalDx == 0 && finalDy == 0)
                        {
                            // Default to down if somehow on same tile
                            finalDx = 0;
                            finalDy = 1;
                        }

                        // Try to position player one tile away in the chosen direction
                        // If that position is invalid, try other cardinal directions
                        struct CardinalDir
                        {
                            int dx, dy;
                        };
                        CardinalDir cardinals[] = {
                            {finalDx, finalDy}, // Preferred direction
                            {0, 1},             // Down
                            {0, -1},            // Up
                            {1, 0},             // Right
                            {-1, 0}             // Left
                        };

                        bool foundValidPosition = false;

                        for (const auto &dir : cardinals)
                        {
                            int testTileX = npcTileX + dir.dx;
                            int testTileY = npcTileY + dir.dy;

                            // Safety check, never place player on NPC's tile
                            if (testTileX == npcTileX && testTileY == npcTileY)
                            {
                                continue;
                            }

                            // Check bounds
                            if (testTileX < 0 || testTileY < 0 ||
                                testTileX >= m_Tilemap.GetMapWidth() ||
                                testTileY >= m_Tilemap.GetMapHeight())
                            {
                                continue;
                            }

                            // Check collision
                            if (m_Tilemap.GetTileCollision(testTileX, testTileY))
                            {
                                continue;
                            }

                            // Valid position found
                            playerTileXFinal = testTileX;
                            playerTileYFinal = testTileY;
                            foundValidPosition = true;
                            break;
                        }

                        // If no valid position found, try the preferred direction
                        if (!foundValidPosition)
                        {
                            int safeTileX = npcTileX + finalDx;
                            int safeTileY = npcTileY + finalDy;

                            // Safety check, never place player on NPC's tile
                            if (safeTileX != npcTileX || safeTileY != npcTileY)
                            {
                                playerTileXFinal = safeTileX;
                                playerTileYFinal = safeTileY;
                            }
                            else
                            {
                                // Fallback, use down direction if preferred would place on same tile
                                playerTileXFinal = npcTileX;
                                playerTileYFinal = npcTileY + 1;
                            }
                        }

                        // Final safety check, never place player on NPC's tile
                        if (playerTileXFinal == npcTileX && playerTileYFinal == npcTileY)
                        {
                            // Emergency fallback, place one tile south
                            playerTileXFinal = npcTileX;
                            playerTileYFinal = npcTileY + 1;
                        }

                        // Snap player to the chosen tile position (ensures tile center)
                        m_Player.SetTilePosition(playerTileXFinal, playerTileYFinal);
                    }
                    else
                    {
                        // Player is already valid, but ensure they're exactly at tile center
                        m_Player.SetTilePosition(currentPlayerTileX, currentPlayerTileY);
                    }

                    // Stop player movement to prevent getting stuck
                    m_Player.Stop();

                    // Get fresh position after snapping to tile center
                    playerPos = m_Player.GetPosition();

                    // Make NPC face the player
                    glm::vec2 npcToPlayer = playerPos - npcPos;
                    if (std::abs(npcToPlayer.x) > std::abs(npcToPlayer.y))
                    {
                        // Horizontal direction
                        npc.SetDirection(npcToPlayer.x > 0 ? NPCDirection::RIGHT : NPCDirection::LEFT);
                    }
                    else
                    {
                        // Vertical direction
                        npc.SetDirection(npcToPlayer.y > 0 ? NPCDirection::DOWN : NPCDirection::UP);
                    }

                    // Make player face the NPC
                    glm::vec2 playerToNPC = npcPos - playerPos;
                    if (std::abs(playerToNPC.x) > std::abs(playerToNPC.y))
                    {
                        // Horizontal direction
                        m_Player.SetDirection(playerToNPC.x > 0 ? Direction::RIGHT : Direction::LEFT);
                    }
                    else
                    {
                        // Vertical direction
                        m_Player.SetDirection(playerToNPC.y > 0 ? Direction::DOWN : Direction::UP);
                    }

                    // Freeze both in place
                    npc.SetStopped(true);       // Lock NPC in place
                    npc.ResetAnimationToIdle(); // Reset NPC animation to idle frame
                    // Player movement is already disabled by m_InDialogue check in Update()

                    std::cout << "Started dialogue with NPC: " << npc.GetType()
                              << " at tile (" << npcTileX << ", " << npcTileY << ")"
                              << ", player at tile (" << playerTileXFinal << ", " << playerTileYFinal << ")" << std::endl;
                    break;
                }
            }
        }
        fKeyPressed = true;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_F) == GLFW_RELEASE)
    {
        fKeyPressed = false;
    }

    // Handle branching dialogue tree input
    if (m_DialogueManager.IsActive())
    {
        static bool upKeyPressed = false;
        static bool downKeyPressed = false;
        static bool enterKeyTree = false;
        static bool spaceKeyTree = false;
        static bool escapeKeyTree = false;

        // Navigate options with Up/Down or W/S
        if ((glfwGetKey(m_Window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(m_Window, GLFW_KEY_W) == GLFW_PRESS) && !upKeyPressed)
        {
            m_DialogueManager.SelectPrevious();
            upKeyPressed = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_UP) == GLFW_RELEASE && glfwGetKey(m_Window, GLFW_KEY_W) == GLFW_RELEASE)
        {
            upKeyPressed = false;
        }

        if ((glfwGetKey(m_Window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_PRESS) && !downKeyPressed)
        {
            m_DialogueManager.SelectNext();
            downKeyPressed = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_DOWN) == GLFW_RELEASE && glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_RELEASE)
        {
            downKeyPressed = false;
        }

        // Confirm selection with Enter or Space
        if (glfwGetKey(m_Window, GLFW_KEY_ENTER) == GLFW_PRESS && !enterKeyTree)
        {
            // Check if we need to advance pages first
            if (!IsDialogueOnLastPage())
            {
                m_DialoguePage++;
            }
            else
            {
                m_DialoguePage = 0; // Reset for next node
                m_DialogueManager.ConfirmSelection();
                // If dialogue ended, release NPC
                if (!m_DialogueManager.IsActive() && m_DialogueNPC)
                {
                    m_DialogueNPC->SetStopped(false);
                    m_DialogueNPC = nullptr;
                }
            }
            enterKeyTree = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_ENTER) == GLFW_RELEASE)
        {
            enterKeyTree = false;
        }

        if (glfwGetKey(m_Window, GLFW_KEY_SPACE) == GLFW_PRESS && !spaceKeyTree)
        {
            // Check if we need to advance pages first
            if (!IsDialogueOnLastPage())
            {
                m_DialoguePage++;
            }
            else
            {
                m_DialoguePage = 0; // Reset for next node
                m_DialogueManager.ConfirmSelection();
                if (!m_DialogueManager.IsActive() && m_DialogueNPC)
                {
                    m_DialogueNPC->SetStopped(false);
                    m_DialogueNPC = nullptr;
                }
            }
            spaceKeyTree = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_SPACE) == GLFW_RELEASE)
        {
            spaceKeyTree = false;
        }

        // Escape to force-close dialogue
        if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !escapeKeyTree)
        {
            m_DialogueManager.EndDialogue();
            m_DialoguePage = 0; // Reset pagination
            if (m_DialogueNPC)
            {
                m_DialogueNPC->SetStopped(false);
                m_DialogueNPC = nullptr;
            }
            escapeKeyTree = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_RELEASE)
        {
            escapeKeyTree = false;
        }
    }

    // Close simple dialogue
    if (m_InDialogue)
    {
        static bool enterKeyPressed = false;
        static bool spaceKeyPressed = false;
        static bool escapeKeyPressed = false;

        if (glfwGetKey(m_Window, GLFW_KEY_ENTER) == GLFW_PRESS && !enterKeyPressed)
        {
            m_InDialogue = false;
            if (m_DialogueNPC)
            {
                m_DialogueNPC->SetStopped(false);
            }
            m_DialogueNPC = nullptr;
            m_DialogueText = "";
            enterKeyPressed = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_ENTER) == GLFW_RELEASE)
        {
            enterKeyPressed = false;
        }

        if (glfwGetKey(m_Window, GLFW_KEY_SPACE) == GLFW_PRESS && !spaceKeyPressed)
        {
            m_InDialogue = false;
            if (m_DialogueNPC)
            {
                m_DialogueNPC->SetStopped(false);
            }
            m_DialogueNPC = nullptr;
            m_DialogueText = "";
            spaceKeyPressed = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_SPACE) == GLFW_RELEASE)
        {
            spaceKeyPressed = false;
        }

        if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !escapeKeyPressed)
        {
            m_InDialogue = false;
            if (m_DialogueNPC)
            {
                m_DialogueNPC->SetStopped(false);
            }
            m_DialogueNPC = nullptr;
            m_DialogueText = "";
            escapeKeyPressed = true;
        }
        if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_RELEASE)
        {
            escapeKeyPressed = false;
        }
    }

    // Only process player movement if not in editor mode and not in dialogue
    if (!m_EditorMode && !m_InDialogue && !m_DialogueManager.IsActive())
    {
        // Remember previous position for resolving collisions with NPCs
        m_PlayerPreviousPosition = m_Player.GetPosition();

        // Collect NPC positions for collision checking
        std::vector<glm::vec2> npcPositions;
        for (const auto &npc : m_NPCs)
        {
            npcPositions.push_back(npc.GetPosition());
        }

        m_Player.Move(moveDirection, deltaTime, &m_Tilemap, &npcPositions);
    }
    else if (m_InDialogue)
    {
        // Stop player movement during dialogue
        m_Player.Stop();
    }

    // Process mouse input for editor
    if (m_EditorMode)
    {
        ProcessMouseInput();
    }
}

void Game::ProcessMouseInput()
{
    double mouseX, mouseY;
    glfwGetCursorPos(m_Window, &mouseX, &mouseY);

    // Query mouse button states
    int leftMouseButton = glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_LEFT);
    int rightMouseButton = glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_RIGHT);
    bool leftMouseDown = (leftMouseButton == GLFW_PRESS);
    bool rightMouseDown = (rightMouseButton == GLFW_PRESS);

    // Right-click toggles collision or navigation flags depending on mode.
    // Supports drag-to-draw: first click sets target state, dragging applies it.
    if (rightMouseDown && !m_ShowTilePicker)
    {
        // Screen -> World coordinate transformation
        float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * 16);
        float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * 16);
        float worldWidth = baseWorldWidth / m_CameraZoom;
        float worldHeight = baseWorldHeight / m_CameraZoom;

        // Screen space uses top-left origin
        float worldX = (static_cast<float>(mouseX) / static_cast<float>(m_ScreenWidth)) * worldWidth + m_CameraPosition.x;
        float worldY = (static_cast<float>(mouseY) / static_cast<float>(m_ScreenHeight)) * worldHeight + m_CameraPosition.y;

        // World -> Tile coordinate transformation
        int tileX = static_cast<int>(std::floor(worldX / m_Tilemap.GetTileWidth()));
        int tileY = static_cast<int>(std::floor(worldY / m_Tilemap.GetTileHeight()));

        // Check if cursor moved to a new tile
        bool isNewNavigationTilePosition = (tileX != m_LastNavigationTileX || tileY != m_LastNavigationTileY);
        bool isNewCollisionTilePosition = (tileX != m_LastCollisionTileX || tileY != m_LastCollisionTileY);

        if (tileX >= 0 && tileX < m_Tilemap.GetMapWidth() &&
            tileY >= 0 && tileY < m_Tilemap.GetMapHeight())
        {
            // Animation edit mode, right-click removes animation from tile
            if (m_AnimationEditMode)
            {
                int currentAnim = m_Tilemap.GetTileAnimation(tileX, tileY, m_CurrentLayer);
                if (currentAnim >= 0)
                {
                    m_Tilemap.SetTileAnimation(tileX, tileY, m_CurrentLayer, -1);
                    std::cout << "Removed animation from tile (" << tileX << ", " << tileY << ") on layer " << m_CurrentLayer << std::endl;
                }
                m_RightMousePressed = true;
                return;
            }
            // Elevation edit mode, right-click clears elevation at tile
            else if (m_ElevationEditMode)
            {
                m_Tilemap.SetElevation(tileX, tileY, 0);
                std::cout << "Cleared elevation at (" << tileX << ", " << tileY << ")" << std::endl;
                m_RightMousePressed = true;
            }
            // Structure edit mode, right-click clears structure assignment from tiles
            // Shift+right-click, flood-fill to clear all connected tiles
            else if (m_StructureEditMode)
            {
                bool shiftHeld = (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                  glfwGetKey(m_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

                if (shiftHeld)
                {
                    // Flood-fill to clear structure assignment
                    int mapWidth = m_Tilemap.GetMapWidth();
                    int mapHeight = m_Tilemap.GetMapHeight();
                    std::vector<bool> visited(static_cast<size_t>(mapWidth * mapHeight), false);
                    std::vector<std::pair<int, int>> stack;
                    stack.push_back({tileX, tileY});
                    int count = 0;

                    while (!stack.empty())
                    {
                        auto [cx, cy] = stack.back();
                        stack.pop_back();

                        if (cx < 0 || cx >= mapWidth || cy < 0 || cy >= mapHeight)
                            continue;

                        size_t idx = static_cast<size_t>(cy * mapWidth + cx);
                        if (visited[idx])
                            continue;

                        // Check if tile has structure assignment on current layer
                        int structId = m_Tilemap.GetTileStructureId(cx, cy, m_CurrentLayer + 1);
                        if (structId < 0)
                            continue;

                        visited[idx] = true;
                        m_Tilemap.SetTileStructureId(cx, cy, m_CurrentLayer + 1, -1);
                        count++;

                        stack.push_back({cx - 1, cy});
                        stack.push_back({cx + 1, cy});
                        stack.push_back({cx, cy - 1});
                        stack.push_back({cx, cy + 1});
                    }
                    std::cout << "Cleared structure assignment from " << count << " tiles (layer " << (m_CurrentLayer + 1) << ")" << std::endl;
                }
                else
                {
                    // Single tile: clear structure assignment
                    m_Tilemap.SetTileStructureId(tileX, tileY, m_CurrentLayer + 1, -1);
                    std::cout << "Cleared structure assignment at (" << tileX << ", " << tileY << ")" << std::endl;
                }
                m_RightMousePressed = true;
            }
            // No-projection edit mode, right-click clears no-projection flag for current layer
            // Shift+right-click, flood-fill to clear all connected tiles
            else if (m_NoProjectionEditMode)
            {
                bool shiftHeld = (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                  glfwGetKey(m_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

                if (shiftHeld)
                {
                    int mapWidth = m_Tilemap.GetMapWidth();
                    int mapHeight = m_Tilemap.GetMapHeight();
                    std::vector<bool> visited(static_cast<size_t>(mapWidth * mapHeight), false);
                    std::vector<std::pair<int, int>> stack;
                    stack.push_back({tileX, tileY});
                    int count = 0;

                    while (!stack.empty())
                    {
                        auto [cx, cy] = stack.back();
                        stack.pop_back();

                        if (cx < 0 || cx >= mapWidth || cy < 0 || cy >= mapHeight)
                            continue;

                        size_t idx = static_cast<size_t>(cy * mapWidth + cx);
                        if (visited[idx])
                            continue;

                        // Check if tile has no-projection flag in ANY layer
                        bool hasNoProj = false;
                        for (size_t li = 0; li < m_Tilemap.GetLayerCount(); ++li)
                        {
                            if (m_Tilemap.GetLayerNoProjection(cx, cy, li))
                            {
                                hasNoProj = true;
                                break;
                            }
                        }
                        if (!hasNoProj)
                            continue;

                        visited[idx] = true;
                        // Clear noProjection on ALL layers at this position
                        for (size_t li = 0; li < m_Tilemap.GetLayerCount(); ++li)
                        {
                            m_Tilemap.SetLayerNoProjection(cx, cy, li, false);
                        }
                        count++;

                        stack.push_back({cx - 1, cy});
                        stack.push_back({cx + 1, cy});
                        stack.push_back({cx, cy - 1});
                        stack.push_back({cx, cy + 1});
                    }
                    std::cout << "Cleared no-projection on " << count << " connected tiles (all layers)" << std::endl;
                }
                else
                {
                    // Clear noProjection on ALL layers at this position
                    for (size_t li = 0; li < m_Tilemap.GetLayerCount(); ++li)
                    {
                        m_Tilemap.SetLayerNoProjection(tileX, tileY, li, false);
                    }
                    std::cout << "Cleared no-projection at (" << tileX << ", " << tileY << ") all layers" << std::endl;
                }
                m_RightMousePressed = true;
            }
            // Y-sort-plus edit mode, right-click clears Y-sort-plus flag for current layer
            // Shift+right-click, flood-fill to clear all connected tiles
            else if (m_YSortPlusEditMode)
            {
                bool shiftHeld = (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                  glfwGetKey(m_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

                if (shiftHeld)
                {
                    int mapWidth = m_Tilemap.GetMapWidth();
                    int mapHeight = m_Tilemap.GetMapHeight();
                    std::vector<bool> visited(static_cast<size_t>(mapWidth * mapHeight), false);
                    std::vector<std::pair<int, int>> stack;
                    stack.push_back({tileX, tileY});
                    int count = 0;

                    while (!stack.empty())
                    {
                        auto [cx, cy] = stack.back();
                        stack.pop_back();

                        if (cx < 0 || cx >= mapWidth || cy < 0 || cy >= mapHeight)
                            continue;

                        size_t idx = static_cast<size_t>(cy * mapWidth + cx);
                        if (visited[idx])
                            continue;

                        // Check if tile has Y-sort-plus flag in current layer
                        if (!m_Tilemap.GetLayerYSortPlus(cx, cy, m_CurrentLayer))
                            continue;

                        visited[idx] = true;
                        m_Tilemap.SetLayerYSortPlus(cx, cy, m_CurrentLayer, false);
                        count++;

                        stack.push_back({cx - 1, cy});
                        stack.push_back({cx + 1, cy});
                        stack.push_back({cx, cy - 1});
                        stack.push_back({cx, cy + 1});
                    }
                    std::cout << "Cleared Y-sort-plus on " << count << " connected tiles (layer " << (m_CurrentLayer + 1) << ")" << std::endl;
                }
                else
                {
                    m_Tilemap.SetLayerYSortPlus(tileX, tileY, m_CurrentLayer, false);
                    std::cout << "Cleared Y-sort-plus at (" << tileX << ", " << tileY << ") layer " << (m_CurrentLayer + 1) << std::endl;
                }
                m_RightMousePressed = true;
            }
            // Y-sort-minus edit mode, right-click clears Y-sort-minus flag for current layer
            // Shift+right-click, flood-fill to clear all connected tiles
            else if (m_YSortMinusEditMode)
            {
                bool shiftHeld = (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                  glfwGetKey(m_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

                if (shiftHeld)
                {
                    int mapWidth = m_Tilemap.GetMapWidth();
                    int mapHeight = m_Tilemap.GetMapHeight();
                    std::vector<bool> visited(static_cast<size_t>(mapWidth * mapHeight), false);
                    std::vector<std::pair<int, int>> stack;
                    stack.push_back({tileX, tileY});
                    int count = 0;

                    while (!stack.empty())
                    {
                        auto [cx, cy] = stack.back();
                        stack.pop_back();

                        if (cx < 0 || cx >= mapWidth || cy < 0 || cy >= mapHeight)
                            continue;

                        size_t idx = static_cast<size_t>(cy * mapWidth + cx);
                        if (visited[idx])
                            continue;

                        // Check if tile has Y-sort-minus flag in current layer
                        if (!m_Tilemap.GetLayerYSortMinus(cx, cy, m_CurrentLayer))
                            continue;

                        visited[idx] = true;
                        m_Tilemap.SetLayerYSortMinus(cx, cy, m_CurrentLayer, false);
                        count++;

                        stack.push_back({cx - 1, cy});
                        stack.push_back({cx + 1, cy});
                        stack.push_back({cx, cy - 1});
                        stack.push_back({cx, cy + 1});
                    }
                    std::cout << "Cleared Y-sort-minus on " << count << " connected tiles (layer " << (m_CurrentLayer + 1) << ")" << std::endl;
                }
                else
                {
                    m_Tilemap.SetLayerYSortMinus(tileX, tileY, m_CurrentLayer, false);
                    std::cout << "Cleared Y-sort-minus at (" << tileX << ", " << tileY << ") layer " << (m_CurrentLayer + 1) << std::endl;
                }
                m_RightMousePressed = true;
            }
            // Particle zone edit mode, right-click removes zone under cursor
            else if (m_ParticleZoneEditMode)
            {
                // Find zone under cursor and remove it
                auto *zones = m_Tilemap.GetParticleZonesMutable();
                for (size_t i = 0; i < zones->size(); ++i)
                {
                    const ParticleZone &zone = (*zones)[i];
                    if (worldX >= zone.position.x && worldX < zone.position.x + zone.size.x &&
                        worldY >= zone.position.y && worldY < zone.position.y + zone.size.y)
                    {
                        const char *typeNames[] = {"Firefly", "Rain", "Snow", "Fog", "Sparkles", "Wisp", "Lantern"};
                        std::cout << "Removed " << typeNames[static_cast<int>(zone.type)]
                                  << " zone at (" << zone.position.x << ", " << zone.position.y << ")" << std::endl;
                        m_Particles.OnZoneRemoved(static_cast<int>(i));
                        m_Tilemap.RemoveParticleZone(i);
                        break;
                    }
                }
                m_RightMousePressed = true;
            }
            else if (m_EditNavigationMode)
            {
                // Navigation editing mode, support drag-to-draw
                bool navigationChanged = false;
                if (!m_RightMousePressed)
                {
                    // Initial click determines target state
                    bool walkable = m_Tilemap.GetNavigation(tileX, tileY);
                    m_NavigationDragState = !walkable; // Set to opposite of current state
                    m_Tilemap.SetNavigation(tileX, tileY, m_NavigationDragState);
                    navigationChanged = true;
                    std::cout << "=== NAVIGATION DRAG START ===" << std::endl;
                    std::cout << "Tile (" << tileX << ", " << tileY << "): "
                              << (walkable ? "ON" : "OFF") << " -> "
                              << (m_NavigationDragState ? "ON" : "OFF") << std::endl;
                    m_LastNavigationTileX = tileX;
                    m_LastNavigationTileY = tileY;
                    m_RightMousePressed = true;
                }
                else if (isNewNavigationTilePosition)
                {
                    // Dragging sets navigation to the same state as initial click
                    bool currentWalkable = m_Tilemap.GetNavigation(tileX, tileY);
                    if (currentWalkable != m_NavigationDragState)
                    {
                        m_Tilemap.SetNavigation(tileX, tileY, m_NavigationDragState);
                        navigationChanged = true;
                        std::cout << "Navigation drag: Tile (" << tileX << ", " << tileY << ") -> "
                                  << (m_NavigationDragState ? "ON" : "OFF") << std::endl;
                    }
                    m_LastNavigationTileX = tileX;
                    m_LastNavigationTileY = tileY;
                }

                // Recalculate patrol routes when navigation changes
                if (navigationChanged)
                {
                    RecalculateNPCPatrolRoutes();
                }
            }
            else
            {
                // Collision editing mode, support drag-to-draw
                if (!m_RightMousePressed)
                {
                    // Initial click determines target state
                    bool currentCollision = m_Tilemap.GetTileCollision(tileX, tileY);
                    m_CollisionDragState = !currentCollision; // Set to opposite of current state
                    m_Tilemap.SetTileCollision(tileX, tileY, m_CollisionDragState);
                    std::cout << "=== COLLISION DRAG START ===" << std::endl;
                    std::cout << "Tile (" << tileX << ", " << tileY << "): "
                              << (currentCollision ? "ON" : "OFF") << " -> "
                              << (m_CollisionDragState ? "ON" : "OFF") << std::endl;
                    m_LastCollisionTileX = tileX;
                    m_LastCollisionTileY = tileY;
                    m_RightMousePressed = true;
                }
                else if (isNewCollisionTilePosition)
                {
                    // Dragging sets collision to the same state as initial click
                    bool currentCollision = m_Tilemap.GetTileCollision(tileX, tileY);
                    if (currentCollision != m_CollisionDragState)
                    {
                        m_Tilemap.SetTileCollision(tileX, tileY, m_CollisionDragState);
                        std::cout << "Collision drag: Tile (" << tileX << ", " << tileY << ") -> "
                                  << (m_CollisionDragState ? "ON" : "OFF") << std::endl;
                    }
                    m_LastCollisionTileX = tileX;
                    m_LastCollisionTileY = tileY;
                }
            }
        }
        else
        {
            if (!m_RightMousePressed)
            {
                std::cout << "Right-click outside map bounds (tileX=" << tileX << " tileY=" << tileY
                          << " map size=" << m_Tilemap.GetMapWidth() << "x" << m_Tilemap.GetMapHeight() << ")" << std::endl;
            }
        }
    }
    else if (!rightMouseDown)
    {
        m_RightMousePressed = false;
        // Reset navigation and collision drag tracking when mouse is released
        m_LastNavigationTileX = -1;
        m_LastNavigationTileY = -1;
        m_LastCollisionTileX = -1;
        m_LastCollisionTileY = -1;
    }

    // Handle tile picker selection
    if (m_ShowTilePicker)
    {
        int dataTilesPerRow = m_Tilemap.GetTilesetDataWidth() / m_Tilemap.GetTileWidth();
        int dataTilesPerCol = m_Tilemap.GetTilesetDataHeight() / m_Tilemap.GetTileHeight();
        int totalTiles = dataTilesPerRow * dataTilesPerCol;
        int tilesPerRow = dataTilesPerRow;
        float baseTileSize = (static_cast<float>(m_ScreenWidth) / static_cast<float>(tilesPerRow)) * 1.5f;
        float tileSize = baseTileSize * m_TilePickerZoom;

        // Start selection on mouse down
        if (leftMouseDown && !m_MousePressed && !m_IsSelectingTiles)
        {
            if (mouseX >= 0 && mouseX < m_ScreenWidth && mouseY >= 0 && mouseY < m_ScreenHeight)
            {
                // Account for offset when calculating tile position
                double adjustedMouseX = mouseX - m_TilePickerOffsetX;
                double adjustedMouseY = mouseY - m_TilePickerOffsetY;
                int pickerTileX = static_cast<int>(adjustedMouseX / tileSize);
                int pickerTileY = static_cast<int>(adjustedMouseY / tileSize);
                int localIndex = pickerTileY * tilesPerRow + pickerTileX;
                int clickedTileID = localIndex;

                if (clickedTileID >= 0 && clickedTileID < totalTiles)
                {
                    // Animation edit mode, collect frames instead of normal selection
                    if (m_AnimationEditMode)
                    {
                        // Add frame to animation
                        m_AnimationFrames.push_back(clickedTileID);
                        m_MousePressed = true;
                        std::cout << "Added animation frame: " << clickedTileID << " (total frames: " << m_AnimationFrames.size() << ")" << std::endl;
                    }
                    else
                    {
                        m_IsSelectingTiles = true;
                        m_SelectionStartTileID = clickedTileID;
                        m_SelectedTileID = clickedTileID;
                        m_MousePressed = true; // Prevent other click handlers from firing
                        std::cout << "Started selection at tile ID: " << clickedTileID << " (mouse: " << mouseX << ", " << mouseY
                                  << ", adjusted: " << adjustedMouseX << ", " << adjustedMouseY << ", offset: "
                                  << m_TilePickerOffsetX << ", " << m_TilePickerOffsetY << ")" << std::endl;
                    }
                }
            }
        }

        // Update selection while dragging
        if (leftMouseDown && m_IsSelectingTiles)
        {
            if (mouseX >= 0 && mouseX < m_ScreenWidth && mouseY >= 0 && mouseY < m_ScreenHeight)
            {
                // Account for offset when calculating tile position
                double adjustedMouseX = mouseX - m_TilePickerOffsetX;
                double adjustedMouseY = mouseY - m_TilePickerOffsetY;
                int pickerTileX = static_cast<int>(adjustedMouseX / tileSize);
                int pickerTileY = static_cast<int>(adjustedMouseY / tileSize);
                int localIndex = pickerTileY * tilesPerRow + pickerTileX;
                int clickedTileID = localIndex;

                if (clickedTileID >= 0 && clickedTileID < totalTiles)
                {
                    m_SelectedTileID = clickedTileID;
                }
            }
        }

        // Reset mouse pressed state when mouse released in animation mode
        if (!leftMouseDown && m_AnimationEditMode && m_MousePressed)
        {
            m_MousePressed = false;
        }

        // Finish selection on mouse up
        if (!leftMouseDown && m_IsSelectingTiles)
        {
            if (m_SelectionStartTileID >= 0)
            {
                int startTileID = m_SelectionStartTileID;
                int endTileID = m_SelectedTileID;

                int startX = startTileID % dataTilesPerRow;
                int startY = startTileID / dataTilesPerRow;
                int endX = endTileID % dataTilesPerRow;
                int endY = endTileID / dataTilesPerRow;

                int minX = std::min(startX, endX);
                int maxX = std::max(startX, endX);
                int minY = std::min(startY, endY);
                int maxY = std::max(startY, endY);

                m_SelectedTileStartID = minY * dataTilesPerRow + minX;
                m_SelectedTileWidth = maxX - minX + 1;
                m_SelectedTileHeight = maxY - minY + 1;

                if (m_SelectedTileWidth > 1 || m_SelectedTileHeight > 1)
                {
                    // Multi-tile selection, enable placement mode,
                    // but do not change the world camera or zoom.
                    m_MultiTileSelectionMode = true;
                    m_IsPlacingMultiTile = true;
                    m_MultiTileRotation = 0; // Reset rotation for new selection
                    std::cout << "=== MULTI-TILE SELECTION ===" << std::endl;
                    std::cout << "Start tile ID: " << m_SelectedTileStartID << std::endl;
                    std::cout << "Size: " << m_SelectedTileWidth << "x" << m_SelectedTileHeight << std::endl;
                }
                else
                {
                    m_MultiTileSelectionMode = false;
                    m_IsPlacingMultiTile = false;
                    m_MultiTileRotation = 0; // Reset rotation
                    std::cout << "=== SINGLE TILE SELECTION ===" << std::endl;
                    std::cout << "Tile ID: " << m_SelectedTileStartID << std::endl;
                }

                m_ShowTilePicker = false;
            }
            m_IsSelectingTiles = false;
            m_SelectionStartTileID = -1;
            m_MousePressed = false; // Reset mouse pressed state
        }

        // Early return to prevent tile placement when tile picker is shown
        if (m_ShowTilePicker)
        {
            // Update mouse position for preview
            m_LastMouseX = mouseX;
            m_LastMouseY = mouseY;
            return; // Don't process tile placement when picker is shown
        }
    }

    // Handle left mouse click
    if (leftMouseDown && !m_ShowTilePicker)
    {
        // Convert screen coordinates to world coordinates
        float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * 16);
        float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * 16);
        float worldWidth = baseWorldWidth / m_CameraZoom;
        float worldHeight = baseWorldHeight / m_CameraZoom;

        float worldX = (static_cast<float>(mouseX) / static_cast<float>(m_ScreenWidth)) * worldWidth + m_CameraPosition.x;
        float worldY = (static_cast<float>(mouseY) / static_cast<float>(m_ScreenHeight)) * worldHeight + m_CameraPosition.y;

        int tileX = static_cast<int>(std::floor(worldX / m_Tilemap.GetTileWidth()));
        int tileY = static_cast<int>(std::floor(worldY / m_Tilemap.GetTileHeight()));

        // NPC placement mode, toggle NPC on this tile instead of placing tiles
        if (m_EditorMode && m_NPCPlacementMode)
        {
            if (tileX >= 0 && tileX < m_Tilemap.GetMapWidth() &&
                tileY >= 0 && tileY < m_Tilemap.GetMapHeight())
            {
                // Only process if this is a new tile
                if (tileX == m_LastNPCPlacementTileX && tileY == m_LastNPCPlacementTileY)
                {
                    return; // Already processed this tile during this click
                }
                m_LastNPCPlacementTileX = tileX;
                m_LastNPCPlacementTileY = tileY;

                const int tileSize = m_Tilemap.GetTileWidth();

                // First, try to remove any NPC at this tile (works on any tile)
                bool removed = false;
                for (auto it = m_NPCs.begin(); it != m_NPCs.end(); ++it)
                {
                    if (it->GetTileX() == tileX && it->GetTileY() == tileY)
                    {
                        m_NPCs.erase(it);
                        removed = true;
                        std::cout << "Removed NPC at tile (" << tileX << ", " << tileY << ")" << std::endl;
                        break;
                    }
                }

                // Only place new NPCs on navigation tiles
                if (!removed && m_Tilemap.GetNavigation(tileX, tileY))
                {
                    if (!m_AvailableNPCTypes.empty())
                    {
                        NonPlayerCharacter npc;
                        std::string npcType = m_AvailableNPCTypes[m_SelectedNPCTypeIndex];
                        if (npc.Load(npcType))
                        {
                            npc.SetTilePosition(tileX, tileY, tileSize);

                            // Randomly assign one of several mystery-themed dialogue trees
                            // TODO: Load from save.json only and create dialogues via editor
                            DialogueTree tree;
                            std::string npcName;
                            int mysteryType = rand() % 5;

                            if (mysteryType == 0)
                            {
                                // UFO sighting mystery
                                tree.id = "ufo_sighting";
                                tree.startNodeId = "start";
                                npcName = "Anna";

                                DialogueNode startNode;
                                startNode.id = "start";
                                startNode.speaker = npcName;
                                startNode.text = "Please, you have to help me! My brother went to investigate strange lights in the northern field three nights ago. He hasn't come back.";
                                DialogueOption askLightsOpt("Strange lights?", "lights");
                                askLightsOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_ufo_quest"));
                                startNode.options.push_back(askLightsOpt);
                                DialogueOption cantHelpOpt("I'm sorry, I can't help.", "");
                                cantHelpOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_ufo_quest"));
                                startNode.options.push_back(cantHelpOpt);
                                DialogueOption updateOpt("Any news about your brother?", "update");
                                updateOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_SET, "accepted_ufo_quest"));
                                startNode.options.push_back(updateOpt);
                                tree.AddNode(startNode);

                                DialogueNode lightsNode;
                                lightsNode.id = "lights";
                                lightsNode.speaker = npcName;
                                lightsNode.text = "Green lights, hovering in the sky. People say it's a UFO. Others have gone missing too. Will you look for him?";
                                DialogueOption questOpt("I'll find your brother.", "accept");
                                questOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_ufo_quest"));
                                questOpt.consequences.push_back(DialogueConsequence(DialogueConsequence::Type::SET_FLAG_VALUE, "accepted_ufo_quest", "Find Anna's missing brother in the northern field!"));
                                lightsNode.options.push_back(questOpt);
                                lightsNode.options.push_back(DialogueOption("That sounds too dangerous.", ""));
                                tree.AddNode(lightsNode);

                                DialogueNode acceptNode;
                                acceptNode.id = "accept";
                                acceptNode.speaker = npcName;
                                acceptNode.text = "Thank you! The field is north of town. Please be careful, and bring him home safe.";
                                acceptNode.options.push_back(DialogueOption("I'll do my best.", ""));
                                tree.AddNode(acceptNode);

                                DialogueNode updateNode;
                                updateNode.id = "update";
                                updateNode.speaker = npcName;
                                updateNode.text = "Have you found him? Please, the northern field... that's where he went. I can't lose him.";
                                updateNode.options.push_back(DialogueOption("I'm still looking.", ""));
                                tree.AddNode(updateNode);
                            }
                            else if (mysteryType == 1)
                            {
                                // Bigfoot/cryptid sighting mystery
                                tree.id = "bigfoot_sighting";
                                tree.startNodeId = "start";
                                npcName = "Mona";

                                DialogueNode startNode;
                                startNode.id = "start";
                                startNode.speaker = npcName;
                                startNode.text = "I know what I saw. Eight feet tall, covered in fur, walking upright through the forest. Everyone thinks I'm crazy.";
                                DialogueOption askMoreOpt("Tell me more about what you saw.", "details");
                                askMoreOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_bigfoot_quest"));
                                startNode.options.push_back(askMoreOpt);
                                DialogueOption dismissOpt("Probably just a bear.", "");
                                dismissOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_bigfoot_quest"));
                                startNode.options.push_back(dismissOpt);
                                DialogueOption updateOpt("Found any more evidence?", "update");
                                updateOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_SET, "accepted_bigfoot_quest"));
                                startNode.options.push_back(updateOpt);
                                tree.AddNode(startNode);

                                DialogueNode detailsNode;
                                detailsNode.id = "details";
                                detailsNode.speaker = npcName;
                                detailsNode.text = "It left tracks, huge ones, near the old mill. I found tufts of hair too. Something's out there. Will you help me prove it?";
                                DialogueOption questOpt("I'll investigate the old mill.", "accept");
                                questOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_bigfoot_quest"));
                                questOpt.consequences.push_back(DialogueConsequence(DialogueConsequence::Type::SET_FLAG_VALUE, "accepted_bigfoot_quest", "Investigate the strange tracks near the old mill."));
                                detailsNode.options.push_back(questOpt);
                                detailsNode.options.push_back(DialogueOption("I'd rather not get involved.", ""));
                                tree.AddNode(detailsNode);

                                DialogueNode acceptNode;
                                acceptNode.id = "accept";
                                acceptNode.speaker = npcName;
                                acceptNode.text = "Finally, someone who believes me! The mill is east of here. Look for broken branches and disturbed earth. And be careful.";
                                acceptNode.options.push_back(DialogueOption("I'll see what I can find.", ""));
                                tree.AddNode(acceptNode);

                                DialogueNode updateNode;
                                updateNode.id = "update";
                                updateNode.speaker = npcName;
                                updateNode.text = "Any luck at the mill? I've been hearing strange howls at night. Something's definitely out there.";
                                updateNode.options.push_back(DialogueOption("Still investigating.", ""));
                                tree.AddNode(updateNode);
                            }
                            else if (mysteryType == 2)
                            {
                                // Haunted house mystery
                                tree.id = "haunted_manor";
                                tree.startNodeId = "start";
                                npcName = "Eleanor";

                                DialogueNode startNode;
                                startNode.id = "start";
                                startNode.speaker = npcName;
                                startNode.text = "The Blackwood Manor has been abandoned for decades. But lately... I've seen lights in the windows. And heard music. Piano music.";
                                DialogueOption askMoreOpt("That does sound strange.", "details");
                                askMoreOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_ghost_quest"));
                                startNode.options.push_back(askMoreOpt);
                                DialogueOption dismissOpt("Probably just squatters.", "");
                                dismissOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_ghost_quest"));
                                startNode.options.push_back(dismissOpt);
                                DialogueOption updateOpt("I went to the manor...", "update");
                                updateOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_SET, "accepted_ghost_quest"));
                                startNode.options.push_back(updateOpt);
                                tree.AddNode(startNode);

                                DialogueNode detailsNode;
                                detailsNode.id = "details";
                                detailsNode.speaker = npcName;
                                detailsNode.text = "The Blackwoods all died in a fire fifty years ago. The piano burned with them. Yet I hear it playing every midnight. Will you find out what's happening?";
                                DialogueOption questOpt("I'll investigate the manor.", "accept");
                                questOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_ghost_quest"));
                                questOpt.consequences.push_back(DialogueConsequence(DialogueConsequence::Type::SET_FLAG_VALUE, "accepted_ghost_quest", "Investigate the strange occurrences at Blackwood Manor."));
                                detailsNode.options.push_back(questOpt);
                                detailsNode.options.push_back(DialogueOption("I don't believe in ghosts.", ""));
                                tree.AddNode(detailsNode);

                                DialogueNode acceptNode;
                                acceptNode.id = "accept";
                                acceptNode.speaker = npcName;
                                acceptNode.text = "Bless you. The manor is on the hill west of town. Go at midnight if you want to hear the music. But don't say I didn't warn you.";
                                acceptNode.options.push_back(DialogueOption("I'll be careful.", ""));
                                tree.AddNode(acceptNode);

                                DialogueNode updateNode;
                                updateNode.id = "update";
                                updateNode.speaker = npcName;
                                updateNode.text = "Did you hear it? The piano? Some say it's Lady Blackwood, still playing for her children. They never found her body in the fire...";
                                updateNode.options.push_back(DialogueOption("I need to look deeper.", ""));
                                tree.AddNode(updateNode);
                            }
                            else if (mysteryType == 3)
                            {
                                // Bermuda Triangle-style sea mystery
                                tree.id = "sea_vanishings";
                                tree.startNodeId = "start";
                                npcName = "Claire";

                                DialogueNode startNode;
                                startNode.id = "start";
                                startNode.speaker = npcName;
                                startNode.text = "Three ships. Three ships vanished in the same waters this month. No storms. No wreckage. Just... gone. The sea took them.";
                                DialogueOption askMoreOpt("Where did they disappear?", "details");
                                askMoreOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_sea_quest"));
                                startNode.options.push_back(askMoreOpt);
                                DialogueOption dismissOpt("Ships sink all the time.", "");
                                dismissOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_sea_quest"));
                                startNode.options.push_back(dismissOpt);
                                DialogueOption updateOpt("Any word on the missing ships?", "update");
                                updateOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_SET, "accepted_sea_quest"));
                                startNode.options.push_back(updateOpt);
                                tree.AddNode(startNode);

                                DialogueNode detailsNode;
                                detailsNode.id = "details";
                                detailsNode.speaker = npcName;
                                detailsNode.text = "All near the Devil's Reef. Sailors tell of strange lights beneath the waves. Compasses spinning wildly. My own brother was on the last ship. Find out what happened.";
                                DialogueOption questOpt("I'll look into it.", "accept");
                                questOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_sea_quest"));
                                questOpt.consequences.push_back(DialogueConsequence(DialogueConsequence::Type::SET_FLAG_VALUE, "accepted_sea_quest", "Investigate the mysterious disappearances near Devil's Reef."));
                                detailsNode.options.push_back(questOpt);
                                detailsNode.options.push_back(DialogueOption("The sea keeps its secrets.", ""));
                                tree.AddNode(detailsNode);

                                DialogueNode acceptNode;
                                acceptNode.id = "accept";
                                acceptNode.speaker = npcName;
                                acceptNode.text = "Thank you. Talk to the lighthouse keeper. He watches those waters every night. If anyone's seen something, it's him.";
                                acceptNode.options.push_back(DialogueOption("I'll find the lighthouse.", ""));
                                tree.AddNode(acceptNode);

                                DialogueNode updateNode;
                                updateNode.id = "update";
                                updateNode.speaker = npcName;
                                updateNode.text = "Another ship reported strange fog near the reef last night. They barely made it through. Something's out there, I tell you.";
                                updateNode.options.push_back(DialogueOption("I'm getting closer to the truth.", ""));
                                tree.AddNode(updateNode);
                            }
                            else
                            {
                                // Crop circles mystery
                                tree.id = "crop_circles";
                                tree.startNodeId = "start";
                                npcName = "Fiona";

                                DialogueNode startNode;
                                startNode.id = "start";
                                startNode.speaker = npcName;
                                startNode.text = "Every morning, new patterns in the wheat fields up north. Perfect circles and spirals. No footprints leading in or out. Something's making them at night.";
                                DialogueOption askMoreOpt("What kind of patterns?", "details");
                                askMoreOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_circles_quest"));
                                startNode.options.push_back(askMoreOpt);
                                DialogueOption dismissOpt("Probably just pranksters.", "");
                                dismissOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_circles_quest"));
                                startNode.options.push_back(dismissOpt);
                                DialogueOption updateOpt("Any new formations?", "update");
                                updateOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_SET, "accepted_circles_quest"));
                                startNode.options.push_back(updateOpt);
                                tree.AddNode(startNode);

                                DialogueNode detailsNode;
                                detailsNode.id = "details";
                                detailsNode.speaker = npcName;
                                detailsNode.text = "Mathematical precision. My dog won't go near them, howls all night long. Last week I found a metal disc in the center of one. Will you watch the fields tonight?";
                                DialogueOption questOpt("I'll keep watch tonight.", "accept");
                                questOpt.conditions.push_back(DialogueCondition(DialogueCondition::Type::FLAG_NOT_SET, "accepted_circles_quest"));
                                questOpt.consequences.push_back(DialogueConsequence(DialogueConsequence::Type::SET_FLAG_VALUE, "accepted_circles_quest", "Watch Farmer Giles' fields at night to discover what's making the crop circles."));
                                detailsNode.options.push_back(questOpt);
                                detailsNode.options.push_back(DialogueOption("I have better things to do.", ""));
                                tree.AddNode(detailsNode);

                                DialogueNode acceptNode;
                                acceptNode.id = "accept";
                                acceptNode.speaker = npcName;
                                acceptNode.text = "Good. Hide by the old scarecrow around midnight. That's when the humming starts. And whatever you do, don't let them see you.";
                                acceptNode.options.push_back(DialogueOption("I'll be there.", ""));
                                tree.AddNode(acceptNode);

                                DialogueNode updateNode;
                                updateNode.id = "update";
                                updateNode.speaker = npcName;
                                updateNode.text = "Three new circles appeared last night. Bigger than before. The wheat in the center was warm to the touch at dawn. Unnatural warm.";
                                updateNode.options.push_back(DialogueOption("I'll catch them in the act.", ""));
                                tree.AddNode(updateNode);
                            }

                            npc.SetDialogueTree(tree);
                            npc.SetName(npcName);

                            m_NPCs.emplace_back(std::move(npc));
                            std::cout << "Placed NPC " << npcType << " at tile (" << tileX << ", " << tileY << ") with dialogue tree" << std::endl;
                        }
                        else
                        {
                            std::cerr << "Failed to load NPC type: " << npcType << std::endl;
                        }
                    }
                    else
                    {
                        std::cerr << "No NPC types available!" << std::endl;
                    }
                }
            }
            // In NPC placement mode we don't place tiles
            return;
        }

        // Particle zone editing mode, click and drag to create zones
        if (m_EditorMode && m_ParticleZoneEditMode)
        {
            if (!m_PlacingParticleZone)
            {
                // Start placing a new zone
                m_PlacingParticleZone = true;
                // Snap to tile grid
                m_ParticleZoneStart.x = static_cast<float>(tileX * m_Tilemap.GetTileWidth());
                m_ParticleZoneStart.y = static_cast<float>(tileY * m_Tilemap.GetTileHeight());
            }
            // Zone is created on mouse release, so just track mouse here
            return;
        }

        // Animation edit mode, apply selected animation to clicked tile
        if (m_EditorMode && m_AnimationEditMode && m_SelectedAnimationId >= 0)
        {
            if (tileX >= 0 && tileX < m_Tilemap.GetMapWidth() &&
                tileY >= 0 && tileY < m_Tilemap.GetMapHeight())
            {
                m_Tilemap.SetTileAnimation(tileX, tileY, m_CurrentLayer, m_SelectedAnimationId);
                std::cout << "Applied animation #" << m_SelectedAnimationId << " to tile (" << tileX << ", " << tileY << ") layer " << m_CurrentLayer << std::endl;
            }
            return;
        }

        // Elevation editing mode, paint elevation values
        if (m_EditorMode && m_ElevationEditMode)
        {
            if (tileX >= 0 && tileX < m_Tilemap.GetMapWidth() &&
                tileY >= 0 && tileY < m_Tilemap.GetMapHeight())
            {
                m_Tilemap.SetElevation(tileX, tileY, m_CurrentElevation);
                std::cout << "Set elevation at (" << tileX << ", " << tileY << ") to " << m_CurrentElevation << std::endl;
            }
            return;
        }

        // Structure editing mode - works like no-projection mode with anchor placement
        // Click = toggle no-projection, Shift+click = flood-fill, Ctrl+click = place anchors
        if (m_EditorMode && m_StructureEditMode)
        {
            if (tileX >= 0 && tileX < m_Tilemap.GetMapWidth() &&
                tileY >= 0 && tileY < m_Tilemap.GetMapHeight())
            {
                bool shiftHeld = (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                  glfwGetKey(m_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
                bool ctrlHeld = (glfwGetKey(m_Window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                                 glfwGetKey(m_Window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);

                if (ctrlHeld && !m_MousePressed)
                {
                    // Ctrl+click: place anchor at clicked corner of tile (no tile modification)
                    int tileWidth = m_Tilemap.GetTileWidth();
                    int tileHeight = m_Tilemap.GetTileHeight();
                    float tileCenterX = (tileX + 0.5f) * tileWidth;
                    float tileCenterY = (tileY + 0.5f) * tileHeight;

                    bool clickedRight = worldX >= tileCenterX;
                    bool clickedBottom = worldY >= tileCenterY;

                    float cornerX = static_cast<float>(clickedRight ? (tileX + 1) * tileWidth : tileX * tileWidth);
                    float cornerY = static_cast<float>(clickedBottom ? (tileY + 1) * tileHeight : tileY * tileHeight);

                    const char* cornerNames[4] = {"top-left", "top-right", "bottom-left", "bottom-right"};
                    int cornerIdx = (clickedBottom ? 2 : 0) + (clickedRight ? 1 : 0);

                    if (m_PlacingAnchor == 0 || m_PlacingAnchor == 1)
                    {
                        // Place left anchor
                        m_TempLeftAnchor = glm::vec2(cornerX, cornerY);
                        m_PlacingAnchor = 2;
                        m_MousePressed = true;
                        std::cout << "Left anchor: " << cornerNames[cornerIdx]
                                  << " of tile (" << tileX << ", " << tileY << ")" << std::endl;
                    }
                    else if (m_PlacingAnchor == 2)
                    {
                        // Place right anchor and create structure
                        m_TempRightAnchor = glm::vec2(cornerX, cornerY);
                        m_PlacingAnchor = 0;
                        m_MousePressed = true;

                        int id = m_Tilemap.AddNoProjectionStructure(m_TempLeftAnchor, m_TempRightAnchor);
                        m_CurrentStructureId = id;
                        std::cout << "Right anchor: " << cornerNames[cornerIdx]
                                  << " of tile (" << tileX << ", " << tileY << ")" << std::endl;
                        std::cout << "Created structure " << id << std::endl;
                        m_TempLeftAnchor = glm::vec2(-1.0f, -1.0f);
                        m_TempRightAnchor = glm::vec2(-1.0f, -1.0f);
                    }
                    // Don't process any tile modifications when placing anchors
                }
                else if (shiftHeld && !m_MousePressed)
                {
                    // Shift+click: flood-fill set no-projection and assign to structure
                    m_MousePressed = true;
                    int mapWidth = m_Tilemap.GetMapWidth();
                    int mapHeight = m_Tilemap.GetMapHeight();
                    std::vector<bool> visited(static_cast<size_t>(mapWidth * mapHeight), false);
                    std::vector<std::pair<int, int>> stack;
                    stack.push_back({tileX, tileY});
                    int count = 0;

                    while (!stack.empty())
                    {
                        auto [cx, cy] = stack.back();
                        stack.pop_back();

                        if (cx < 0 || cx >= mapWidth || cy < 0 || cy >= mapHeight)
                            continue;

                        size_t idx = static_cast<size_t>(cy * mapWidth + cx);
                        if (visited[idx])
                            continue;

                        int tid = m_Tilemap.GetLayerTile(cx, cy, m_CurrentLayer);
                        int animId = m_Tilemap.GetTileAnimation(cx, cy, static_cast<int>(m_CurrentLayer));
                        if (tid < 0 && animId < 0)
                            continue;

                        visited[idx] = true;
                        m_Tilemap.SetLayerNoProjection(cx, cy, m_CurrentLayer, true);
                        if (m_CurrentStructureId >= 0)
                        {
                            m_Tilemap.SetTileStructureId(cx, cy, m_CurrentLayer + 1, m_CurrentStructureId);
                        }
                        count++;

                        stack.push_back({cx - 1, cy});
                        stack.push_back({cx + 1, cy});
                        stack.push_back({cx, cy - 1});
                        stack.push_back({cx, cy + 1});
                    }
                    if (m_CurrentStructureId >= 0)
                        std::cout << "Set no-projection on " << count << " tiles, assigned to structure " << m_CurrentStructureId << std::endl;
                    else
                        std::cout << "Set no-projection on " << count << " tiles (no structure)" << std::endl;
                }
                else if (!ctrlHeld && !shiftHeld && !m_MousePressed)
                {
                    // Normal click: toggle no-projection on single tile
                    m_MousePressed = true;
                    bool current = m_Tilemap.GetLayerNoProjection(tileX, tileY, m_CurrentLayer);
                    m_Tilemap.SetLayerNoProjection(tileX, tileY, m_CurrentLayer, !current);
                    if (m_CurrentStructureId >= 0 && !current)
                    {
                        m_Tilemap.SetTileStructureId(tileX, tileY, m_CurrentLayer + 1, m_CurrentStructureId);
                    }
                    std::cout << (current ? "Cleared" : "Set") << " no-projection at (" << tileX << ", " << tileY << ")" << std::endl;
                }
            }
            return;
        }

        // No-projection editing mode, set no-projection flag for current layer
        // Shift+click, flood-fill to mark all connected tiles in the shape
        if (m_EditorMode && m_NoProjectionEditMode)
        {
            if (tileX >= 0 && tileX < m_Tilemap.GetMapWidth() &&
                tileY >= 0 && tileY < m_Tilemap.GetMapHeight())
            {
                bool shiftHeld = (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                  glfwGetKey(m_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

                if (shiftHeld)
                {
                    // Flood-fill to find connected tiles on CURRENT layer
                    // Connectivity is determined by current layer only
                    int mapWidth = m_Tilemap.GetMapWidth();
                    int mapHeight = m_Tilemap.GetMapHeight();
                    std::vector<bool> visited(static_cast<size_t>(mapWidth * mapHeight), false);
                    std::vector<std::pair<int, int>> stack;
                    stack.push_back({tileX, tileY});
                    int count = 0;

                    while (!stack.empty())
                    {
                        auto [cx, cy] = stack.back();
                        stack.pop_back();

                        if (cx < 0 || cx >= mapWidth || cy < 0 || cy >= mapHeight)
                            continue;

                        size_t idx = static_cast<size_t>(cy * mapWidth + cx);
                        if (visited[idx])
                            continue;

                        // Check connectivity based on CURRENT layer only (tile or animation)
                        int tid = m_Tilemap.GetLayerTile(cx, cy, m_CurrentLayer);
                        int animId = m_Tilemap.GetTileAnimation(cx, cy, static_cast<int>(m_CurrentLayer));
                        if (tid < 0 && animId < 0)
                            continue;

                        visited[idx] = true;
                        // Set noProjection on current layer only
                        m_Tilemap.SetLayerNoProjection(cx, cy, m_CurrentLayer, true);
                        count++;

                        // 4-way connectivity
                        stack.push_back({cx - 1, cy});
                        stack.push_back({cx + 1, cy});
                        stack.push_back({cx, cy - 1});
                        stack.push_back({cx, cy + 1});
                    }
                    std::cout << "Set no-projection on " << count << " connected tiles (layer " << (m_CurrentLayer + 1) << ")" << std::endl;
                }
                else
                {
                    // Single tile: set noProjection on current layer only
                    m_Tilemap.SetLayerNoProjection(tileX, tileY, m_CurrentLayer, true);
                    std::cout << "Set no-projection at (" << tileX << ", " << tileY << ") on layer " << (m_CurrentLayer + 1) << std::endl;
                }
            }
            return;
        }

        // Y-sort-plus editing mode, set Y-sort-plus flag for current layer
        // Shift+click, flood-fill to mark all connected tiles in the shape
        if (m_EditorMode && m_YSortPlusEditMode)
        {
            if (tileX >= 0 && tileX < m_Tilemap.GetMapWidth() &&
                tileY >= 0 && tileY < m_Tilemap.GetMapHeight())
            {
                bool shiftHeld = (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                  glfwGetKey(m_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

                if (shiftHeld)
                {
                    // Flood-fill to find connected tiles with valid tile IDs
                    int mapWidth = m_Tilemap.GetMapWidth();
                    int mapHeight = m_Tilemap.GetMapHeight();
                    std::vector<bool> visited(static_cast<size_t>(mapWidth * mapHeight), false);
                    std::vector<std::pair<int, int>> stack;
                    stack.push_back({tileX, tileY});
                    int count = 0;

                    while (!stack.empty())
                    {
                        auto [cx, cy] = stack.back();
                        stack.pop_back();

                        if (cx < 0 || cx >= mapWidth || cy < 0 || cy >= mapHeight)
                            continue;

                        size_t idx = static_cast<size_t>(cy * mapWidth + cx);
                        if (visited[idx])
                            continue;

                        // Check if tile has a valid tile ID or animation in current layer
                        int tid = m_Tilemap.GetLayerTile(cx, cy, m_CurrentLayer);
                        int animId = m_Tilemap.GetTileAnimation(cx, cy, static_cast<int>(m_CurrentLayer));
                        if (tid < 0 && animId < 0)
                            continue;

                        visited[idx] = true;
                        m_Tilemap.SetLayerYSortPlus(cx, cy, m_CurrentLayer, true);
                        count++;

                        // 4-way connectivity
                        stack.push_back({cx - 1, cy});
                        stack.push_back({cx + 1, cy});
                        stack.push_back({cx, cy - 1});
                        stack.push_back({cx, cy + 1});
                    }
                    std::cout << "Set Y-sort-plus on " << count << " connected tiles (layer " << (m_CurrentLayer + 1) << ")" << std::endl;
                }
                else
                {
                    m_Tilemap.SetLayerYSortPlus(tileX, tileY, m_CurrentLayer, true);
                    std::cout << "Set Y-sort-plus at (" << tileX << ", " << tileY << ") layer " << (m_CurrentLayer + 1) << std::endl;
                }
            }
            return;
        }

        // Y-sort-minus editing mode, set Y-sort-minus flag for current layer
        // Shift+click, flood-fill to mark all connected tiles in the shape
        if (m_EditorMode && m_YSortMinusEditMode)
        {
            if (tileX >= 0 && tileX < m_Tilemap.GetMapWidth() &&
                tileY >= 0 && tileY < m_Tilemap.GetMapHeight())
            {
                bool shiftHeld = (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                  glfwGetKey(m_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

                if (shiftHeld)
                {
                    // Flood-fill to find connected tiles with valid tile IDs
                    int mapWidth = m_Tilemap.GetMapWidth();
                    int mapHeight = m_Tilemap.GetMapHeight();
                    std::vector<bool> visited(static_cast<size_t>(mapWidth * mapHeight), false);
                    std::vector<std::pair<int, int>> stack;
                    stack.push_back({tileX, tileY});
                    int count = 0;

                    while (!stack.empty())
                    {
                        auto [cx, cy] = stack.back();
                        stack.pop_back();

                        if (cx < 0 || cx >= mapWidth || cy < 0 || cy >= mapHeight)
                            continue;

                        size_t idx = static_cast<size_t>(cy * mapWidth + cx);
                        if (visited[idx])
                            continue;

                        // Check if tile has a valid tile ID or animation in current layer
                        int tid = m_Tilemap.GetLayerTile(cx, cy, m_CurrentLayer);
                        int animId = m_Tilemap.GetTileAnimation(cx, cy, static_cast<int>(m_CurrentLayer));
                        if (tid < 0 && animId < 0)
                            continue;

                        visited[idx] = true;
                        m_Tilemap.SetLayerYSortMinus(cx, cy, m_CurrentLayer, true);
                        count++;

                        // 4-way connectivity
                        stack.push_back({cx - 1, cy});
                        stack.push_back({cx + 1, cy});
                        stack.push_back({cx, cy - 1});
                        stack.push_back({cx, cy + 1});
                    }
                    std::cout << "Set Y-sort-minus on " << count << " connected tiles (layer " << (m_CurrentLayer + 1) << ")" << std::endl;
                }
                else
                {
                    m_Tilemap.SetLayerYSortMinus(tileX, tileY, m_CurrentLayer, true);
                    bool isYSortPlus = m_Tilemap.GetLayerYSortPlus(tileX, tileY, m_CurrentLayer);
                    std::cout << "Set Y-sort-minus at (" << tileX << ", " << tileY << ") layer " << (m_CurrentLayer + 1);
                    std::cout << " (Y-sort-plus: " << (isYSortPlus ? "YES" : "NO - tile must also be Y-sort-plus!") << ")" << std::endl;
                }
            }
            return;
        }

        // Check if this is a new tile position
        bool isNewTilePosition = (tileX != m_LastPlacedTileX || tileY != m_LastPlacedTileY);

        if (m_MultiTileSelectionMode)
        {
            // Multi-tile placement, only place on initial click, not on drag
            if (!m_MousePressed)
            {
                int dataTilesPerRow = m_Tilemap.GetTilesetDataWidth() / m_Tilemap.GetTileWidth();

                // Calculate rotated dimensions
                int rotatedWidth = (m_MultiTileRotation == 90 || m_MultiTileRotation == 270) ? m_SelectedTileHeight : m_SelectedTileWidth;
                int rotatedHeight = (m_MultiTileRotation == 90 || m_MultiTileRotation == 270) ? m_SelectedTileWidth : m_SelectedTileHeight;

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

                        int placeX = tileX + dx;
                        int placeY = tileY + dy;
                        int sourceTileID = m_SelectedTileStartID + sourceDy * dataTilesPerRow + sourceDx;

                        if (placeX >= 0 && placeX < m_Tilemap.GetMapWidth() &&
                            placeY >= 0 && placeY < m_Tilemap.GetMapHeight())
                        {
                            // For 90 and 270, flip the texture rotation by 180 to compensate for coordinate system
                            float tileRotation = static_cast<float>(m_MultiTileRotation);
                            if (m_MultiTileRotation == 90 || m_MultiTileRotation == 270)
                            {
                                tileRotation = static_cast<float>((m_MultiTileRotation + 180) % 360);
                            }

                            m_Tilemap.SetLayerTile(placeX, placeY, m_CurrentLayer, sourceTileID);
                            m_Tilemap.SetLayerRotation(placeX, placeY, m_CurrentLayer, tileRotation);
                        }
                    }
                }
                std::cout << "Placed " << m_SelectedTileWidth << "x" << m_SelectedTileHeight
                          << " tiles starting at (" << tileX << ", " << tileY << ") on layer " << (m_CurrentLayer + 1) << std::endl;

                // Keep multi-tile selection active for multiple placements
                m_LastPlacedTileX = tileX;
                m_LastPlacedTileY = tileY;
                m_MousePressed = true;
            }
        }
        else
        {
            // Single tile placement, support drag-to-place with rotation
            if (isNewTilePosition || !m_MousePressed)
            {
                if (tileX >= 0 && tileX < m_Tilemap.GetMapWidth() &&
                    tileY >= 0 && tileY < m_Tilemap.GetMapHeight())
                {
                    // Calculate rotation
                    float tileRotation = static_cast<float>(m_MultiTileRotation);
                    if (m_MultiTileRotation == 90 || m_MultiTileRotation == 270)
                    {
                        tileRotation = static_cast<float>((m_MultiTileRotation + 180) % 360);
                    }

                    m_Tilemap.SetLayerTile(tileX, tileY, m_CurrentLayer, m_SelectedTileStartID);
                    m_Tilemap.SetLayerRotation(tileX, tileY, m_CurrentLayer, tileRotation);

                    m_LastPlacedTileX = tileX;
                    m_LastPlacedTileY = tileY;
                    m_MousePressed = true;
                }
            }
        }
    }

    // Reset mouse pressed state and last placed tile position when mouse button is released
    if (!leftMouseDown)
    {
        // Finalize particle zone placement on mouse release
        if (m_PlacingParticleZone && m_ParticleZoneEditMode)
        {
            // Convert current mouse position to world coordinates
            float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * 16);
            float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * 16);
            float worldWidth = baseWorldWidth / m_CameraZoom;
            float worldHeight = baseWorldHeight / m_CameraZoom;

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

            // Create the zone
            ParticleZone zone;
            zone.position = glm::vec2(zoneX, zoneY);
            zone.size = glm::vec2(zoneW, zoneH);
            zone.type = m_CurrentParticleType;
            zone.enabled = true;

            // Auto-detect noProjection from tiles
            bool hasNoProjection = m_ParticleNoProjection; // Start with manual setting
            if (!hasNoProjection)
            {
                // Check all tiles in the zone across all layers
                for (int ty = minTileY; ty <= maxTileY && !hasNoProjection; ty++)
                {
                    for (int tx = minTileX; tx <= maxTileX && !hasNoProjection; tx++)
                    {
                        for (size_t layer = 0; layer < m_Tilemap.GetLayerCount(); layer++)
                        {
                            if (m_Tilemap.GetLayerNoProjection(tx, ty, layer))
                            {
                                hasNoProjection = true;
                                break;
                            }
                        }
                    }
                }
            }
            zone.noProjection = hasNoProjection;
            m_Tilemap.AddParticleZone(zone);

            const char *typeNames[] = {"Firefly", "Rain", "Snow", "Fog", "Sparkles", "Wisp", "Lantern", "Sunshine"};
            std::cout << "Created " << typeNames[static_cast<int>(m_CurrentParticleType)]
                      << " zone at (" << zoneX << ", " << zoneY << ") size " << zoneW << "x" << zoneH;
            if (hasNoProjection)
                std::cout << " [noProjection]";
            std::cout << std::endl;

            m_PlacingParticleZone = false;
        }

        m_MousePressed = false;
        m_LastPlacedTileX = -1;
        m_LastPlacedTileY = -1;
        m_LastNPCPlacementTileX = -1;
        m_LastNPCPlacementTileY = -1;
    }

    // Update mouse position for preview
    m_LastMouseX = mouseX;
    m_LastMouseY = mouseY;
}

void Game::ScrollCallback(GLFWwindow *window, double /*xoffset*/, double yoffset)
{
    // Retrieve Game instance from window user pointer
    Game *game = static_cast<Game *>(glfwGetWindowUserPointer(window));
    if (!game)
    {
        return;
    }

    // Check for Ctrl modifier
    int ctrlState = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) | glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL);

    // Elevation adjustment with scroll wheel when in elevation edit mode
    if (game->m_EditorMode && game->m_ElevationEditMode && ctrlState != GLFW_PRESS)
    {
        if (yoffset > 0)
        {
            game->m_CurrentElevation += 2;
            if (game->m_CurrentElevation > 32)
                game->m_CurrentElevation = 32;
        }
        else if (yoffset < 0)
        {
            game->m_CurrentElevation -= 2;
            if (game->m_CurrentElevation < -32)
                game->m_CurrentElevation = -32;
        }
        std::cout << "Elevation value: " << game->m_CurrentElevation << " pixels" << std::endl;
        return;
    }

    // If tile picker is not open, handle camera zoom
    if (!(game->m_EditorMode && game->m_ShowTilePicker))
    {
        // Camera zoom with Ctrl+scroll
        if (ctrlState == GLFW_PRESS)
        {
            // Zoom centered on player position
            float baseWorldWidth = static_cast<float>(game->m_TilesVisibleWidth * game->m_Tilemap.GetTileWidth());
            float baseWorldHeight = static_cast<float>(game->m_TilesVisibleHeight * game->m_Tilemap.GetTileHeight());

            float oldZoom = game->m_CameraZoom;
            float oldWorldWidth = baseWorldWidth / oldZoom;
            float oldWorldHeight = baseWorldHeight / oldZoom;

            // Get the player's visual center
            glm::vec2 playerPos = game->m_Player.GetPosition();
            glm::vec2 playerVisualCenter = playerPos - glm::vec2(0.0f, PlayerCharacter::HITBOX_HEIGHT * 0.5f);

            // Apply zoom with snapping to prevent sub-pixel seams
            float zoomDelta = yoffset > 0 ? 1.1f : 0.9f;
            game->m_CameraZoom *= zoomDelta;

            // Editor mode allows zooming out further (0.1x) to see entire map
            float minZoom = (game->m_EditorMode && game->m_FreeCameraMode) ? 0.1f : 0.4f;
            game->m_CameraZoom = std::max(minZoom, std::min(4.0f, game->m_CameraZoom));
            // Snap to 0.1 increments
            game->m_CameraZoom = std::round(game->m_CameraZoom * 10.0f) / 10.0f;

            float newZoom = game->m_CameraZoom;
            float newWorldWidth = baseWorldWidth / newZoom;
            float newWorldHeight = baseWorldHeight / newZoom;

            // Adjust camera position to keep player centered
            game->m_CameraPosition = playerVisualCenter - glm::vec2(newWorldWidth * 0.5f, newWorldHeight * 0.5f);

            // Clamp camera to map bounds (skip in editor free-camera mode)
            if (!(game->m_EditorMode && game->m_FreeCameraMode))
            {
                float mapWidth = static_cast<float>(game->m_Tilemap.GetMapWidth() * game->m_Tilemap.GetTileWidth());
                float mapHeight = static_cast<float>(game->m_Tilemap.GetMapHeight() * game->m_Tilemap.GetTileHeight());
                game->m_CameraPosition.x = std::max(0.0f, std::min(game->m_CameraPosition.x, mapWidth - newWorldWidth));
                game->m_CameraPosition.y = std::max(0.0f, std::min(game->m_CameraPosition.y, mapHeight - newWorldHeight));
            }

            // Also update the follow target so camera doesn't snap back
            game->m_CameraFollowTarget = game->m_CameraPosition;

            std::cout << "Camera zoom: " << game->m_CameraZoom << "x" << std::endl;
        }
        return;
    }

    // Tile picker is open
    int dataTilesPerRow = game->m_Tilemap.GetTilesetDataWidth() / game->m_Tilemap.GetTileWidth();
    int dataTilesPerCol = game->m_Tilemap.GetTilesetDataHeight() / game->m_Tilemap.GetTileHeight();
    float baseTileSizePixels = (static_cast<float>(game->m_ScreenWidth) / static_cast<float>(dataTilesPerRow)) * 1.5f;

    if (ctrlState == GLFW_PRESS)
    {
        // Zoom centered on mouse
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        float oldTileSize = baseTileSizePixels * game->m_TilePickerZoom;

        float adjustedMouseX = static_cast<float>(mouseX) - game->m_TilePickerOffsetX;
        float adjustedMouseY = static_cast<float>(mouseY) - game->m_TilePickerOffsetY;
        int pickerTileX = static_cast<int>(adjustedMouseX / oldTileSize);
        int pickerTileY = static_cast<int>(adjustedMouseY / oldTileSize);

        float zoomDelta = yoffset > 0 ? 1.1f : 0.9f;
        game->m_TilePickerZoom *= zoomDelta;
        game->m_TilePickerZoom = std::max(0.25f, std::min(8.0f, game->m_TilePickerZoom));

        float newTileSize = baseTileSizePixels * game->m_TilePickerZoom;

        // Keep the tile under the cursor fixed by adjusting offsets
        float newTileCenterX = pickerTileX * newTileSize + newTileSize * 0.5f;
        float newTileCenterY = pickerTileY * newTileSize + newTileSize * 0.5f;
        float newOffsetX = static_cast<float>(mouseX) - newTileCenterX;
        float newOffsetY = static_cast<float>(mouseY) - newTileCenterY;

        // Clamp offsets so the sheet stays within viewable bounds
        float totalTilesWidth = newTileSize * dataTilesPerRow;
        float totalTilesHeight = newTileSize * dataTilesPerCol;
        float minOffsetX = game->m_ScreenWidth - totalTilesWidth;
        float maxOffsetX = 0.0f;
        float minOffsetY = game->m_ScreenHeight - totalTilesHeight;
        float maxOffsetY = 0.0f;

        newOffsetX = std::max(minOffsetX, std::min(maxOffsetX, newOffsetX));
        newOffsetY = std::max(minOffsetY, std::min(maxOffsetY, newOffsetY));

        // For zoom, update both current and target for immediate response
        game->m_TilePickerOffsetX = newOffsetX;
        game->m_TilePickerOffsetY = newOffsetY;
        game->m_TilePickerTargetOffsetX = newOffsetX;
        game->m_TilePickerTargetOffsetY = newOffsetY;

        std::cout << "Tile picker zoom: " << game->m_TilePickerZoom << "x (offset: "
                  << game->m_TilePickerOffsetX << ", " << game->m_TilePickerOffsetY << ")" << std::endl;
    }
    else
    {
        // Vertical pan with scroll wheel
        float panAmount = static_cast<float>(yoffset) * 200.0f;
        game->m_TilePickerTargetOffsetY += panAmount;

        float tileSizePixels = baseTileSizePixels * game->m_TilePickerZoom;
        float totalTilesWidth = tileSizePixels * dataTilesPerRow;
        float totalTilesHeight = tileSizePixels * dataTilesPerCol;
        float minOffsetX = game->m_ScreenWidth - totalTilesWidth;
        float maxOffsetX = 0.0f;
        float minOffsetY = game->m_ScreenHeight - totalTilesHeight;
        float maxOffsetY = 0.0f;

        game->m_TilePickerTargetOffsetX = std::max(minOffsetX, std::min(maxOffsetX, game->m_TilePickerTargetOffsetX));
        game->m_TilePickerTargetOffsetY = std::max(minOffsetY, std::min(maxOffsetY, game->m_TilePickerTargetOffsetY));
    }
}

void Game::RecalculateNPCPatrolRoutes()
{
    std::cout << "Recalculating patrol routes for " << m_NPCs.size() << " NPCs..." << std::endl;
    int successCount = 0;
    int removedCount = 0;

    // Remove NPCs that no longer have any navigation tile to stand on
    for (auto it = m_NPCs.begin(); it != m_NPCs.end();)
    {
        int tileX = it->GetTileX();
        int tileY = it->GetTileY();

        // Check if the NPC has at least one navigation tile
        if (!m_Tilemap.GetNavigation(tileX, tileY))
        {
            std::cout << "Removing NPC at (" << tileX << ", " << tileY << ") - no navigation tile" << std::endl;
            it = m_NPCs.erase(it);
            removedCount++;
        }
        else
        {
            // NPC has at least 1 tile to stand on, try to create patrol route
            if (it->ReinitializePatrolRoute(&m_Tilemap))
            {
                successCount++;
            }
            ++it;
        }
    }

    std::cout << "Patrol routes: " << successCount << " successful, " << removedCount << " NPCs removed" << std::endl;
}
