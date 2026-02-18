#pragma once

#include "Tilemap.h"
#include "PlayerCharacter.h"
#include "NonPlayerCharacter.h"
#include "ParticleSystem.h"
#include "IRenderer.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

/**
 * @struct EditorContext
 * @brief Lightweight bridge giving the Editor read/write access to Game-owned state.
 * @author Alex (https://github.com/lextpf)
 * @ingroup Editor
 *
 * EditorContext is constructed by Game::MakeEditorContext() each frame and passed
 * by value to every Editor method. Value members are snapshots (window size,
 * visible tiles); reference members allow the editor to mutate shared state
 * (camera position, zoom, free-camera flag) without a back-pointer to Game.
 *
 * @par Usage
 * @code{.cpp}
 * // Inside Game:
 * EditorContext ctx = MakeEditorContext();
 * m_Editor.ProcessInput(deltaTime, ctx);
 * m_Editor.Render(ctx);
 * @endcode
 *
 * @par Design Rationale
 * Using a context struct instead of a Game pointer keeps Editor decoupled from
 * the Game class definition. Editor.h never includes Game.h, which prevents
 * circular dependencies and makes the editor testable in isolation.
 *
 * @see Editor, Game::MakeEditorContext()
 */
struct EditorContext
{
    GLFWwindow* window;
    int screenWidth;
    int screenHeight;
    int tilesVisibleWidth;
    int tilesVisibleHeight;
    glm::vec2& cameraPosition;
    glm::vec2& cameraFollowTarget;
    bool& hasCameraFollowTarget;
    float& cameraZoom;
    bool& freeCameraMode;
    bool& enable3DEffect;
    float& cameraTilt;
    float& globeSphereRadius;
    Tilemap& tilemap;
    PlayerCharacter& player;
    std::vector<NonPlayerCharacter>& npcs;
    IRenderer& renderer;
    ParticleSystem& particles;
};

/**
 * @class Editor
 * @brief Level editor with tile placement, overlay rendering, and debug tools.
 * @author Alex (https://github.com/lextpf)
 * @ingroup Editor
 *
 * Editor owns all editor-specific state (mode flags, tile selection, mouse
 * tracking, tile picker camera) and implements tile placement, overlay
 * rendering, and debug visualization. Game delegates to Editor via an
 * EditorContext built each frame; Editor never holds a pointer to Game.
 *
 * @par Activation
 * Toggled with the **E** key in Game::ProcessInput(). When active the tile
 * picker opens automatically; when deactivated it closes.
 *
 * @par Editor Modes
 * Only one sub-mode is active at a time, selected by hotkey:
 *
 * | Key | Mode               | Left-Click Action                | Right-Click Action           |
 * |-----|--------------------|----------------------------------|------------------------------|
 * |   T | Tile Picker        | Select tile / multi-tile region  | -                            |
 * |   M | Navigation Edit    | -                                | Toggle walkability (drag)    |
 * |   N | NPC Placement      | Place / remove NPC               | -                            |
 * |   B | No-Projection Edit | Set no-projection flag (flood)   | Clear flag (flood)           |
 * |   G | Structure Edit     | Anchor + flood assign structure  | Clear structure assignment   |
 * |   H | Elevation Edit     | Paint elevation value            | Clear elevation              |
 * |   J | Particle Zone Edit | Drag to create zone              | Remove zone                  |
 * |   K | Animation Edit     | Apply animation to tile          | Remove animation             |
 * |   - | Default            | Place selected tile (drag)       | Toggle collision (drag)      |
 *
 * @par Per-Frame Pipeline
 * @code
 * Game::ProcessInput   -->  Editor::ProcessInput       (keyboard)
 *                      -->  Editor::ProcessMouseInput  (mouse)
 * Game::Update         -->  Editor::Update          (tile picker smoothing)
 * Game::Render         -->  Editor::Render          (overlays + tile picker)
 * Game::ScrollCallback -->  Editor::HandleScroll    (elevation / tile picker)
 * @endcode
 *
 * @par Debug Overlays (F3)
 * When debug mode is active (toggled independently of editor mode), all
 * overlay layers are rendered: collision, navigation, elevation, corner
 * cutting, no-projection, structures, Y-sort flags, particle zones, and
 * NPC patrol info.
 *
 * @see EditorContext, Game::MakeEditorContext()
 */
class Editor
{
public:
    Editor();

    /**
     * @brief Initialize editor with available NPC types.
     * @param npcTypes List of NPC sprite paths.
     */
    void Initialize(const std::vector<std::string> &npcTypes);

    bool IsActive() const { return m_EditorMode; }
    void SetActive(bool active);

    void ProcessInput(float deltaTime, EditorContext ctx);
    void ProcessMouseInput(EditorContext ctx);
    void HandleScroll(double yoffset, EditorContext ctx);
    void Update(float deltaTime, EditorContext ctx);

    /**
     * @brief Render editor overlays and tile picker.
     *
     * Handles perspective suspension internally for the tile picker.
     * Called from Game::Render() when editor or debug mode is active.
     */
    void Render(EditorContext ctx);

    /**
     * @brief Render no-projection anchor markers on top of everything.
     *
     * Separate from Render() because anchors must appear above all UI.
     * Caller is responsible for suspending perspective before calling.
     */
    void RenderNoProjectionAnchors(EditorContext ctx);

    bool IsDebugMode() const { return m_DebugMode; }
    bool IsShowDebugInfo() const { return m_ShowDebugInfo; }
    bool IsShowNoProjectionAnchors() const { return m_ShowNoProjectionAnchors; }
    bool ShowTilePicker() const { return m_ShowTilePicker; }

    void ToggleDebugMode();
    void ToggleShowDebugInfo();

    /**
     * @brief Reset tile picker zoom and pan to defaults.
     *
     * Called from Game when Z key is pressed in editor mode.
     */
    void ResetTilePickerState();

private:
    void RenderEditorUI(EditorContext ctx);
    void RenderCollisionOverlays(EditorContext ctx);
    void RenderNavigationOverlays(EditorContext ctx);
    void RenderElevationOverlays(EditorContext ctx);
    void RenderNoProjectionOverlays(EditorContext ctx);
    void RenderNoProjectionAnchorsImpl(EditorContext ctx);
    void RenderStructureOverlays(EditorContext ctx);
    void RenderLayerFlagOverlays(EditorContext ctx, bool editMode,
                                  bool (Tilemap::*getter)(int, int, size_t) const,
                                  const glm::vec3& color);
    void RenderYSortPlusOverlays(EditorContext ctx);
    void RenderYSortMinusOverlays(EditorContext ctx);
    void RenderParticleZoneOverlays(EditorContext ctx);
    void RenderNPCDebugInfo(EditorContext ctx);
    void RenderCornerCuttingOverlays(EditorContext ctx);
    void RenderLayerOverlay(EditorContext ctx, int layerIndex, const glm::vec4& color);
    void RenderPlacementPreview(EditorContext ctx);

    void RecalculateNPCPatrolRoutes(EditorContext ctx);

    /// @name Mode Flags
    /// @{
    bool m_EditorMode;
    bool m_ShowTilePicker;
    bool m_EditNavigationMode;
    bool m_ElevationEditMode;
    bool m_NPCPlacementMode;
    bool m_NoProjectionEditMode;
    bool m_YSortPlusEditMode;
    bool m_YSortMinusEditMode;
    bool m_ParticleZoneEditMode;
    bool m_StructureEditMode;
    bool m_AnimationEditMode;
    /// @}

    /// @name Particle Zone Editing
    /// @{
    ParticleType m_CurrentParticleType;
    bool m_ParticleNoProjection;
    bool m_PlacingParticleZone;
    glm::vec2 m_ParticleZoneStart;
    /// @}

    /// @name Structure Editing
    /// @{
    int m_CurrentStructureId;
    int m_PlacingAnchor;
    glm::vec2 m_TempLeftAnchor;
    glm::vec2 m_TempRightAnchor;
    bool m_AssigningTilesToStructure;
    /// @}

    /// @name Animation Editing
    /// @{
    std::vector<int> m_AnimationFrames;
    float m_AnimationFrameDuration;
    int m_SelectedAnimationId;
    /// @}

    /// @name Debug Flags
    /// @{
    bool m_DebugMode;
    bool m_ShowDebugInfo;
    bool m_ShowNoProjectionAnchors;
    /// @}

    /// @name Tile Selection
    /// @{
    int m_SelectedTileID;
    int m_CurrentLayer;
    int m_CurrentElevation;
    /// @}

    /// @name NPC Types
    /// @{
    std::vector<std::string> m_AvailableNPCTypes;
    size_t m_SelectedNPCTypeIndex;
    /// @}

    /// @name Mouse/Drag State
    /// @{
    double m_LastMouseX;
    double m_LastMouseY;
    bool m_MousePressed;
    bool m_RightMousePressed;
    int m_LastPlacedTileX;
    int m_LastPlacedTileY;
    int m_LastNavigationTileX;
    int m_LastNavigationTileY;
    bool m_NavigationDragState;
    int m_LastCollisionTileX;
    int m_LastCollisionTileY;
    bool m_CollisionDragState;
    int m_LastNPCPlacementTileX;
    int m_LastNPCPlacementTileY;
    /// @}

    /// @name Tile Picker State
    /// @{
    float m_TilePickerZoom;
    float m_TilePickerOffsetX;
    float m_TilePickerOffsetY;
    float m_TilePickerTargetOffsetX;
    float m_TilePickerTargetOffsetY;
    /// @}

    /// @name Multi-Tile Selection
    /// @{
    bool m_MultiTileSelectionMode;
    int m_SelectedTileStartID;
    int m_SelectedTileWidth;
    int m_SelectedTileHeight;
    bool m_IsSelectingTiles;
    int m_SelectionStartTileID;
    float m_PlacementCameraZoom;
    bool m_IsPlacingMultiTile;
    int m_MultiTileRotation;
    /// @}
};
