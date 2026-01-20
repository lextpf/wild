#include "Tilemap.h"
#include "NonPlayerCharacter.h"

#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <functional>
#include <cstring>
#include <json.hpp>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

// Note: STB_IMAGE_IMPLEMENTATION is already defined in Texture.cpp
// We just need the header for function declarations
#include <stb_image.h>

Tilemap::Tilemap()
    : m_TileWidth(16)
    , m_TileHeight(16)
    , m_MapWidth(125)
    , m_MapHeight(125)
    , m_TilesetWidth(0)
    , m_TilesetHeight(0)
    , m_TilesPerRow(0)
    , m_TilesetData(nullptr)
    , m_TilesetDataWidth(0)
    , m_TilesetDataHeight(0)
    , m_TilesetChannels(0)
    , m_TilesetDataFromStbi(false)
    , m_TransparencyCacheBuilt(false)
    , m_AnimationTime(0.0f)
{
    // Allocate storage for all layers using row-major layout: size = width * height
    const size_t mapSize = m_MapWidth * m_MapHeight;

    // Collision and navigation maps
    m_CollisionMap.Resize(m_MapWidth, m_MapHeight);
    m_NavigationMap.Resize(m_MapWidth, m_MapHeight);

    // Initialize 8 dynamic layers with proper render order:
    // Background layers (rendered before player/NPCs):
    //   Layer 0: Ground (renderOrder 0)
    //   Layer 1: Ground Detail (renderOrder 10)
    //   Layer 2: Objects (renderOrder 20)
    //   Layer 3: Objects2 (renderOrder 30)
    // Foreground layers (rendered after player/NPCs):
    //   Layer 4: Foreground (renderOrder 100)
    //   Layer 5: Foreground2 (renderOrder 110)
    //   Layer 6: Overlay (renderOrder 120)
    //   Layer 7: Overlay2 (renderOrder 130)
    m_Layers.clear();
    m_Layers.emplace_back("Ground", 0, true);
    m_Layers.emplace_back("Ground Detail", 10, true);
    m_Layers.emplace_back("Objects", 20, true);
    m_Layers.emplace_back("Objects2", 30, true);
    m_Layers.emplace_back("Foreground", 100, false);
    m_Layers.emplace_back("Foreground2", 110, false);
    m_Layers.emplace_back("Overlay", 120, false);
    m_Layers.emplace_back("Overlay2", 130, false);

    // Resize all layers to map size
    for (auto &layer : m_Layers)
    {
        layer.resize(mapSize);
    }

    // Initialize animation map (all tiles start with no animation)
    m_TileAnimationMap.assign(mapSize, -1);

    // Defer map generation until tileset is loaded
    // GenerateDefaultMap() will be called from SetTilemapSize() after LoadTileset()
}

Tilemap::~Tilemap()
{
    if (m_TilesetData)
    {
        if (m_TilesetDataFromStbi)
            stbi_image_free(m_TilesetData);
        else
            delete[] m_TilesetData;
        m_TilesetData = nullptr;
    }
}

bool Tilemap::LoadTileset(const std::string &path, int tileWidth, int tileHeight)
{
    m_TileWidth = tileWidth;
    m_TileHeight = tileHeight;

    // === Load #1: GPU texture for rendering ===
    if (!m_TilesetTexture.LoadFromFile(path))
    {
        return false;
    }

    m_TilesetWidth = m_TilesetTexture.GetWidth();
    m_TilesetHeight = m_TilesetTexture.GetHeight();
    m_TilesPerRow = m_TilesetWidth / m_TileWidth;

    std::cout << "Texture dimensions: " << m_TilesetWidth << "x" << m_TilesetHeight << std::endl;
    std::cout << "Tile size: " << m_TileWidth << "x" << m_TileHeight << std::endl;
    std::cout << "Tiles per row: " << m_TilesPerRow << std::endl;

    // === Load #2: CPU pixel data for transparency checking ===
    // Don't flip - we need raw pixel coordinates for IsTileTransparent()
    stbi_set_flip_vertically_on_load(false);
    m_TilesetData = stbi_load(path.c_str(), &m_TilesetDataWidth, &m_TilesetDataHeight, &m_TilesetChannels, 0);
    m_TilesetDataFromStbi = true;  // Allocated by stbi_load, must use stbi_image_free

    if (!m_TilesetData)
    {
        std::cerr << "ERROR: Could not load tileset data for transparency checking!" << std::endl;
        std::cerr << "Path: " << path << std::endl;
        m_TilesetChannels = 0;
        m_TilesetDataWidth = 0;
        m_TilesetDataHeight = 0;
        return false;
    }

    std::cout << "Loaded tileset data: " << m_TilesetDataWidth << "x" << m_TilesetDataHeight
              << " channels: " << m_TilesetChannels << std::endl;
    std::cout << "Tiles per row (from data): " << (m_TilesetDataWidth / m_TileWidth)
              << ", Total tiles: "
              << (m_TilesetDataWidth / m_TileWidth) * (m_TilesetDataHeight / m_TileHeight) << std::endl;

    // Debug: sample a pixel to verify data integrity
    if (16 < m_TilesetDataWidth && 32 < m_TilesetDataHeight)
    {
        int testIndex = (32 * m_TilesetDataWidth + 16) * m_TilesetChannels;
        if (testIndex >= 0 && testIndex < m_TilesetDataWidth * m_TilesetDataHeight * m_TilesetChannels)
        {
            if (m_TilesetChannels == 4)
            {
                std::cout << "Pixel at (16,32): RGBA("
                          << (int)m_TilesetData[testIndex] << ","
                          << (int)m_TilesetData[testIndex + 1] << ","
                          << (int)m_TilesetData[testIndex + 2] << ","
                          << (int)m_TilesetData[testIndex + 3] << ")" << std::endl;
            }
            else if (m_TilesetChannels == 3)
            {
                std::cout << "Pixel at (16,32): RGB("
                          << (int)m_TilesetData[testIndex] << ","
                          << (int)m_TilesetData[testIndex + 1] << ","
                          << (int)m_TilesetData[testIndex + 2] << ")" << std::endl;
            }
        }
    }

    // Build transparency cache for all tiles
    BuildTransparencyCache();

    // Map generation is deferred to SetTilemapSize() or LoadMapFromJSON()
    return true;
}

void Tilemap::BuildTransparencyCache()
{
    if (!m_TilesetData || m_TilesetChannels == 0)
    {
        m_TransparencyCacheBuilt = false;
        return;
    }

    int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    int dataTilesPerCol = m_TilesetDataHeight / m_TileHeight;
    int totalTiles = dataTilesPerRow * dataTilesPerCol;

    m_TileTransparencyCache.resize(totalTiles, true);

    for (int tileID = 0; tileID < totalTiles; ++tileID)
    {
        int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
        int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;

        bool isTransparent = true;

        // Scan pixels in this tile
        for (int y = 0; y < m_TileHeight && isTransparent; ++y)
        {
            for (int x = 0; x < m_TileWidth && isTransparent; ++x)
            {
                int px = tilesetX + x;
                int py = tilesetY + y;

                if (px >= m_TilesetDataWidth || py >= m_TilesetDataHeight)
                    continue;

                int index = (py * m_TilesetDataWidth + px) * m_TilesetChannels;

                if (m_TilesetChannels == 4)
                {
                    unsigned char alpha = m_TilesetData[index + 3];
                    if (alpha > 0)
                        isTransparent = false;
                }
                else if (m_TilesetChannels == 3)
                {
                    unsigned char r = m_TilesetData[index];
                    unsigned char g = m_TilesetData[index + 1];
                    unsigned char b = m_TilesetData[index + 2];
                    bool isPureBlack = (r == 0 && g == 0 && b == 0);
                    bool isPureWhite = (r == 255 && g == 255 && b == 255);
                    if (!isPureBlack && !isPureWhite)
                        isTransparent = false;
                }
            }
        }

        m_TileTransparencyCache[tileID] = isTransparent;
    }

    m_TransparencyCacheBuilt = true;
    std::cout << "Built transparency cache for " << totalTiles << " tiles" << std::endl;
}

bool Tilemap::LoadCombinedTileset(const std::string &path1, const std::string &path2, int tileWidth, int tileHeight)
{
    m_TileWidth = tileWidth;
    m_TileHeight = tileHeight;

    // Load both tilesets as raw pixel data (no flipping for combining)
    stbi_set_flip_vertically_on_load(false);

    int width1, height1, channels1;
    unsigned char *data1 = stbi_load(path1.c_str(), &width1, &height1, &channels1, 0);
    if (!data1)
    {
        std::cerr << "ERROR: Could not load first tileset: " << path1 << std::endl;
        return false;
    }

    int width2, height2, channels2;
    unsigned char *data2 = stbi_load(path2.c_str(), &width2, &height2, &channels2, 0);
    if (!data2)
    {
        std::cerr << "ERROR: Could not load second tileset: " << path2 << std::endl;
        stbi_image_free(data1);
        return false;
    }

    // Verify channel compatibility (required for memcpy-based combining)
    if (channels1 != channels2)
    {
        std::cerr << "ERROR: Tilesets must have the same number of channels! Tileset 1: "
                  << channels1 << ", Tileset 2: " << channels2 << std::endl;
        stbi_image_free(data1);
        stbi_image_free(data2);
        return false;
    }

    // Calculate combined dimensions with width padding for narrower tilesets
    int combinedWidth = std::max(width1, width2);
    int combinedHeight = height1 + height2;
    int channels = channels1;

    // Allocate and zero-initialize combined buffer (transparent padding)
    unsigned char *combinedData = new unsigned char[combinedWidth * combinedHeight * channels];
    memset(combinedData, 0, combinedWidth * combinedHeight * channels);

    // Copy tileset 1 to top (row by row to handle width differences)
    for (int y = 0; y < height1; ++y)
    {
        memcpy(combinedData + y * combinedWidth * channels,
               data1 + y * width1 * channels,
               width1 * channels);
        // Remaining pixels in row stay transparent (zeroed)
    }

    // Copy tileset 2 below tileset 1
    for (int y = 0; y < height2; ++y)
    {
        memcpy(combinedData + (height1 + y) * combinedWidth * channels,
               data2 + y * width2 * channels,
               width2 * channels);
    }

    // Create vertically-flipped copy for OpenGL texture (origin at bottom-left)
    unsigned char *flippedData = new unsigned char[combinedWidth * combinedHeight * channels];
    for (int y = 0; y < combinedHeight; ++y)
    {
        int srcY = combinedHeight - 1 - y;
        memcpy(flippedData + y * combinedWidth * channels,
               combinedData + srcY * combinedWidth * channels,
               combinedWidth * channels);
    }

    // Upload flipped data as GPU texture
    if (!m_TilesetTexture.LoadFromData(flippedData, combinedWidth, combinedHeight, channels, false))
    {
        std::cerr << "ERROR: Failed to create combined texture!" << std::endl;
        delete[] combinedData;
        delete[] flippedData;
        stbi_image_free(data1);
        stbi_image_free(data2);
        return false;
    }

    // Keep unflipped data for CPU-side transparency checking
    m_TilesetData = combinedData;
    m_TilesetDataFromStbi = false;  // Allocated with new[], must use delete[]
    m_TilesetDataWidth = combinedWidth;
    m_TilesetDataHeight = combinedHeight;
    m_TilesetChannels = channels;

    m_TilesetWidth = combinedWidth;
    m_TilesetHeight = combinedHeight;
    m_TilesPerRow = m_TilesetWidth / m_TileWidth;

    std::cout << "Combined tileset dimensions: " << m_TilesetWidth << "x" << m_TilesetHeight << std::endl;
    std::cout << "  Tileset 1: " << width1 << "x" << height1 << " (" << (width1 / m_TileWidth) << " tiles wide)" << std::endl;
    std::cout << "  Tileset 2: " << width2 << "x" << height2 << " (" << (width2 / m_TileWidth) << " tiles wide)" << std::endl;
    if (width1 != width2)
    {
        std::cout << "  Note: Tilesets have different widths. Narrower tileset padded with transparency." << std::endl;
    }
    std::cout << "Tile size: " << m_TileWidth << "x" << m_TileHeight << std::endl;
    std::cout << "Tiles per row: " << m_TilesPerRow << std::endl;
    std::cout << "Total tiles: " << (m_TilesetDataWidth / m_TileWidth) * (m_TilesetDataHeight / m_TileHeight) << std::endl;

    // Cleanup
    delete[] flippedData;
    stbi_image_free(data1);
    stbi_image_free(data2);

    // Build transparency cache for all tiles
    BuildTransparencyCache();

    return true;
}

bool Tilemap::LoadCombinedTileset3(const std::string &path1, const std::string &path2, const std::string &path3, int tileWidth, int tileHeight)
{
    m_TileWidth = tileWidth;
    m_TileHeight = tileHeight;

    // Load first tileset as raw data
    stbi_set_flip_vertically_on_load(false);
    int width1, height1, channels1;
    unsigned char *data1 = stbi_load(path1.c_str(), &width1, &height1, &channels1, 0);
    if (!data1)
    {
        std::cerr << "ERROR: Could not load first tileset: " << path1 << std::endl;
        return false;
    }

    // Load second tileset as raw data
    int width2, height2, channels2;
    unsigned char *data2 = stbi_load(path2.c_str(), &width2, &height2, &channels2, 0);
    if (!data2)
    {
        std::cerr << "ERROR: Could not load second tileset: " << path2 << std::endl;
        stbi_image_free(data1);
        return false;
    }

    // Load third tileset as raw data
    int width3, height3, channels3;
    unsigned char *data3 = stbi_load(path3.c_str(), &width3, &height3, &channels3, 0);
    if (!data3)
    {
        std::cerr << "ERROR: Could not load third tileset: " << path3 << std::endl;
        stbi_image_free(data1);
        stbi_image_free(data2);
        return false;
    }

    // Verify all tilesets have same channels
    if (channels1 != channels2 || channels1 != channels3)
    {
        std::cerr << "ERROR: Tilesets must have the same number of channels! Tileset 1: " << channels1
                  << ", Tileset 2: " << channels2 << ", Tileset 3: " << channels3 << std::endl;
        stbi_image_free(data1);
        stbi_image_free(data2);
        stbi_image_free(data3);
        return false;
    }

    // Use the maximum width for the combined tileset (pad narrower tilesets)
    int combinedWidth = std::max({width1, width2, width3});
    int combinedHeight = height1 + height2 + height3;
    int channels = channels1;

    unsigned char *combinedData = new unsigned char[combinedWidth * combinedHeight * channels];

    // Initialize combined data to transparent (0)
    memset(combinedData, 0, combinedWidth * combinedHeight * channels);

    // Copy first tileset to top (may need padding if narrower)
    for (int y = 0; y < height1; ++y)
    {
        memcpy(combinedData + y * combinedWidth * channels,
               data1 + y * width1 * channels,
               width1 * channels);
        // Rest of the row is already transparent from memset
    }

    // Copy second tileset below first tileset (may need padding if narrower)
    for (int y = 0; y < height2; ++y)
    {
        memcpy(combinedData + (height1 + y) * combinedWidth * channels,
               data2 + y * width2 * channels,
               width2 * channels);
        // Rest of the row is already transparent from memset
    }

    // Copy third tileset below second tileset (may need padding if narrower)
    for (int y = 0; y < height3; ++y)
    {
        memcpy(combinedData + (height1 + height2 + y) * combinedWidth * channels,
               data3 + y * width3 * channels,
               width3 * channels);
        // Rest of the row is already transparent from memset
    }

    // Create OpenGL texture from combined data
    // Flip vertically for OpenGL (origin at bottom-left)
    unsigned char *flippedData = new unsigned char[combinedWidth * combinedHeight * channels];
    for (int y = 0; y < combinedHeight; ++y)
    {
        int srcY = combinedHeight - 1 - y;
        memcpy(flippedData + y * combinedWidth * channels,
               combinedData + srcY * combinedWidth * channels,
               combinedWidth * channels);
    }

    // Load combined texture
    if (!m_TilesetTexture.LoadFromData(flippedData, combinedWidth, combinedHeight, channels, false))
    {
        std::cerr << "ERROR: Failed to create combined texture!" << std::endl;
        delete[] combinedData;
        delete[] flippedData;
        stbi_image_free(data1);
        stbi_image_free(data2);
        stbi_image_free(data3);
        return false;
    }

    // Store combined data for transparency checking (don't flip for data checking)
    m_TilesetData = combinedData;
    m_TilesetDataFromStbi = false;  // Allocated with new[], must use delete[]
    m_TilesetDataWidth = combinedWidth;
    m_TilesetDataHeight = combinedHeight;
    m_TilesetChannels = channels;

    m_TilesetWidth = combinedWidth;
    m_TilesetHeight = combinedHeight;
    m_TilesPerRow = m_TilesetWidth / m_TileWidth;

    std::cout << "Combined tileset dimensions: " << m_TilesetWidth << "x" << m_TilesetHeight << std::endl;
    std::cout << "  Tileset 1: " << width1 << "x" << height1 << " (" << (width1 / m_TileWidth) << " tiles wide)" << std::endl;
    std::cout << "  Tileset 2: " << width2 << "x" << height2 << " (" << (width2 / m_TileWidth) << " tiles wide)" << std::endl;
    std::cout << "  Tileset 3: " << width3 << "x" << height3 << " (" << (width3 / m_TileWidth) << " tiles wide)" << std::endl;
    if (width1 != width2 || width1 != width3)
    {
        std::cout << "  Note: Tilesets have different widths. Narrower tilesets padded with transparency." << std::endl;
    }
    std::cout << "Tile size: " << m_TileWidth << "x" << m_TileHeight << std::endl;
    std::cout << "Tiles per row: " << m_TilesPerRow << std::endl;
    std::cout << "Total tiles: " << (m_TilesetDataWidth / m_TileWidth) * (m_TilesetDataHeight / m_TileHeight) << std::endl;

    // Clean up temporary data
    delete[] flippedData;
    stbi_image_free(data1);
    stbi_image_free(data2);
    stbi_image_free(data3);

    // Build transparency cache for all tiles
    BuildTransparencyCache();

    return true;
}

bool Tilemap::LoadCombinedTilesets(const std::vector<std::string> &paths, int tileWidth, int tileHeight)
{
    if (paths.empty())
    {
        std::cerr << "ERROR: No tileset paths provided!" << std::endl;
        return false;
    }

    m_TileWidth = tileWidth;
    m_TileHeight = tileHeight;

    // Load all tilesets as raw data
    stbi_set_flip_vertically_on_load(false);

    struct TilesetData
    {
        unsigned char *data;
        int width;
        int height;
        int channels;
    };

    std::vector<TilesetData> tilesets;
    tilesets.reserve(paths.size());

    // Load all tilesets
    for (size_t i = 0; i < paths.size(); ++i)
    {
        int width, height, channels;
        unsigned char *data = stbi_load(paths[i].c_str(), &width, &height, &channels, 0);
        if (!data)
        {
            std::cerr << "ERROR: Could not load tileset " << (i + 1) << ": " << paths[i] << std::endl;
            // Clean up already loaded tilesets
            for (auto &ts : tilesets)
            {
                stbi_image_free(ts.data);
            }
            return false;
        }
        tilesets.push_back({data, width, height, channels});
    }

    // Verify at least one tileset was loaded
    if (tilesets.empty())
    {
        std::cerr << "ERROR: No tilesets were loaded!" << std::endl;
        return false;
    }

    // Verify all tilesets have same channels
    int channels = tilesets[0].channels;
    for (size_t i = 1; i < tilesets.size(); ++i)
    {
        if (tilesets[i].channels != channels)
        {
            std::cerr << "ERROR: Tilesets must have the same number of channels! Tileset 1: " << channels
                      << ", Tileset " << (i + 1) << ": " << tilesets[i].channels << std::endl;
            // Clean up
            for (auto &ts : tilesets)
            {
                stbi_image_free(ts.data);
            }
            return false;
        }
    }

    // Find maximum width for the combined tileset
    int combinedWidth = tilesets[0].width;
    int combinedHeight = 0;
    for (const auto &ts : tilesets)
    {
        combinedWidth = std::max(combinedWidth, ts.width);
        combinedHeight += ts.height;
    }

    // Allocate combined data
    unsigned char *combinedData = new unsigned char[combinedWidth * combinedHeight * channels];

    // Initialize combined data to transparent (0)
    memset(combinedData, 0, combinedWidth * combinedHeight * channels);

    // Copy each tileset vertically, stacking them
    int currentY = 0;
    for (size_t i = 0; i < tilesets.size(); ++i)
    {
        const auto &ts = tilesets[i];
        for (int y = 0; y < ts.height; ++y)
        {
            memcpy(combinedData + (currentY + y) * combinedWidth * channels,
                   ts.data + y * ts.width * channels,
                   ts.width * channels);
            // Rest of the row is already transparent from memset
        }
        currentY += ts.height;
    }

    // Create OpenGL texture from combined data
    // Flip vertically for OpenGL (origin at bottom-left)
    unsigned char *flippedData = new unsigned char[combinedWidth * combinedHeight * channels];
    for (int y = 0; y < combinedHeight; ++y)
    {
        int srcY = combinedHeight - 1 - y;
        memcpy(flippedData + y * combinedWidth * channels,
               combinedData + srcY * combinedWidth * channels,
               combinedWidth * channels);
    }

    // Load combined texture
    if (!m_TilesetTexture.LoadFromData(flippedData, combinedWidth, combinedHeight, channels, false))
    {
        std::cerr << "ERROR: Failed to create combined texture!" << std::endl;
        delete[] combinedData;
        delete[] flippedData;
        for (auto &ts : tilesets)
        {
            stbi_image_free(ts.data);
        }
        return false;
    }

    // Store combined data for transparency checking (don't flip for data checking)
    m_TilesetData = combinedData;
    m_TilesetDataFromStbi = false;  // Allocated with new[], must use delete[]
    m_TilesetDataWidth = combinedWidth;
    m_TilesetDataHeight = combinedHeight;
    m_TilesetChannels = channels;

    m_TilesetWidth = combinedWidth;
    m_TilesetHeight = combinedHeight;
    m_TilesPerRow = m_TilesetWidth / m_TileWidth;

    std::cout << "Combined tileset dimensions: " << m_TilesetWidth << "x" << m_TilesetHeight << std::endl;
    for (size_t i = 0; i < tilesets.size(); ++i)
    {
        std::cout << "  Tileset " << (i + 1) << ": " << tilesets[i].width << "x" << tilesets[i].height
                  << " (" << (tilesets[i].width / m_TileWidth) << " tiles wide) - " << paths[i] << std::endl;
    }
    if (tilesets.size() > 1)
    {
        bool differentWidths = false;
        for (size_t i = 1; i < tilesets.size(); ++i)
        {
            if (tilesets[i].width != tilesets[0].width)
            {
                differentWidths = true;
                break;
            }
        }
        if (differentWidths)
        {
            std::cout << "  Note: Tilesets have different widths. Narrower tilesets padded with transparency." << std::endl;
        }
    }
    std::cout << "Tile size: " << m_TileWidth << "x" << m_TileHeight << std::endl;
    std::cout << "Tiles per row: " << m_TilesPerRow << std::endl;
    std::cout << "Total tiles: " << (m_TilesetDataWidth / m_TileWidth) * (m_TilesetDataHeight / m_TileHeight) << std::endl;

    // Clean up temporary data
    delete[] flippedData;
    for (auto &ts : tilesets)
    {
        stbi_image_free(ts.data);
    }

    // Build transparency cache for all tiles
    BuildTransparencyCache();

    return true;
}

void Tilemap::SetTilemapSize(int width, int height, bool generateMap)
{
    m_MapWidth = width;
    m_MapHeight = height;

    const size_t mapSize = static_cast<size_t>(m_MapWidth) * static_cast<size_t>(m_MapHeight);

    m_Elevation.assign(mapSize, 0);

    // Initialize dynamic layers (8 total: 4 background, 4 foreground)
    m_Layers.clear();
    m_Layers.reserve(8);

    // Background layers (rendered before player)
    m_Layers.push_back(TileLayer("Ground", 0, true));         // Layer 0: Base terrain
    m_Layers.push_back(TileLayer("Ground Detail", 10, true)); // Layer 1: Ground details
    m_Layers.push_back(TileLayer("Objects", 20, true));       // Layer 2: Background objects
    m_Layers.push_back(TileLayer("Objects2", 30, true));      // Layer 3: More background objects

    // Foreground layers (rendered after player for depth)
    m_Layers.push_back(TileLayer("Foreground", 100, false));  // Layer 4: Foreground objects
    m_Layers.push_back(TileLayer("Foreground2", 110, false)); // Layer 5: More foreground
    m_Layers.push_back(TileLayer("Overlay", 120, false));     // Layer 6: Top overlay
    m_Layers.push_back(TileLayer("Overlay2", 130, false));    // Layer 7: Extra top layer

    // Resize all layer data arrays
    for (auto &layer : m_Layers)
    {
        layer.resize(mapSize);
    }

    m_CollisionMap.Resize(m_MapWidth, m_MapHeight);
    m_NavigationMap.Resize(m_MapWidth, m_MapHeight);
    m_CornerCutBlocked.assign(mapSize, 0); // All corners allow cutting by default

    // Initialize animation map
    m_TileAnimationMap.assign(mapSize, -1);
    m_AnimationTime = 0.0f;

    if (generateMap && m_TilesetWidth > 0 && m_TilesetHeight > 0)
        GenerateDefaultMap();
}

void Tilemap::SetTile(int x, int y, int tileID)
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 0)
    {
        m_Layers[0].tiles[static_cast<size_t>(y * m_MapWidth + x)] = tileID;
    }
}

int Tilemap::GetTile(int x, int y) const
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 0)
    {
        return m_Layers[0].tiles[static_cast<size_t>(y * m_MapWidth + x)];
    }
    return -1; // Invalid sentinel value
}

void Tilemap::SetTileCollision(int x, int y, bool hasCollision)
{
    m_CollisionMap.SetCollision(x, y, hasCollision);
}

bool Tilemap::GetTileCollision(int x, int y) const
{
    return m_CollisionMap.HasCollision(x, y);
}

void Tilemap::SetCornerCutBlocked(int x, int y, Corner corner, bool blocked)
{
    if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return;
    size_t idx = static_cast<size_t>(y * m_MapWidth + x);
    if (idx >= m_CornerCutBlocked.size())
        return;

    uint8_t bit = 1 << static_cast<uint8_t>(corner);
    if (blocked)
        m_CornerCutBlocked[idx] |= bit;
    else
        m_CornerCutBlocked[idx] &= ~bit;
}

bool Tilemap::IsCornerCutBlocked(int x, int y, Corner corner) const
{
    if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return false;
    size_t idx = static_cast<size_t>(y * m_MapWidth + x);
    if (idx >= m_CornerCutBlocked.size())
        return false;

    uint8_t bit = 1 << static_cast<uint8_t>(corner);
    return (m_CornerCutBlocked[idx] & bit) != 0;
}

uint8_t Tilemap::GetCornerCutMask(int x, int y) const
{
    if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return 0;
    size_t idx = static_cast<size_t>(y * m_MapWidth + x);
    if (idx >= m_CornerCutBlocked.size())
        return 0;
    return m_CornerCutBlocked[idx];
}

void Tilemap::SetCornerCutMask(int x, int y, uint8_t mask)
{
    if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return;
    size_t idx = static_cast<size_t>(y * m_MapWidth + x);
    if (idx >= m_CornerCutBlocked.size())
        return;
    m_CornerCutBlocked[idx] = mask & 0x0F; // Only use lower 4 bits
}

void Tilemap::SetNavigation(int x, int y, bool walkable)
{
    m_NavigationMap.SetNavigation(x, y, walkable);
}

bool Tilemap::GetNavigation(int x, int y) const
{
    return m_NavigationMap.GetNavigation(x, y);
}

void Tilemap::SetTileRotation(int x, int y, float rotation)
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 0)
    {
        // Normalize rotation to [0, 360) range using modular arithmetic
        while (rotation < 0.0f)
            rotation += 360.0f;
        while (rotation >= 360.0f)
            rotation -= 360.0f;
        m_Layers[0].rotation[static_cast<size_t>(y * m_MapWidth + x)] = rotation;
    }
}

float Tilemap::GetTileRotation(int x, int y) const
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 0)
    {
        return m_Layers[0].rotation[static_cast<size_t>(y * m_MapWidth + x)];
    }
    return 0.0f;
}

void Tilemap::SetTile2(int x, int y, int tileID)
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 1)
    {
        m_Layers[1].tiles[static_cast<size_t>(y * m_MapWidth + x)] = tileID;
    }
}

int Tilemap::GetTile2(int x, int y) const
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 1)
    {
        return m_Layers[1].tiles[static_cast<size_t>(y * m_MapWidth + x)];
    }
    return 0; // Out of bounds = empty (transparent)
}

void Tilemap::SetTileRotation2(int x, int y, float rotation)
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 1)
    {
        // Normalize rotation to 0-360 range
        while (rotation < 0.0f)
            rotation += 360.0f;
        while (rotation >= 360.0f)
            rotation -= 360.0f;
        m_Layers[1].rotation[static_cast<size_t>(y * m_MapWidth + x)] = rotation;
    }
}

float Tilemap::GetTileRotation2(int x, int y) const
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 1)
    {
        return m_Layers[1].rotation[static_cast<size_t>(y * m_MapWidth + x)];
    }
    return 0.0f; // Out of bounds = no rotation
}

// Layer 3 functions
void Tilemap::SetTile3(int x, int y, int tileID)
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 2)
    {
        m_Layers[2].tiles[static_cast<size_t>(y * m_MapWidth + x)] = tileID;
    }
}

int Tilemap::GetTile3(int x, int y) const
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 2)
    {
        return m_Layers[2].tiles[static_cast<size_t>(y * m_MapWidth + x)];
    }
    return 0; // Out of bounds = empty (transparent)
}

void Tilemap::SetTileRotation3(int x, int y, float rotation)
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 2)
    {
        // Normalize rotation to 0-360 range
        while (rotation < 0.0f)
            rotation += 360.0f;
        while (rotation >= 360.0f)
            rotation -= 360.0f;
        m_Layers[2].rotation[static_cast<size_t>(y * m_MapWidth + x)] = rotation;
    }
}

float Tilemap::GetTileRotation3(int x, int y) const
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 2)
    {
        return m_Layers[2].rotation[static_cast<size_t>(y * m_MapWidth + x)];
    }
    return 0.0f; // Out of bounds = no rotation
}

// Layer 4 functions
void Tilemap::SetTile4(int x, int y, int tileID)
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 3)
    {
        m_Layers[3].tiles[static_cast<size_t>(y * m_MapWidth + x)] = tileID;
    }
}

int Tilemap::GetTile4(int x, int y) const
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 3)
    {
        return m_Layers[3].tiles[static_cast<size_t>(y * m_MapWidth + x)];
    }
    return 0; // Out of bounds = empty (transparent)
}

void Tilemap::SetTileRotation4(int x, int y, float rotation)
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 3)
    {
        // Normalize rotation to 0-360 range
        while (rotation < 0.0f)
            rotation += 360.0f;
        while (rotation >= 360.0f)
            rotation -= 360.0f;
        m_Layers[3].rotation[static_cast<size_t>(y * m_MapWidth + x)] = rotation;
    }
}

float Tilemap::GetTileRotation4(int x, int y) const
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 3)
    {
        return m_Layers[3].rotation[static_cast<size_t>(y * m_MapWidth + x)];
    }
    return 0.0f; // Out of bounds = no rotation
}

// Layer 5 functions
void Tilemap::SetTile5(int x, int y, int tileID)
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 4)
    {
        m_Layers[4].tiles[static_cast<size_t>(y * m_MapWidth + x)] = tileID;
    }
}

int Tilemap::GetTile5(int x, int y) const
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 4)
    {
        return m_Layers[4].tiles[static_cast<size_t>(y * m_MapWidth + x)];
    }
    return 0; // Out of bounds = empty (transparent)
}

void Tilemap::SetTileRotation5(int x, int y, float rotation)
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 4)
    {
        // Normalize rotation to 0-360 range
        while (rotation < 0.0f)
            rotation += 360.0f;
        while (rotation >= 360.0f)
            rotation -= 360.0f;
        m_Layers[4].rotation[static_cast<size_t>(y * m_MapWidth + x)] = rotation;
    }
}

float Tilemap::GetTileRotation5(int x, int y) const
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 4)
    {
        return m_Layers[4].rotation[static_cast<size_t>(y * m_MapWidth + x)];
    }
    return 0.0f; // Out of bounds = no rotation
}

// Layer 6 functions
void Tilemap::SetTile6(int x, int y, int tileID)
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 5)
    {
        m_Layers[5].tiles[static_cast<size_t>(y * m_MapWidth + x)] = tileID;
    }
}

int Tilemap::GetTile6(int x, int y) const
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 5)
    {
        return m_Layers[5].tiles[static_cast<size_t>(y * m_MapWidth + x)];
    }
    return 0; // Out of bounds = empty (transparent)
}

void Tilemap::SetTileRotation6(int x, int y, float rotation)
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 5)
    {
        // Normalize rotation to 0-360 range
        while (rotation < 0.0f)
            rotation += 360.0f;
        while (rotation >= 360.0f)
            rotation -= 360.0f;
        m_Layers[5].rotation[static_cast<size_t>(y * m_MapWidth + x)] = rotation;
    }
}

float Tilemap::GetTileRotation6(int x, int y) const
{
    if (x >= 0 && x < m_MapWidth && y >= 0 && y < m_MapHeight && m_Layers.size() > 5)
    {
        return m_Layers[5].rotation[static_cast<size_t>(y * m_MapWidth + x)];
    }
    return 0.0f; // Out of bounds = no rotation
}

bool Tilemap::IsTileTransparent(int tileID) const
{
    // Use cached result if available (massive performance improvement)
    if (m_TransparencyCacheBuilt && tileID >= 0 &&
        tileID < static_cast<int>(m_TileTransparencyCache.size()))
    {
        return m_TileTransparencyCache[tileID];
    }

    // Fallback to pixel scanning if cache not available
    if (!m_TilesetData || tileID < 0 || m_TilesetChannels == 0)
    {
        return true; // Treat as transparent if we can't check
    }

    int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
    int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;

    if (tilesetX + m_TileWidth > m_TilesetDataWidth ||
        tilesetY + m_TileHeight > m_TilesetDataHeight)
    {
        return true;
    }

    for (int y = 0; y < m_TileHeight; ++y)
    {
        for (int x = 0; x < m_TileWidth; ++x)
        {
            int px = tilesetX + x;
            int py = tilesetY + y;
            if (px >= m_TilesetDataWidth || py >= m_TilesetDataHeight)
                continue;

            int index = (py * m_TilesetDataWidth + px) * m_TilesetChannels;
            if (index >= 0 && index < m_TilesetDataWidth * m_TilesetDataHeight * m_TilesetChannels)
            {
                if (m_TilesetChannels == 4)
                {
                    if (m_TilesetData[index + 3] > 0)
                        return false;
                }
                else if (m_TilesetChannels == 3)
                {
                    unsigned char r = m_TilesetData[index];
                    unsigned char g = m_TilesetData[index + 1];
                    unsigned char b = m_TilesetData[index + 2];
                    if (!(r == 0 && g == 0 && b == 0) && !(r == 255 && g == 255 && b == 255))
                        return false;
                }
            }
        }
    }
    return true;
}

int Tilemap::GetElevation(int x, int y) const
{
    if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return 0;

    int index = y * m_MapWidth + x;
    if (index < 0 || index >= static_cast<int>(m_Elevation.size()))
        return 0;

    return m_Elevation[index];
}

void Tilemap::SetElevation(int x, int y, int elevation)
{
    if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return;

    int index = y * m_MapWidth + x;
    if (index < 0 || index >= static_cast<int>(m_Elevation.size()))
        return;

    m_Elevation[index] = elevation;
}

float Tilemap::GetElevationAtWorldPos(float worldX, float worldY) const
{
    // Convert world position to tile coordinates
    // Note: Entity positions use "feet position" convention where Y is at the
    // bottom of the tile (y * tileHeight + tileHeight). We subtract half a tile
    // height to correctly map feet position back to the occupied tile.
    int tileX = static_cast<int>(std::floor(worldX / m_TileWidth));
    int tileY = static_cast<int>(std::floor((worldY - m_TileHeight * 0.5f) / m_TileHeight));

    // Return elevation of current tile
    return static_cast<float>(GetElevation(tileX, tileY));
}

bool Tilemap::GetNoProjection(int x, int y, int layer) const
{
    if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return false;

    // Convert 1-indexed layer to 0-indexed dynamic layer
    size_t layerIdx = static_cast<size_t>(layer - 1);
    if (layerIdx >= m_Layers.size())
        return false;

    size_t index = static_cast<size_t>(y * m_MapWidth + x);
    return m_Layers[layerIdx].noProjection[index];
}

void Tilemap::SetNoProjection(int x, int y, bool noProjection, int layer)
{
    if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return;

    // Convert 1-indexed layer to 0-indexed dynamic layer
    size_t layerIdx = static_cast<size_t>(layer - 1);
    if (layerIdx >= m_Layers.size())
        return;

    size_t index = static_cast<size_t>(y * m_MapWidth + x);
    m_Layers[layerIdx].noProjection[index] = noProjection;
}

bool Tilemap::FindNoProjectionStructureBounds(int tileX, int tileY,
                                              int &outMinX, int &outMaxX,
                                              int &outMinY, int &outMaxY) const
{
    if (tileX < 0 || tileX >= m_MapWidth || tileY < 0 || tileY >= m_MapHeight)
        return false;

    // Check if this tile has noProjection in ANY layer
    size_t idx = static_cast<size_t>(tileY * m_MapWidth + tileX);
    bool hasNoProj = false;
    for (size_t li = 0; li < m_Layers.size(); ++li)
    {
        if (idx < m_Layers[li].noProjection.size() && m_Layers[li].noProjection[idx])
        {
            hasNoProj = true;
            break;
        }
    }
    if (!hasNoProj)
        return false;

    // Flood-fill to find all connected noProjection tiles (same as RenderLayerNoProjection)
    std::vector<bool> processed(static_cast<size_t>(m_MapWidth * m_MapHeight), false);
    std::vector<std::pair<int, int>> stack;
    stack.push_back({tileX, tileY});

    outMinX = tileX;
    outMaxX = tileX;
    outMinY = tileY;
    outMaxY = tileY;

    while (!stack.empty())
    {
        auto [cx, cy] = stack.back();
        stack.pop_back();

        if (cx < 0 || cx >= m_MapWidth || cy < 0 || cy >= m_MapHeight)
            continue;

        size_t cIdx = static_cast<size_t>(cy * m_MapWidth + cx);
        if (processed[cIdx])
            continue;

        // Check if this tile is no-projection in ANY layer
        bool isNoProj = false;
        for (size_t li = 0; li < m_Layers.size(); ++li)
        {
            if (cIdx < m_Layers[li].noProjection.size() && m_Layers[li].noProjection[cIdx])
            {
                isNoProj = true;
                break;
            }
        }
        if (!isNoProj)
            continue;

        processed[cIdx] = true;

        outMinX = std::min(outMinX, cx);
        outMaxX = std::max(outMaxX, cx);
        outMinY = std::min(outMinY, cy);
        outMaxY = std::max(outMaxY, cy);

        // 4-way connectivity
        stack.push_back({cx - 1, cy});
        stack.push_back({cx + 1, cy});
        stack.push_back({cx, cy - 1});
        stack.push_back({cx, cy + 1});
    }

    return true;
}

bool Tilemap::GetYSorted(int x, int y, int layer) const
{
    if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return false;

    // Convert 1-indexed layer to 0-indexed dynamic layer
    size_t layerIdx = static_cast<size_t>(layer - 1);
    if (layerIdx >= m_Layers.size())
        return false;

    size_t index = static_cast<size_t>(y * m_MapWidth + x);
    return m_Layers[layerIdx].ySorted[index];
}

void Tilemap::SetYSorted(int x, int y, bool ySorted, int layer)
{
    if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return;

    // Convert 1-indexed layer to 0-indexed dynamic layer
    size_t layerIdx = static_cast<size_t>(layer - 1);
    if (layerIdx >= m_Layers.size())
        return;

    size_t index = static_cast<size_t>(y * m_MapWidth + x);
    m_Layers[layerIdx].ySorted[index] = ySorted;
}

std::vector<Tilemap::YSortedTile> Tilemap::GetVisibleYSortedTiles(glm::vec2 cullCam, glm::vec2 cullSize) const
{
    std::vector<YSortedTile> result;

    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    // Helper to check if a tile at (x,y,layer) is Y-sorted and non-empty using dynamic layers
    auto isYSortedTile = [this](int x, int y, size_t layerIdx) -> bool
    {
        if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
            return false;
        if (layerIdx >= m_Layers.size())
            return false;
        size_t index = static_cast<size_t>(y * m_MapWidth + x);
        const TileLayer &layer = m_Layers[layerIdx];
        if (index >= layer.ySorted.size())
            return false;
        if (!layer.ySorted[index])
            return false;
        if (layer.tiles[index] < 0)
            return false;
        return true;
    };

    // Check all dynamic layers for Y-sorted tiles
    for (size_t layerIdx = 0; layerIdx < m_Layers.size(); ++layerIdx)
    {
        const TileLayer &layer = m_Layers[layerIdx];

        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                size_t index = static_cast<size_t>(y * m_MapWidth + x);
                if (index >= layer.ySorted.size())
                    continue;

                if (!layer.ySorted[index])
                    continue;

                // Skip empty tiles
                int tileID = layer.tiles[index];
                if (tileID < 0)
                    continue;

                // Find the bottom-most Y-sorted tile in this column (same x, same layer)
                // This groups vertically stacked tiles to sort together
                int bottomY = y;
                while (isYSortedTile(x, bottomY + 1, layerIdx))
                {
                    bottomY++;
                }

                YSortedTile tile;
                tile.x = x;
                tile.y = y;
                tile.layer = static_cast<int>(layerIdx); // Store dynamic layer index
                // Use bottom tile's anchorY so entire vertical stack sorts together
                tile.anchorY = static_cast<float>((bottomY + 1) * m_TileHeight);
                // Check if this tile has no-projection flag
                tile.noProjection = layer.noProjection[index];
                result.push_back(tile);
            }
        }
    }

    return result;
}

void Tilemap::RenderSingleTile(IRenderer &renderer, int x, int y, int layer, glm::vec2 cameraPos)
{
    if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return;

    size_t layerIdx = static_cast<size_t>(layer);
    if (layerIdx >= m_Layers.size())
        return;

    size_t index = static_cast<size_t>(y * m_MapWidth + x);
    const TileLayer &tileLayer = m_Layers[layerIdx];

    if (index >= tileLayer.tiles.size())
        return;

    int tileID = tileLayer.tiles[index];
    float rotation = tileLayer.rotation[index];

    if (tileID < 0)
        return;

    if (IsTileTransparent(tileID))
        return;

    // Check if this tile has no-projection flag
    bool isNoProjection = tileLayer.noProjection[index];

    int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
    int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;
    glm::vec2 texCoord(static_cast<float>(tilesetX), static_cast<float>(tilesetY));
    glm::vec2 texSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));
    bool flipY = renderer.RequiresYFlip();

    if (isNoProjection)
    {
        // Check if perspective is enabled
        bool perspectiveEnabled = renderer.GetPerspectiveState().enabled;

        if (!perspectiveEnabled)
        {
            // 2D mode: render directly like normal tiles
            float worldX = static_cast<float>(x * m_TileWidth);
            float worldY = static_cast<float>(y * m_TileHeight);
            glm::vec2 screenPos(worldX - cameraPos.x, worldY - cameraPos.y);
            glm::vec2 renderSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));

            renderer.DrawSpriteRegion(m_TilesetTexture, screenPos, renderSize,
                                      texCoord, texSize, rotation, glm::vec3(1.0f), flipY);
        }
        else
        {
            // 3D mode: find structure, project anchor, render at integer offset

            // Flood-fill to find the structure's bounding box
            std::vector<bool> visited(static_cast<size_t>(m_MapWidth * m_MapHeight), false);
            std::vector<std::pair<int, int>> stack;
            stack.push_back({x, y});

            int minX = x, maxX = x, minY = y, maxY = y;

            while (!stack.empty())
            {
                auto [cx, cy] = stack.back();
                stack.pop_back();

                if (cx < 0 || cx >= m_MapWidth || cy < 0 || cy >= m_MapHeight)
                    continue;

                size_t cIdx = static_cast<size_t>(cy * m_MapWidth + cx);
                if (visited[cIdx])
                    continue;

                // Check if no-projection in any layer
                bool isNoProj = false;
                for (size_t li = 0; li < m_Layers.size(); ++li)
                {
                    if (cIdx < m_Layers[li].noProjection.size() && m_Layers[li].noProjection[cIdx])
                    {
                        isNoProj = true;
                        break;
                    }
                }
                if (!isNoProj)
                    continue;

                visited[cIdx] = true;

                minX = std::min(minX, cx);
                maxX = std::max(maxX, cx);
                minY = std::min(minY, cy);
                maxY = std::max(maxY, cy);

                stack.push_back({cx - 1, cy});
                stack.push_back({cx + 1, cy});
                stack.push_back({cx, cy - 1});
                stack.push_back({cx, cy + 1});
            }

            // Project structure base anchors once to pin X across the stack
            int leftPixelX = minX * m_TileWidth;
            int rightPixelX = (maxX + 1) * m_TileWidth;
            int bottomPixelY = (maxY + 1) * m_TileHeight;

            glm::vec2 baseProjLeft = renderer.ProjectPoint(
                glm::vec2(static_cast<float>(leftPixelX) - cameraPos.x,
                          static_cast<float>(bottomPixelY) - cameraPos.y));
            glm::vec2 baseProjRight = renderer.ProjectPoint(
                glm::vec2(static_cast<float>(rightPixelX) - cameraPos.x,
                          static_cast<float>(bottomPixelY) - cameraPos.y));

            float tilesWide = static_cast<float>(maxX - minX + 1);
            float baseSpanX = baseProjRight.x - baseProjLeft.x;
            float baseTileWidth = (tilesWide > 0.0f) ? (baseSpanX / tilesWide) : static_cast<float>(m_TileWidth);
            if (baseTileWidth == 0.0f)
                baseTileWidth = static_cast<float>(m_TileWidth);

            // Project per-tile anchors directly to avoid drift: project bottom-left/right and top-left of this tile
            float worldBLX = static_cast<float>(x * m_TileWidth);
            float worldBLY = static_cast<float>((y + 1) * m_TileHeight);
            float worldTLX = worldBLX;
            float worldTLY = static_cast<float>(y * m_TileHeight);

            glm::vec2 projBL = renderer.ProjectPoint(glm::vec2(worldBLX - cameraPos.x, worldBLY - cameraPos.y));
            glm::vec2 projBR = renderer.ProjectPoint(glm::vec2(worldBLX + m_TileWidth - cameraPos.x, worldBLY - cameraPos.y));
            glm::vec2 projTL = renderer.ProjectPoint(glm::vec2(worldTLX - cameraPos.x, worldTLY - cameraPos.y));

            float tileDrawWidth = projBR.x - projBL.x;
            float tileDrawHeight = projBL.y - projTL.y; // should be positive

            // Fallbacks for degenerate dimensions
            if (tileDrawWidth == 0.0f)
                tileDrawWidth = static_cast<float>(m_TileWidth);
            if (tileDrawHeight == 0.0f)
                tileDrawHeight = static_cast<float>(m_TileHeight);

            // Column alignment: always anchor to the base-row projection for this column
            glm::vec2 baseColBL = renderer.ProjectPoint(glm::vec2(worldBLX - cameraPos.x,
                                                                  static_cast<float>((maxY + 1) * m_TileHeight) - cameraPos.y));
            glm::vec2 baseColBR = renderer.ProjectPoint(glm::vec2(worldBLX + m_TileWidth - cameraPos.x,
                                                                  static_cast<float>((maxY + 1) * m_TileHeight) - cameraPos.y));
            float colBaseWidth = baseColBR.x - baseColBL.x;
            if (colBaseWidth == 0.0f)
                colBaseWidth = static_cast<float>(m_TileWidth);

            // Stack rows directly on the base column span (no bias)
            glm::vec2 screenPos(baseColBL.x, projTL.y);
            glm::vec2 renderSize(colBaseWidth, tileDrawHeight);

            renderer.SuspendPerspective(true);
            renderer.DrawSpriteRegion(m_TilesetTexture, screenPos, renderSize,
                                      texCoord, texSize, rotation, glm::vec3(1.0f), flipY);
            renderer.SuspendPerspective(false);
        }
    }
    else
    {
        // Normal rendering: let renderer handle perspective
        float worldX = static_cast<float>(x * m_TileWidth);
        float worldY = static_cast<float>(y * m_TileHeight);
        glm::vec2 screenPos(worldX - cameraPos.x, worldY - cameraPos.y);

        glm::vec2 renderSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));
        renderer.DrawSpriteRegion(m_TilesetTexture, screenPos, renderSize,
                                  texCoord, texSize, rotation, glm::vec3(1.0f), flipY);
    }
}

// 4-parameter version: renderCam for positioning, cullCam/cullSize for visibility
void Tilemap::Render(IRenderer &renderer,
                     glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                     glm::vec2 cullCam, glm::vec2 cullSize)
{
    // Use cullCam/cullSize to determine which tiles are visible
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    // Pre-compute constants outside loop
    const int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    const int mapWidth = m_MapWidth;
    const int tileW = m_TileWidth;
    const int tileH = m_TileHeight;
    const float tileWf = static_cast<float>(tileW);
    const float tileHf = static_cast<float>(tileH);
    const glm::vec2 texSize(tileWf, tileHf);
    const float seamFix = renderer.GetPerspectiveState().enabled ? 0.1f : 0.05f;
    const glm::vec2 tileSizeRender(16.0f + seamFix, 16.0f + seamFix);
    const bool flipY = renderer.RequiresYFlip();
    const glm::vec3 white(1.0f);
    const TileLayer &layer = m_Layers[0];
    const int *tiles = layer.tiles.data();
    const float *rotations = layer.rotation.data();
    const bool hasTransparencyCache = m_TransparencyCacheBuilt;
    const std::vector<bool> &transparencyCache = m_TileTransparencyCache;
    const int transparencyCacheSize = static_cast<int>(transparencyCache.size());
    const std::vector<bool> &noProjection = layer.noProjection;
    const std::vector<bool> &ySorted = layer.ySorted;

    // Iterate over visible tiles
    // Debug: track if we render any animated tiles (print once per second)
    static float animDebugTimer = 0.0f;
    static int animTilesRendered = 0;
    animDebugTimer += 0.016f; // Approximate frame time
    bool shouldPrintDebug = animDebugTimer > 1.0f;
    if (shouldPrintDebug)
    {
        animDebugTimer = 0.0f;
        animTilesRendered = 0;
    }

    for (int y = y0; y <= y1; ++y)
    {
        const int rowOffset = y * mapWidth;
        const float tilePosY = y * tileHf - renderCam.y;

        for (int x = x0; x <= x1; ++x)
        {
            const int idx = rowOffset + x;
            int tileID = tiles[idx];

            // Skip empty tiles (negative means invalid/empty)
            if (tileID < 0)
                continue;

            // Skip no-projection tiles (rendered separately without 3D perspective)
            if (noProjection[idx])
                continue;

            // Skip Y-sorted tiles (rendered in sorted pass with entities)
            if (ySorted[idx])
                continue;

            // Check for animated tile (per-layer animation map)
            if (idx < static_cast<int>(layer.animationMap.size()))
            {
                int animId = layer.animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    int origTileID = tileID;
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                    animTilesRendered++;
                    if (shouldPrintDebug && animTilesRendered == 1)
                    {
                        /*std::cout << "[DEBUG] Rendering anim tile: idx=" << idx
                                  << " animId=" << animId << " time=" << m_AnimationTime
                                  << " orig=" << origTileID << " frame=" << tileID << std::endl;*/
                    }
                }
            }

            // Skip fully transparent tiles using cache (no function call overhead)
            if (hasTransparencyCache && tileID < transparencyCacheSize && transparencyCache[tileID])
                continue;

            // Calculate screen position
            const float tilePosX = x * tileWf - renderCam.x;

            // Calculate tileset UV coordinates directly
            const int tilesetX = (tileID % dataTilesPerRow) * tileW;
            const int tilesetY = (tileID / dataTilesPerRow) * tileH;

            renderer.DrawSpriteRegion(m_TilesetTexture,
                                      glm::vec2(tilePosX, tilePosY),
                                      tileSizeRender,
                                      glm::vec2(static_cast<float>(tilesetX), static_cast<float>(tilesetY)),
                                      texSize,
                                      rotations[idx],
                                      white, flipY);
        }
    }
}

// 4-parameter overloads for Layer 2-6: renderCam for positioning, cullCam/cullSize for visibility
void Tilemap::RenderLayer2(IRenderer &renderer,
                           glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                           glm::vec2 cullCam, glm::vec2 cullSize)
{
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    const int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    const int mapWidth = m_MapWidth;
    const int tileW = m_TileWidth;
    const int tileH = m_TileHeight;
    const float tileWf = static_cast<float>(tileW);
    const float tileHf = static_cast<float>(tileH);
    const glm::vec2 texSize(tileWf, tileHf);
    const float seamFix = renderer.GetPerspectiveState().enabled ? 0.1f : 0.05f;
    const glm::vec2 tileSizeRender(16.0f + seamFix, 16.0f + seamFix);
    const bool flipY = renderer.RequiresYFlip();
    const glm::vec3 white(1.0f);
    const TileLayer &layer = m_Layers[1];
    const int *tiles = layer.tiles.data();
    const float *rotations = layer.rotation.data();
    const bool hasTransparencyCache = m_TransparencyCacheBuilt;
    const std::vector<bool> &transparencyCache = m_TileTransparencyCache;
    const int transparencyCacheSize = static_cast<int>(transparencyCache.size());
    const std::vector<bool> &noProjection = layer.noProjection;
    const std::vector<bool> &ySorted = layer.ySorted;

    // Debug: check for animated tiles in this layer once per second
    static float layer2Debug = 0.0f;
    layer2Debug += 0.0003f;
    bool shouldDebug = layer2Debug > 1.0f;
    if (shouldDebug)
        layer2Debug = 0.0f;

    for (int y = y0; y <= y1; ++y)
    {
        const int rowOffset = y * mapWidth;
        const float tilePosY = y * tileHf - renderCam.y;
        for (int x = x0; x <= x1; ++x)
        {
            const int idx = rowOffset + x;
            int tileID = tiles[idx];
            if (tileID < 0)
                continue;
            if (noProjection[idx])
                continue;
            if (ySorted[idx])
                continue;

            // Check for animated tile (per-layer animation map)
            if (idx < static_cast<int>(layer.animationMap.size()))
            {
                int animId = layer.animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    int origTile = tileID;
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                    if (shouldDebug)
                    {
                        /*std::cout << "[DEBUG] Layer2 ANIMATED idx=" << idx
                                  << " orig=" << origTile << " now=" << tileID << std::endl;*/
                    }
                }
            }

            if (hasTransparencyCache && tileID < transparencyCacheSize && transparencyCache[tileID])
                continue;

            const float tilePosX = x * tileWf - renderCam.x;
            const int tilesetX = (tileID % dataTilesPerRow) * tileW;
            const int tilesetY = (tileID / dataTilesPerRow) * tileH;
            renderer.DrawSpriteRegion(m_TilesetTexture, glm::vec2(tilePosX, tilePosY), tileSizeRender,
                                      glm::vec2(static_cast<float>(tilesetX), static_cast<float>(tilesetY)),
                                      texSize, rotations[idx], white, flipY);
        }
    }
}

void Tilemap::RenderLayer3(IRenderer &renderer,
                           glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                           glm::vec2 cullCam, glm::vec2 cullSize)
{
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    const int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    const int mapWidth = m_MapWidth;
    const int tileW = m_TileWidth;
    const int tileH = m_TileHeight;
    const float tileWf = static_cast<float>(tileW);
    const float tileHf = static_cast<float>(tileH);
    const glm::vec2 texSize(tileWf, tileHf);
    const float seamFix = renderer.GetPerspectiveState().enabled ? 0.1f : 0.05f;
    const glm::vec2 tileSizeRender(16.0f + seamFix, 16.0f + seamFix);
    const bool flipY = renderer.RequiresYFlip();
    const glm::vec3 white(1.0f);
    const TileLayer &layer = m_Layers[2];
    const int *tiles = layer.tiles.data();
    const float *rotations = layer.rotation.data();
    const bool hasTransparencyCache = m_TransparencyCacheBuilt;
    const std::vector<bool> &transparencyCache = m_TileTransparencyCache;
    const int transparencyCacheSize = static_cast<int>(transparencyCache.size());
    const std::vector<bool> &noProjection = layer.noProjection;
    const std::vector<bool> &ySorted = layer.ySorted;

    for (int y = y0; y <= y1; ++y)
    {
        const int rowOffset = y * mapWidth;
        const float tilePosY = y * tileHf - renderCam.y;
        for (int x = x0; x <= x1; ++x)
        {
            const int idx = rowOffset + x;
            int tileID = tiles[idx];
            if (tileID < 0)
                continue;
            if (noProjection[idx])
                continue;
            if (ySorted[idx])
                continue;

            // Check for animated tile (per-layer animation map)
            if (idx < static_cast<int>(layer.animationMap.size()))
            {
                int animId = layer.animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                }
            }

            if (hasTransparencyCache && tileID < transparencyCacheSize && transparencyCache[tileID])
                continue;

            const float tilePosX = x * tileWf - renderCam.x;
            const int tilesetX = (tileID % dataTilesPerRow) * tileW;
            const int tilesetY = (tileID / dataTilesPerRow) * tileH;
            renderer.DrawSpriteRegion(m_TilesetTexture, glm::vec2(tilePosX, tilePosY), tileSizeRender,
                                      glm::vec2(static_cast<float>(tilesetX), static_cast<float>(tilesetY)),
                                      texSize, rotations[idx], white, flipY);
        }
    }
}

void Tilemap::RenderLayer4(IRenderer &renderer,
                           glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                           glm::vec2 cullCam, glm::vec2 cullSize)
{
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    const int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    const int mapWidth = m_MapWidth;
    const int tileW = m_TileWidth;
    const int tileH = m_TileHeight;
    const float tileWf = static_cast<float>(tileW);
    const float tileHf = static_cast<float>(tileH);
    const glm::vec2 texSize(tileWf, tileHf);
    const float seamFix = renderer.GetPerspectiveState().enabled ? 0.1f : 0.05f;
    const glm::vec2 tileSizeRender(16.0f + seamFix, 16.0f + seamFix);
    const bool flipY = renderer.RequiresYFlip();
    const glm::vec3 white(1.0f);
    const TileLayer &layer = m_Layers[3];
    const int *tiles = layer.tiles.data();
    const float *rotations = layer.rotation.data();
    const bool hasTransparencyCache = m_TransparencyCacheBuilt;
    const std::vector<bool> &transparencyCache = m_TileTransparencyCache;
    const int transparencyCacheSize = static_cast<int>(transparencyCache.size());
    const std::vector<bool> &noProjection = layer.noProjection;
    const std::vector<bool> &ySorted = layer.ySorted;

    for (int y = y0; y <= y1; ++y)
    {
        const int rowOffset = y * mapWidth;
        const float tilePosY = y * tileHf - renderCam.y;
        for (int x = x0; x <= x1; ++x)
        {
            const int idx = rowOffset + x;
            int tileID = tiles[idx];
            if (tileID < 0)
                continue;
            if (noProjection[idx])
                continue;
            if (ySorted[idx])
                continue;

            // Check for animated tile (per-layer animation map)
            if (idx < static_cast<int>(layer.animationMap.size()))
            {
                int animId = layer.animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                }
            }

            if (hasTransparencyCache && tileID < transparencyCacheSize && transparencyCache[tileID])
                continue;

            const float tilePosX = x * tileWf - renderCam.x;
            const int tilesetX = (tileID % dataTilesPerRow) * tileW;
            const int tilesetY = (tileID / dataTilesPerRow) * tileH;
            renderer.DrawSpriteRegion(m_TilesetTexture, glm::vec2(tilePosX, tilePosY), tileSizeRender,
                                      glm::vec2(static_cast<float>(tilesetX), static_cast<float>(tilesetY)),
                                      texSize, rotations[idx], white, flipY);
        }
    }
}

void Tilemap::RenderLayer5(IRenderer &renderer,
                           glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                           glm::vec2 cullCam, glm::vec2 cullSize)
{
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    const int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    const int mapWidth = m_MapWidth;
    const int tileW = m_TileWidth;
    const int tileH = m_TileHeight;
    const float tileWf = static_cast<float>(tileW);
    const float tileHf = static_cast<float>(tileH);
    const glm::vec2 texSize(tileWf, tileHf);
    const float seamFix = renderer.GetPerspectiveState().enabled ? 0.1f : 0.05f;
    const glm::vec2 tileSizeRender(16.0f + seamFix, 16.0f + seamFix);
    const bool flipY = renderer.RequiresYFlip();
    const glm::vec3 white(1.0f);
    const TileLayer &layer = m_Layers[4];
    const int *tiles = layer.tiles.data();
    const float *rotations = layer.rotation.data();
    const bool hasTransparencyCache = m_TransparencyCacheBuilt;
    const std::vector<bool> &transparencyCache = m_TileTransparencyCache;
    const int transparencyCacheSize = static_cast<int>(transparencyCache.size());
    const std::vector<bool> &noProjection = layer.noProjection;
    const std::vector<bool> &ySorted = layer.ySorted;

    for (int y = y0; y <= y1; ++y)
    {
        const int rowOffset = y * mapWidth;
        const float tilePosY = y * tileHf - renderCam.y;
        for (int x = x0; x <= x1; ++x)
        {
            const int idx = rowOffset + x;
            int tileID = tiles[idx];
            if (tileID < 0)
                continue;
            if (noProjection[idx])
                continue;
            if (ySorted[idx])
                continue;

            // Check for animated tile (per-layer animation map)
            if (idx < static_cast<int>(layer.animationMap.size()))
            {
                int animId = layer.animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                }
            }

            if (hasTransparencyCache && tileID < transparencyCacheSize && transparencyCache[tileID])
                continue;

            const float tilePosX = x * tileWf - renderCam.x;
            const int tilesetX = (tileID % dataTilesPerRow) * tileW;
            const int tilesetY = (tileID / dataTilesPerRow) * tileH;
            renderer.DrawSpriteRegion(m_TilesetTexture, glm::vec2(tilePosX, tilePosY), tileSizeRender,
                                      glm::vec2(static_cast<float>(tilesetX), static_cast<float>(tilesetY)),
                                      texSize, rotations[idx], white, flipY);
        }
    }
}

void Tilemap::RenderLayer6(IRenderer &renderer,
                           glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                           glm::vec2 cullCam, glm::vec2 cullSize)
{
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    const int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    const int mapWidth = m_MapWidth;
    const int tileW = m_TileWidth;
    const int tileH = m_TileHeight;
    const float tileWf = static_cast<float>(tileW);
    const float tileHf = static_cast<float>(tileH);
    const glm::vec2 texSize(tileWf, tileHf);
    const float seamFix = renderer.GetPerspectiveState().enabled ? 0.1f : 0.05f;
    const glm::vec2 tileSizeRender(16.0f + seamFix, 16.0f + seamFix);
    const bool flipY = renderer.RequiresYFlip();
    const glm::vec3 white(1.0f);
    const TileLayer &layer = m_Layers[5];
    const int *tiles = layer.tiles.data();
    const float *rotations = layer.rotation.data();
    const bool hasTransparencyCache = m_TransparencyCacheBuilt;
    const std::vector<bool> &transparencyCache = m_TileTransparencyCache;
    const int transparencyCacheSize = static_cast<int>(transparencyCache.size());
    const std::vector<bool> &noProjection = layer.noProjection;
    const std::vector<bool> &ySorted = layer.ySorted;

    for (int y = y0; y <= y1; ++y)
    {
        const int rowOffset = y * mapWidth;
        const float tilePosY = y * tileHf - renderCam.y;
        for (int x = x0; x <= x1; ++x)
        {
            const int idx = rowOffset + x;
            int tileID = tiles[idx];
            if (tileID < 0)
                continue;
            if (noProjection[idx])
                continue;
            if (ySorted[idx])
                continue;

            // Check for animated tile (per-layer animation map)
            if (idx < static_cast<int>(layer.animationMap.size()))
            {
                int animId = layer.animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                }
            }

            if (hasTransparencyCache && tileID < transparencyCacheSize && transparencyCache[tileID])
                continue;

            const float tilePosX = x * tileWf - renderCam.x;
            const int tilesetX = (tileID % dataTilesPerRow) * tileW;
            const int tilesetY = (tileID / dataTilesPerRow) * tileH;
            renderer.DrawSpriteRegion(m_TilesetTexture, glm::vec2(tilePosX, tilePosY), tileSizeRender,
                                      glm::vec2(static_cast<float>(tilesetX), static_cast<float>(tilesetY)),
                                      texSize, rotations[idx], white, flipY);
        }
    }
}

// Render Layer 1 tiles that have no-projection flag set (call with suspended perspective)
// These tiles are positioned according to the 3D projection but rendered upright (not distorted)
void Tilemap::RenderNoProjection(IRenderer &renderer,
                                 glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                                 glm::vec2 cullCam, glm::vec2 cullSize)
{
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            // Only render tiles with no-projection flag for layer 1
            if (!GetNoProjection(x, y, 1))
                continue;

            int tileID = GetTile(x, y);
            if (tileID < 0)
                continue;

            // Check for animated tile (per-layer animation map - layer 0)
            int idx = y * m_MapWidth + x;
            if (idx < static_cast<int>(m_Layers[0].animationMap.size()))
            {
                int animId = m_Layers[0].animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                }
            }

            if (IsTileTransparent(tileID))
                continue;

            // Calculate tile's base position (bottom-center) in screen space
            float baseX = x * m_TileWidth + m_TileWidth * 0.5f - renderCam.x;
            float baseY = (y + 1) * m_TileHeight - renderCam.y; // Bottom of tile

            // Project the base point through the perspective (like character feet)
            glm::vec2 projectedBase = renderer.ProjectPoint(glm::vec2(baseX, baseY));

            // Draw tile at full size, positioned so its bottom-center is at the projected point
            glm::vec2 tilePos(projectedBase.x - m_TileWidth * 0.5f, projectedBase.y - m_TileHeight);

            int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
            int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
            int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;

            glm::vec2 texCoord(static_cast<float>(tilesetX), static_cast<float>(tilesetY));
            glm::vec2 texSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));
            glm::vec2 tileRenderSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));
            float tileRotation = GetTileRotation(x, y);

            bool flipY = renderer.RequiresYFlip();
            renderer.DrawSpriteRegion(m_TilesetTexture, tilePos,
                                      tileRenderSize, texCoord, texSize, tileRotation, glm::vec3(1.0f), flipY);
        }
    }
}

// Render Layer 2 tiles that have no-projection flag set
void Tilemap::RenderLayer2NoProjection(IRenderer &renderer,
                                       glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                                       glm::vec2 cullCam, glm::vec2 cullSize)
{
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            if (!GetNoProjection(x, y, 2))
                continue;

            int tileID = GetTile2(x, y);
            if (tileID < 0)
                continue;

            // Check for animated tile (per-layer animation map - layer 1)
            int idx = y * m_MapWidth + x;
            if (idx < static_cast<int>(m_Layers[1].animationMap.size()))
            {
                int animId = m_Layers[1].animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                }
            }

            if (IsTileTransparent(tileID))
                continue;

            // Find bottom of no-projection structure across ALL layers
            int bottomY = y;
            while (bottomY + 1 < m_MapHeight)
            {
                bool hasNoProjectionBelow = GetNoProjection(x, bottomY + 1, 1) ||
                                            GetNoProjection(x, bottomY + 1, 2) ||
                                            GetNoProjection(x, bottomY + 1, 3) ||
                                            GetNoProjection(x, bottomY + 1, 4) ||
                                            GetNoProjection(x, bottomY + 1, 5) ||
                                            GetNoProjection(x, bottomY + 1, 6);
                if (!hasNoProjectionBelow)
                    break;
                bottomY++;
            }

            // Position in 2D screen space
            float baseX = x * m_TileWidth - renderCam.x;
            float bottomBaseY = bottomY * m_TileHeight - renderCam.y;

            // Use a fixed reference X (0) for consistent projection across structure
            glm::vec2 refPoint(0, bottomBaseY);
            glm::vec2 projectedRef = renderer.ProjectPoint(refPoint);

            // Calculate scale from bottom position
            glm::vec2 refP2(static_cast<float>(m_TileWidth), bottomBaseY);
            glm::vec2 projP2 = renderer.ProjectPoint(refP2);
            float scale = (projP2.x - projectedRef.x) / static_cast<float>(m_TileWidth);

            // Position tile: use projected reference + scaled offset from reference
            float tilesAboveBottom = static_cast<float>(bottomY - y);
            float tileScreenX = projectedRef.x + (baseX - 0) * scale;
            float tileScreenY = projectedRef.y - (tilesAboveBottom * m_TileHeight * scale);

            glm::vec2 tilePos(tileScreenX, tileScreenY);

            int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
            int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
            int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;
            tilesetX = (tilesetX / m_TileWidth) * m_TileWidth;
            tilesetY = (tilesetY / m_TileHeight) * m_TileHeight;

            glm::vec2 texCoord(static_cast<float>(tilesetX), static_cast<float>(tilesetY));
            glm::vec2 texSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));
            glm::vec2 renderSize(static_cast<float>(m_TileWidth) * scale, static_cast<float>(m_TileHeight) * scale);
            float tileRotation = GetTileRotation2(x, y);
            bool flipY = renderer.RequiresYFlip();
            renderer.DrawSpriteRegion(m_TilesetTexture, tilePos,
                                      renderSize, texCoord, texSize, tileRotation, glm::vec3(1.0f), flipY);
        }
    }
}

// Render Layer 3 tiles that have no-projection flag set
void Tilemap::RenderLayer3NoProjection(IRenderer &renderer,
                                       glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                                       glm::vec2 cullCam, glm::vec2 cullSize)
{
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            if (!GetNoProjection(x, y, 3))
                continue;

            int tileID = GetTile3(x, y);
            if (tileID < 0)
                continue;

            // Check for animated tile (per-layer animation map - layer 2)
            int idx = y * m_MapWidth + x;
            if (idx < static_cast<int>(m_Layers[2].animationMap.size()))
            {
                int animId = m_Layers[2].animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                }
            }

            if (IsTileTransparent(tileID))
                continue;

            // Find bottom of no-projection structure across ALL layers
            int bottomY = y;
            while (bottomY + 1 < m_MapHeight)
            {
                bool hasNoProjectionBelow = GetNoProjection(x, bottomY + 1, 1) ||
                                            GetNoProjection(x, bottomY + 1, 2) ||
                                            GetNoProjection(x, bottomY + 1, 3) ||
                                            GetNoProjection(x, bottomY + 1, 4) ||
                                            GetNoProjection(x, bottomY + 1, 5) ||
                                            GetNoProjection(x, bottomY + 1, 6);
                if (!hasNoProjectionBelow)
                    break;
                bottomY++;
            }

            // Position in 2D screen space
            float baseX = x * m_TileWidth - renderCam.x;
            float bottomBaseY = bottomY * m_TileHeight - renderCam.y;

            // Use a fixed reference X (0) for consistent projection across structure
            glm::vec2 refPoint(0, bottomBaseY);
            glm::vec2 projectedRef = renderer.ProjectPoint(refPoint);

            // Calculate scale from bottom position
            glm::vec2 refP2(static_cast<float>(m_TileWidth), bottomBaseY);
            glm::vec2 projP2 = renderer.ProjectPoint(refP2);
            float scale = (projP2.x - projectedRef.x) / static_cast<float>(m_TileWidth);

            // Position tile: use projected reference + scaled offset from reference
            float tilesAboveBottom = static_cast<float>(bottomY - y);
            float tileScreenX = projectedRef.x + (baseX - 0) * scale;
            float tileScreenY = projectedRef.y - (tilesAboveBottom * m_TileHeight * scale);

            glm::vec2 tilePos(tileScreenX, tileScreenY);

            int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
            int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
            int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;
            tilesetX = (tilesetX / m_TileWidth) * m_TileWidth;
            tilesetY = (tilesetY / m_TileHeight) * m_TileHeight;

            glm::vec2 texCoord(static_cast<float>(tilesetX), static_cast<float>(tilesetY));
            glm::vec2 texSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));
            glm::vec2 renderSize(static_cast<float>(m_TileWidth) * scale, static_cast<float>(m_TileHeight) * scale);
            float tileRotation = GetTileRotation3(x, y);
            bool flipY = renderer.RequiresYFlip();
            renderer.DrawSpriteRegion(m_TilesetTexture, tilePos,
                                      renderSize, texCoord, texSize, tileRotation, glm::vec3(1.0f), flipY);
        }
    }
}

// Render Layer 4 tiles that have no-projection flag set
void Tilemap::RenderLayer4NoProjection(IRenderer &renderer,
                                       glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                                       glm::vec2 cullCam, glm::vec2 cullSize)
{
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            if (!GetNoProjection(x, y, 4))
                continue;

            int tileID = GetTile4(x, y);
            if (tileID < 0)
                continue;

            // Check for animated tile (per-layer animation map - layer 3)
            int idx = y * m_MapWidth + x;
            if (idx < static_cast<int>(m_Layers[3].animationMap.size()))
            {
                int animId = m_Layers[3].animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                }
            }

            if (IsTileTransparent(tileID))
                continue;

            // Find bottom of no-projection structure across ALL layers
            int bottomY = y;
            while (bottomY + 1 < m_MapHeight)
            {
                bool hasNoProjectionBelow = GetNoProjection(x, bottomY + 1, 1) ||
                                            GetNoProjection(x, bottomY + 1, 2) ||
                                            GetNoProjection(x, bottomY + 1, 3) ||
                                            GetNoProjection(x, bottomY + 1, 4) ||
                                            GetNoProjection(x, bottomY + 1, 5) ||
                                            GetNoProjection(x, bottomY + 1, 6);
                if (!hasNoProjectionBelow)
                    break;
                bottomY++;
            }

            // Position in 2D screen space
            float baseX = x * m_TileWidth - renderCam.x;
            float bottomBaseY = bottomY * m_TileHeight - renderCam.y;

            // Use a fixed reference X (0) for consistent projection across structure
            glm::vec2 refPoint(0, bottomBaseY);
            glm::vec2 projectedRef = renderer.ProjectPoint(refPoint);

            // Calculate scale from bottom position
            glm::vec2 refP2(static_cast<float>(m_TileWidth), bottomBaseY);
            glm::vec2 projP2 = renderer.ProjectPoint(refP2);
            float scale = (projP2.x - projectedRef.x) / static_cast<float>(m_TileWidth);

            // Position tile: use projected reference + scaled offset from reference
            float tilesAboveBottom = static_cast<float>(bottomY - y);
            float tileScreenX = projectedRef.x + (baseX - 0) * scale;
            float tileScreenY = projectedRef.y - (tilesAboveBottom * m_TileHeight * scale);

            glm::vec2 tilePos(tileScreenX, tileScreenY);

            int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
            int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
            int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;
            tilesetX = (tilesetX / m_TileWidth) * m_TileWidth;
            tilesetY = (tilesetY / m_TileHeight) * m_TileHeight;

            glm::vec2 texCoord(static_cast<float>(tilesetX), static_cast<float>(tilesetY));
            glm::vec2 texSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));
            glm::vec2 renderSize(static_cast<float>(m_TileWidth) * scale, static_cast<float>(m_TileHeight) * scale);
            float tileRotation = GetTileRotation4(x, y);
            bool flipY = renderer.RequiresYFlip();
            renderer.DrawSpriteRegion(m_TilesetTexture, tilePos,
                                      renderSize, texCoord, texSize, tileRotation, glm::vec3(1.0f), flipY);
        }
    }
}

// Render Layer 5 tiles that have no-projection flag set
void Tilemap::RenderLayer5NoProjection(IRenderer &renderer,
                                       glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                                       glm::vec2 cullCam, glm::vec2 cullSize)
{
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            if (!GetNoProjection(x, y, 5))
                continue;

            int tileID = GetTile5(x, y);
            if (tileID < 0)
                continue;

            // Check for animated tile (per-layer animation map - layer 4)
            int idx = y * m_MapWidth + x;
            if (idx < static_cast<int>(m_Layers[4].animationMap.size()))
            {
                int animId = m_Layers[4].animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                }
            }

            if (IsTileTransparent(tileID))
                continue;

            // Find bottom of no-projection structure across ALL layers
            int bottomY = y;
            while (bottomY + 1 < m_MapHeight)
            {
                bool hasNoProjectionBelow = GetNoProjection(x, bottomY + 1, 1) ||
                                            GetNoProjection(x, bottomY + 1, 2) ||
                                            GetNoProjection(x, bottomY + 1, 3) ||
                                            GetNoProjection(x, bottomY + 1, 4) ||
                                            GetNoProjection(x, bottomY + 1, 5) ||
                                            GetNoProjection(x, bottomY + 1, 6);
                if (!hasNoProjectionBelow)
                    break;
                bottomY++;
            }

            // Position in 2D screen space
            float baseX = x * m_TileWidth - renderCam.x;
            float bottomBaseY = bottomY * m_TileHeight - renderCam.y;

            // Use a fixed reference X (0) for consistent projection across structure
            glm::vec2 refPoint(0, bottomBaseY);
            glm::vec2 projectedRef = renderer.ProjectPoint(refPoint);

            // Calculate scale from bottom position
            glm::vec2 refP2(static_cast<float>(m_TileWidth), bottomBaseY);
            glm::vec2 projP2 = renderer.ProjectPoint(refP2);
            float scale = (projP2.x - projectedRef.x) / static_cast<float>(m_TileWidth);

            // Position tile: use projected reference + scaled offset from reference
            float tilesAboveBottom = static_cast<float>(bottomY - y);
            float tileScreenX = projectedRef.x + (baseX - 0) * scale;
            float tileScreenY = projectedRef.y - (tilesAboveBottom * m_TileHeight * scale);

            glm::vec2 tilePos(tileScreenX, tileScreenY);

            int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
            int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
            int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;
            tilesetX = (tilesetX / m_TileWidth) * m_TileWidth;
            tilesetY = (tilesetY / m_TileHeight) * m_TileHeight;

            glm::vec2 texCoord(static_cast<float>(tilesetX), static_cast<float>(tilesetY));
            glm::vec2 texSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));
            glm::vec2 renderSize(static_cast<float>(m_TileWidth) * scale, static_cast<float>(m_TileHeight) * scale);
            float tileRotation = GetTileRotation5(x, y);
            bool flipY = renderer.RequiresYFlip();
            renderer.DrawSpriteRegion(m_TilesetTexture, tilePos,
                                      renderSize, texCoord, texSize, tileRotation, glm::vec3(1.0f), flipY);
        }
    }
}

// Render Layer 6 tiles that have no-projection flag set
void Tilemap::RenderLayer6NoProjection(IRenderer &renderer,
                                       glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                                       glm::vec2 cullCam, glm::vec2 cullSize)
{
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            if (!GetNoProjection(x, y, 6))
                continue;

            int tileID = GetTile6(x, y);
            if (tileID < 0)
                continue;

            // Check for animated tile (per-layer animation map - layer 5)
            int idx = y * m_MapWidth + x;
            if (idx < static_cast<int>(m_Layers[5].animationMap.size()))
            {
                int animId = m_Layers[5].animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                }
            }

            if (IsTileTransparent(tileID))
                continue;

            // Find bottom of no-projection structure across ALL layers
            int bottomY = y;
            while (bottomY + 1 < m_MapHeight)
            {
                bool hasNoProjectionBelow = GetNoProjection(x, bottomY + 1, 1) ||
                                            GetNoProjection(x, bottomY + 1, 2) ||
                                            GetNoProjection(x, bottomY + 1, 3) ||
                                            GetNoProjection(x, bottomY + 1, 4) ||
                                            GetNoProjection(x, bottomY + 1, 5) ||
                                            GetNoProjection(x, bottomY + 1, 6);
                if (!hasNoProjectionBelow)
                    break;
                bottomY++;
            }

            // Position in 2D screen space
            float baseX = x * m_TileWidth - renderCam.x;
            float bottomBaseY = bottomY * m_TileHeight - renderCam.y;

            // Use a fixed reference X (0) for consistent projection across structure
            glm::vec2 refPoint(0, bottomBaseY);
            glm::vec2 projectedRef = renderer.ProjectPoint(refPoint);

            // Calculate scale from bottom position
            glm::vec2 refP2(static_cast<float>(m_TileWidth), bottomBaseY);
            glm::vec2 projP2 = renderer.ProjectPoint(refP2);
            float scale = (projP2.x - projectedRef.x) / static_cast<float>(m_TileWidth);

            // Position tile: use projected reference + scaled offset from reference
            float tilesAboveBottom = static_cast<float>(bottomY - y);
            float tileScreenX = projectedRef.x + (baseX - 0) * scale;
            float tileScreenY = projectedRef.y - (tilesAboveBottom * m_TileHeight * scale);

            glm::vec2 tilePos(tileScreenX, tileScreenY);

            int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
            int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
            int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;
            tilesetX = (tilesetX / m_TileWidth) * m_TileWidth;
            tilesetY = (tilesetY / m_TileHeight) * m_TileHeight;

            glm::vec2 texCoord(static_cast<float>(tilesetX), static_cast<float>(tilesetY));
            glm::vec2 texSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));
            glm::vec2 renderSize(static_cast<float>(m_TileWidth) * scale, static_cast<float>(m_TileHeight) * scale);
            float tileRotation = GetTileRotation6(x, y);
            bool flipY = renderer.RequiresYFlip();
            renderer.DrawSpriteRegion(m_TilesetTexture, tilePos,
                                      renderSize, texCoord, texSize, tileRotation, glm::vec3(1.0f), flipY);
        }
    }
}

TileLayer &Tilemap::GetLayer(size_t index)
{
    if (index >= m_Layers.size())
    {
        throw std::out_of_range("Layer index out of range");
    }
    return m_Layers[index];
}

const TileLayer &Tilemap::GetLayer(size_t index) const
{
    if (index >= m_Layers.size())
    {
        throw std::out_of_range("Layer index out of range");
    }
    return m_Layers[index];
}

size_t Tilemap::AddLayer(const std::string &name, int renderOrder, bool isBackground)
{
    TileLayer layer(name, renderOrder, isBackground);
    layer.resize(static_cast<size_t>(m_MapWidth * m_MapHeight));
    m_Layers.push_back(std::move(layer));
    return m_Layers.size() - 1;
}

void Tilemap::InsertLayer(size_t index, const std::string &name, int renderOrder, bool isBackground)
{
    TileLayer layer(name, renderOrder, isBackground);
    layer.resize(static_cast<size_t>(m_MapWidth * m_MapHeight));
    if (index >= m_Layers.size())
    {
        m_Layers.push_back(std::move(layer));
    }
    else
    {
        m_Layers.insert(m_Layers.begin() + static_cast<ptrdiff_t>(index), std::move(layer));
    }
}

void Tilemap::RemoveLayer(size_t index)
{
    if (index < m_Layers.size())
    {
        m_Layers.erase(m_Layers.begin() + static_cast<ptrdiff_t>(index));
    }
}

int Tilemap::GetLayerTile(int x, int y, size_t layer) const
{
    if (layer >= m_Layers.size() || x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return -1;
    return m_Layers[layer].tiles[static_cast<size_t>(y * m_MapWidth + x)];
}

void Tilemap::SetLayerTile(int x, int y, size_t layer, int tileID)
{
    if (layer >= m_Layers.size() || x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return;
    m_Layers[layer].tiles[static_cast<size_t>(y * m_MapWidth + x)] = tileID;
}

float Tilemap::GetLayerRotation(int x, int y, size_t layer) const
{
    if (layer >= m_Layers.size() || x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return 0.0f;
    return m_Layers[layer].rotation[static_cast<size_t>(y * m_MapWidth + x)];
}

void Tilemap::SetLayerRotation(int x, int y, size_t layer, float rotation)
{
    if (layer >= m_Layers.size() || x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return;
    m_Layers[layer].rotation[static_cast<size_t>(y * m_MapWidth + x)] = rotation;
}

bool Tilemap::GetLayerNoProjection(int x, int y, size_t layer) const
{
    if (layer >= m_Layers.size() || x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return false;
    return m_Layers[layer].noProjection[static_cast<size_t>(y * m_MapWidth + x)];
}

void Tilemap::SetLayerNoProjection(int x, int y, size_t layer, bool noProjection)
{
    if (layer >= m_Layers.size() || x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return;
    m_Layers[layer].noProjection[static_cast<size_t>(y * m_MapWidth + x)] = noProjection;
}

bool Tilemap::GetLayerYSorted(int x, int y, size_t layer) const
{
    if (layer >= m_Layers.size() || x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return false;
    return m_Layers[layer].ySorted[static_cast<size_t>(y * m_MapWidth + x)];
}

void Tilemap::SetLayerYSorted(int x, int y, size_t layer, bool ySorted)
{
    if (layer >= m_Layers.size() || x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
        return;
    m_Layers[layer].ySorted[static_cast<size_t>(y * m_MapWidth + x)] = ySorted;
}

std::vector<size_t> Tilemap::GetLayerRenderOrder() const
{
    std::vector<size_t> indices(m_Layers.size());
    for (size_t i = 0; i < m_Layers.size(); ++i)
    {
        indices[i] = i;
    }
    std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b)
              { return m_Layers[a].renderOrder < m_Layers[b].renderOrder; });
    return indices;
}

void Tilemap::RenderLayerByIndex(IRenderer &renderer, size_t layerIndex,
                                 glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                                 glm::vec2 cullCam, glm::vec2 cullSize)
{
    if (layerIndex >= m_Layers.size())
        return;
    const TileLayer &layer = m_Layers[layerIndex];

    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            size_t idx = static_cast<size_t>(y * m_MapWidth + x);

            // Skip if no-projection or Y-sorted (rendered separately)
            if (layer.noProjection[idx] || layer.ySorted[idx])
                continue;

            int tileID = layer.tiles[idx];
            if (tileID < 0)
                continue;

            // Check for animated tile (per-layer animation map)
            if (idx < layer.animationMap.size())
            {
                int animId = layer.animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                }
            }

            if (IsTileTransparent(tileID))
                continue;

            int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
            int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;

            glm::vec2 pos(static_cast<float>(x * m_TileWidth) - renderCam.x,
                          static_cast<float>(y * m_TileHeight) - renderCam.y);
            glm::vec2 texCoord(static_cast<float>(tilesetX), static_cast<float>(tilesetY));
            glm::vec2 texSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));

            // Add tiny overlap when perspective is enabled to prevent seams
            float seamFix = renderer.GetPerspectiveState().enabled ? 0.1f : 0.0f;
            glm::vec2 tileRenderSize(static_cast<float>(m_TileWidth) + seamFix,
                                     static_cast<float>(m_TileHeight) + seamFix);

            float rotation = layer.rotation[idx];
            bool flipY = renderer.RequiresYFlip();
            renderer.DrawSpriteRegion(m_TilesetTexture, pos, tileRenderSize,
                                      texCoord, texSize, rotation, glm::vec3(1.0f), flipY);
        }
    }
}

void Tilemap::RenderLayerNoProjection(IRenderer &renderer, size_t layerIndex,
                                      glm::vec2 renderCam, glm::vec2 /*renderSize*/,
                                      glm::vec2 cullCam, glm::vec2 cullSize)
{
    if (layerIndex >= m_Layers.size())
        return;
    const TileLayer &layer = m_Layers[layerIndex];

    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    bool flipY = renderer.RequiresYFlip();

    // Check if perspective is enabled - if not, use simple direct rendering
    bool perspectiveEnabled = renderer.GetPerspectiveState().enabled;

    if (!perspectiveEnabled)
    {
        // 2D mode: render no-projection tiles exactly like normal tiles
        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                size_t idx = static_cast<size_t>(y * m_MapWidth + x);

                if (!layer.noProjection[idx])
                    continue;

                // Skip Y-sorted tiles - they're rendered in the Y-sorted pass
                if (layer.ySorted[idx])
                    continue;

                int tileID = layer.tiles[idx];
                if (tileID < 0)
                    continue;

                // Check for animated tile (per-layer animation map)
                if (idx < layer.animationMap.size())
                {
                    int animId = layer.animationMap[idx];
                    if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                    {
                        tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                    }
                }

                if (IsTileTransparent(tileID))
                    continue;

                int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
                int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;

                // Direct world-to-screen position (same as normal tile rendering)
                glm::vec2 pos(static_cast<float>(x * m_TileWidth) - renderCam.x,
                              static_cast<float>(y * m_TileHeight) - renderCam.y);
                glm::vec2 texCoord(static_cast<float>(tilesetX), static_cast<float>(tilesetY));
                glm::vec2 texSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));
                glm::vec2 tileRenderSize(static_cast<float>(m_TileWidth), static_cast<float>(m_TileHeight));

                float rotation = layer.rotation[idx];
                renderer.DrawSpriteRegion(m_TilesetTexture, pos, tileRenderSize,
                                          texCoord, texSize, rotation, glm::vec3(1.0f), flipY);
            }
        }
        return;
    }

    // 3D mode: use structure-based rendering
    // Project ONE anchor per structure, render tiles at integer offsets (no gaps)

    // Track which tiles have been processed
    std::vector<bool> processed(static_cast<size_t>(m_MapWidth * m_MapHeight), false);

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            size_t idx = static_cast<size_t>(y * m_MapWidth + x);

            if (!layer.noProjection[idx] || layer.ySorted[idx] || processed[idx])
                continue;

            int tileID = layer.tiles[idx];
            if (tileID < 0)
            {
                processed[idx] = true;
                continue;
            }

            // Check for animated tile (per-layer animation map)
            if (idx < layer.animationMap.size())
            {
                int animId = layer.animationMap[idx];
                if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                {
                    tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                }
            }

            if (IsTileTransparent(tileID))
            {
                processed[idx] = true;
                continue;
            }

            // Flood-fill to find connected structure
            std::vector<std::pair<int, int>> structure;
            std::vector<std::pair<int, int>> stack;
            stack.push_back({x, y});

            int minX = x, maxX = x, minY = y, maxY = y;

            while (!stack.empty())
            {
                auto [cx, cy] = stack.back();
                stack.pop_back();

                if (cx < 0 || cx >= m_MapWidth || cy < 0 || cy >= m_MapHeight)
                    continue;

                size_t cIdx = static_cast<size_t>(cy * m_MapWidth + cx);
                if (processed[cIdx])
                    continue;

                // Check if this tile is no-projection in ANY layer
                bool isNoProj = false;
                for (size_t li = 0; li < m_Layers.size(); ++li)
                {
                    if (cIdx < m_Layers[li].noProjection.size() && m_Layers[li].noProjection[cIdx])
                    {
                        isNoProj = true;
                        break;
                    }
                }
                if (!isNoProj)
                    continue;

                processed[cIdx] = true;
                structure.push_back({cx, cy});

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

            // Project bottom-left anchor for Y reference
            int leftPixelX = minX * m_TileWidth;
            int bottomPixelY = (maxY + 1) * m_TileHeight;
            float tileWf = static_cast<float>(m_TileWidth);
            float tileHf = static_cast<float>(m_TileHeight);

            // Get perspective state for globe curvature parameters
            auto perspState = renderer.GetPerspectiveState();
            float centerX = perspState.viewWidth * 0.5f;
            bool applyGlobe = (perspState.mode == IRenderer::ProjectionMode::Globe ||
                               perspState.mode == IRenderer::ProjectionMode::Fisheye);
            float R = perspState.sphereRadius;

            // Calculate vanishing point scale from bottom Y position
            float bottomScreenY = static_cast<float>(bottomPixelY) - renderCam.y;
            float t = (bottomScreenY - perspState.horizonY) / (perspState.viewHeight - perspState.horizonY);
            t = std::max(0.0f, std::min(1.0f, t));
            float vanishScale = perspState.horizonScale + (1.0f - perspState.horizonScale) * t;

            // Project the anchor for Y reference
            glm::vec2 projectedLeft = renderer.ProjectPoint(
                glm::vec2(static_cast<float>(leftPixelX) - renderCam.x,
                          static_cast<float>(bottomPixelY) - renderCam.y));

            // Apply exponential Y offset to fix lifted structure issue (vanishing point)
            float distanceFactor = 1.0f - vanishScale;
            float exponent = 2.0f;
            float multiplier = tileHf * 4.0f;
            float exponentialYOffset = std::pow(distanceFactor, exponent) * multiplier;
            projectedLeft.y += exponentialYOffset;

            // Compensate for globe Y curvature displacement
            if (applyGlobe && R > 0.0f)
            {
                float centerY = perspState.viewHeight * 0.5f;
                float dy = bottomScreenY - centerY;
                float globeY = centerY + R * std::sin(dy / R);
                float globeDisplacement = globeY - bottomScreenY;
                projectedLeft.y += globeDisplacement;
            }

            // Suspend perspective for drawing
            renderer.SuspendPerspective(true);

            // Render all tiles scaled to fit between the two anchors
            for (const auto &[tx, ty] : structure)
            {
                size_t tIdx = static_cast<size_t>(ty * m_MapWidth + tx);

                // Only render tiles from this layer
                if (!layer.noProjection[tIdx] || layer.ySorted[tIdx])
                    continue;

                int tid = layer.tiles[tIdx];
                if (tid < 0)
                    continue;

                // Check for animated tile (per-layer animation map)
                if (tIdx < layer.animationMap.size())
                {
                    int animId = layer.animationMap[tIdx];
                    if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                    {
                        tid = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                    }
                }

                if (IsTileTransparent(tid))
                    continue;

                // Calculate tile's screen X position (left edge of tile)
                float tileScreenX = static_cast<float>(tx * m_TileWidth) - renderCam.x;

                // Apply globe X curvature if enabled
                float curvedX = tileScreenX;
                if (applyGlobe && R > 0.0f)
                {
                    float dx = tileScreenX - centerX;
                    curvedX = centerX + R * std::sin(dx / R);
                }

                // Apply vanishing point X scaling
                float finalX = centerX + (curvedX - centerX) * vanishScale;

                // Y offset from anchor, scaled by vanishScale to match width scaling
                float relativeY = static_cast<float>((ty * m_TileHeight) - bottomPixelY) * vanishScale;
                float finalY = projectedLeft.y + relativeY;

                glm::vec2 pos(finalX, finalY);

                // Calculate scaled tile width for this X position
                float tileRightX = tileScreenX + tileWf;
                float curvedRightX = tileRightX;
                if (applyGlobe && R > 0.0f)
                {
                    float dxRight = tileRightX - centerX;
                    curvedRightX = centerX + R * std::sin(dxRight / R);
                }
                float finalRightX = centerX + (curvedRightX - centerX) * vanishScale;
                float tileDrawWidth = finalRightX - finalX;

                int tsX = (tid % dataTilesPerRow) * m_TileWidth;
                int tsY = (tid / dataTilesPerRow) * m_TileHeight;

                glm::vec2 texCoord(static_cast<float>(tsX), static_cast<float>(tsY));
                glm::vec2 texSize(tileWf, tileHf);

                float rotation = layer.rotation[tIdx];
                renderer.DrawSpriteRegion(m_TilesetTexture, pos, glm::vec2(tileDrawWidth, tileHf * vanishScale),
                                          texCoord, texSize, rotation, glm::vec3(1.0f), flipY);
            }

            renderer.SuspendPerspective(false);
        }
    }
}

void Tilemap::RenderBackgroundLayers(IRenderer &renderer, glm::vec2 renderCam, glm::vec2 renderSize,
                                     glm::vec2 cullCam, glm::vec2 cullSize)
{
    // Single-pass rendering: iterate visible tiles once, render all background layers per tile
    auto order = GetLayerRenderOrder();

    // Collect background layer indices in render order
    std::vector<size_t> bgLayers;
    bgLayers.reserve(m_Layers.size());
    for (size_t idx : order)
    {
        if (m_Layers[idx].isBackground)
        {
            bgLayers.push_back(idx);
        }
    }
    if (bgLayers.empty())
        return;

    // Compute visible tile range once
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    // Pre-compute constants
    const int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    const int mapWidth = m_MapWidth;
    const float tileWf = static_cast<float>(m_TileWidth);
    const float tileHf = static_cast<float>(m_TileHeight);
    const glm::vec2 texSize(tileWf, tileHf);
    const float seamFix = renderer.GetPerspectiveState().enabled ? 0.1f : 0.0f;
    const glm::vec2 tileRenderSize(tileWf + seamFix, tileHf + seamFix);
    const bool flipY = renderer.RequiresYFlip();
    const glm::vec3 white(1.0f);
    const bool hasTransparencyCache = m_TransparencyCacheBuilt;
    const std::vector<bool> &transparencyCache = m_TileTransparencyCache;
    const int transparencyCacheSize = static_cast<int>(transparencyCache.size());

    // Single pass over visible tiles
    for (int y = y0; y <= y1; ++y)
    {
        const int rowOffset = y * mapWidth;
        const float tilePosY = y * tileHf - renderCam.y;

        for (int x = x0; x <= x1; ++x)
        {
            const size_t idx = static_cast<size_t>(rowOffset + x);
            const float tilePosX = x * tileWf - renderCam.x;

            // Render all background layers at this position (in render order)
            for (size_t layerIdx : bgLayers)
            {
                const TileLayer &layer = m_Layers[layerIdx];

                // Skip if no-projection or Y-sorted (rendered separately)
                if (layer.noProjection[idx] || layer.ySorted[idx])
                    continue;

                int tileID = layer.tiles[idx];
                if (tileID < 0)
                    continue;

                // Check for animated tile
                if (idx < layer.animationMap.size())
                {
                    int animId = layer.animationMap[idx];
                    if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                    {
                        tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                    }
                }

                // Skip transparent tiles
                if (hasTransparencyCache && tileID < transparencyCacheSize && transparencyCache[tileID])
                    continue;

                const int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
                const int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;

                renderer.DrawSpriteRegion(m_TilesetTexture,
                                          glm::vec2(tilePosX, tilePosY),
                                          tileRenderSize,
                                          glm::vec2(static_cast<float>(tilesetX), static_cast<float>(tilesetY)),
                                          texSize,
                                          layer.rotation[idx],
                                          white, flipY);
            }
        }
    }
}

void Tilemap::RenderForegroundLayers(IRenderer &renderer, glm::vec2 renderCam, glm::vec2 renderSize,
                                     glm::vec2 cullCam, glm::vec2 cullSize)
{
    // Single-pass rendering: iterate visible tiles once, render all foreground layers per tile
    auto order = GetLayerRenderOrder();

    // Collect foreground layer indices in render order
    std::vector<size_t> fgLayers;
    fgLayers.reserve(m_Layers.size());
    for (size_t idx : order)
    {
        if (!m_Layers[idx].isBackground)
        {
            fgLayers.push_back(idx);
        }
    }
    if (fgLayers.empty())
        return;

    // Compute visible tile range once
    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    // Pre-compute constants
    const int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    const int mapWidth = m_MapWidth;
    const float tileWf = static_cast<float>(m_TileWidth);
    const float tileHf = static_cast<float>(m_TileHeight);
    const glm::vec2 texSize(tileWf, tileHf);
    const float seamFix = renderer.GetPerspectiveState().enabled ? 0.1f : 0.0f;
    const glm::vec2 tileRenderSize(tileWf + seamFix, tileHf + seamFix);
    const bool flipY = renderer.RequiresYFlip();
    const glm::vec3 white(1.0f);
    const bool hasTransparencyCache = m_TransparencyCacheBuilt;
    const std::vector<bool> &transparencyCache = m_TileTransparencyCache;
    const int transparencyCacheSize = static_cast<int>(transparencyCache.size());

    // Single pass over visible tiles
    for (int y = y0; y <= y1; ++y)
    {
        const int rowOffset = y * mapWidth;
        const float tilePosY = y * tileHf - renderCam.y;

        for (int x = x0; x <= x1; ++x)
        {
            const size_t idx = static_cast<size_t>(rowOffset + x);
            const float tilePosX = x * tileWf - renderCam.x;

            // Render all foreground layers at this position (in render order)
            for (size_t layerIdx : fgLayers)
            {
                const TileLayer &layer = m_Layers[layerIdx];

                // Skip if no-projection or Y-sorted (rendered separately)
                if (layer.noProjection[idx] || layer.ySorted[idx])
                    continue;

                int tileID = layer.tiles[idx];
                if (tileID < 0)
                    continue;

                // Check for animated tile
                if (idx < layer.animationMap.size())
                {
                    int animId = layer.animationMap[idx];
                    if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                    {
                        tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                    }
                }

                // Skip transparent tiles
                if (hasTransparencyCache && tileID < transparencyCacheSize && transparencyCache[tileID])
                    continue;

                const int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
                const int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;

                renderer.DrawSpriteRegion(m_TilesetTexture,
                                          glm::vec2(tilePosX, tilePosY),
                                          tileRenderSize,
                                          glm::vec2(static_cast<float>(tilesetX), static_cast<float>(tilesetY)),
                                          texSize,
                                          layer.rotation[idx],
                                          white, flipY);
            }
        }
    }
}

void Tilemap::RenderBackgroundLayersNoProjection(IRenderer &renderer, glm::vec2 renderCam, glm::vec2 renderSize,
                                                 glm::vec2 cullCam, glm::vec2 cullSize)
{
    // Single-pass NoProjection rendering for all background layers
    auto order = GetLayerRenderOrder();

    // Collect background layer indices in render order
    std::vector<size_t> bgLayers;
    bgLayers.reserve(m_Layers.size());
    for (size_t idx : order)
    {
        if (m_Layers[idx].isBackground)
        {
            bgLayers.push_back(idx);
        }
    }
    if (bgLayers.empty())
        return;

    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    const int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    const int mapWidth = m_MapWidth;
    const float tileWf = static_cast<float>(m_TileWidth);
    const float tileHf = static_cast<float>(m_TileHeight);
    const bool flipY = renderer.RequiresYFlip();
    const glm::vec3 white(1.0f);
    const bool perspectiveEnabled = renderer.GetPerspectiveState().enabled;

    if (!perspectiveEnabled)
    {
        // 2D mode: single-pass all background layers
        for (int y = y0; y <= y1; ++y)
        {
            const int rowOffset = y * mapWidth;
            const float tilePosY = y * tileHf - renderCam.y;

            for (int x = x0; x <= x1; ++x)
            {
                const size_t idx = static_cast<size_t>(rowOffset + x);
                const float tilePosX = x * tileWf - renderCam.x;

                for (size_t layerIdx : bgLayers)
                {
                    const TileLayer &layer = m_Layers[layerIdx];

                    if (!layer.noProjection[idx] || layer.ySorted[idx])
                        continue;

                    int tileID = layer.tiles[idx];
                    if (tileID < 0)
                        continue;

                    if (idx < layer.animationMap.size())
                    {
                        int animId = layer.animationMap[idx];
                        if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                        {
                            tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                        }
                    }

                    if (IsTileTransparent(tileID))
                        continue;

                    int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
                    int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;

                    renderer.DrawSpriteRegion(m_TilesetTexture,
                                              glm::vec2(tilePosX, tilePosY),
                                              glm::vec2(tileWf, tileHf),
                                              glm::vec2(static_cast<float>(tilesetX), static_cast<float>(tilesetY)),
                                              glm::vec2(tileWf, tileHf),
                                              layer.rotation[idx], white, flipY);
                }
            }
        }
        return;
    }

    // 3D mode: structure-based rendering with shared processed array
    std::vector<bool> processed(static_cast<size_t>(m_MapWidth * m_MapHeight), false);

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            size_t idx = static_cast<size_t>(y * m_MapWidth + x);

            if (processed[idx])
                continue;

            // Check if ANY background layer has noProjection at this position
            bool hasNoProj = false;
            for (size_t layerIdx : bgLayers)
            {
                if (m_Layers[layerIdx].noProjection[idx] && !m_Layers[layerIdx].ySorted[idx])
                {
                    hasNoProj = true;
                    break;
                }
            }
            if (!hasNoProj)
                continue;

            // Flood-fill to find connected structure (across ALL layers)
            std::vector<std::pair<int, int>> structure;
            std::vector<std::pair<int, int>> stack;
            stack.push_back({x, y});

            int minX = x, maxX = x, minY = y, maxY = y;

            while (!stack.empty())
            {
                auto [cx, cy] = stack.back();
                stack.pop_back();

                if (cx < 0 || cx >= m_MapWidth || cy < 0 || cy >= m_MapHeight)
                    continue;

                size_t cIdx = static_cast<size_t>(cy * m_MapWidth + cx);
                if (processed[cIdx])
                    continue;

                // Check if this tile is no-projection in ANY layer
                bool isNoProj = false;
                for (size_t li = 0; li < m_Layers.size(); ++li)
                {
                    if (cIdx < m_Layers[li].noProjection.size() && m_Layers[li].noProjection[cIdx])
                    {
                        isNoProj = true;
                        break;
                    }
                }
                if (!isNoProj)
                    continue;

                processed[cIdx] = true;
                structure.push_back({cx, cy});

                minX = std::min(minX, cx);
                maxX = std::max(maxX, cx);
                minY = std::min(minY, cy);
                maxY = std::max(maxY, cy);

                stack.push_back({cx - 1, cy});
                stack.push_back({cx + 1, cy});
                stack.push_back({cx, cy - 1});
                stack.push_back({cx, cy + 1});
            }

            // Project bottom-left anchor for Y reference
            int leftPixelX = minX * m_TileWidth;
            int bottomPixelY = (maxY + 1) * m_TileHeight;

            // Get perspective state for globe curvature parameters
            auto perspState = renderer.GetPerspectiveState();
            float centerX = perspState.viewWidth * 0.5f;
            bool applyGlobe = (perspState.mode == IRenderer::ProjectionMode::Globe ||
                               perspState.mode == IRenderer::ProjectionMode::Fisheye);
            float R = perspState.sphereRadius;

            // Calculate vanishing point scale from bottom Y position
            float bottomScreenY = static_cast<float>(bottomPixelY) - renderCam.y;
            float t = (bottomScreenY - perspState.horizonY) / (perspState.viewHeight - perspState.horizonY);
            t = std::max(0.0f, std::min(1.0f, t));
            float vanishScale = perspState.horizonScale + (1.0f - perspState.horizonScale) * t;

            // Project the anchor for Y reference
            glm::vec2 projectedLeft = renderer.ProjectPoint(
                glm::vec2(static_cast<float>(leftPixelX) - renderCam.x,
                          static_cast<float>(bottomPixelY) - renderCam.y));

            // Apply exponential Y offset to fix lifted structure issue (vanishing point)
            float distanceFactor = 1.0f - vanishScale;
            float exponent = 2.0f;
            float multiplier = tileHf * 4.0f;
            float exponentialYOffset = std::pow(distanceFactor, exponent) * multiplier;
            projectedLeft.y += exponentialYOffset;

            // Compensate for globe Y curvature displacement
            if (applyGlobe && R > 0.0f)
            {
                float centerY = perspState.viewHeight * 0.5f;
                float dy = bottomScreenY - centerY;
                float globeY = centerY + R * std::sin(dy / R);
                float globeDisplacement = globeY - bottomScreenY;
                projectedLeft.y += globeDisplacement;
            }

            renderer.SuspendPerspective(true);

            // Render all background layers for this structure (in layer order)
            for (const auto &[tx, ty] : structure)
            {
                size_t tIdx = static_cast<size_t>(ty * m_MapWidth + tx);

                for (size_t layerIdx : bgLayers)
                {
                    const TileLayer &layer = m_Layers[layerIdx];

                    if (!layer.noProjection[tIdx] || layer.ySorted[tIdx])
                        continue;

                    int tid = layer.tiles[tIdx];
                    if (tid < 0)
                        continue;

                    if (tIdx < layer.animationMap.size())
                    {
                        int animId = layer.animationMap[tIdx];
                        if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                        {
                            tid = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                        }
                    }

                    if (IsTileTransparent(tid))
                        continue;

                    // Calculate tile's screen X position (left edge of tile)
                    float tileScreenX = static_cast<float>(tx * m_TileWidth) - renderCam.x;

                    // Apply globe X curvature if enabled
                    float curvedX = tileScreenX;
                    if (applyGlobe && R > 0.0f)
                    {
                        float dx = tileScreenX - centerX;
                        curvedX = centerX + R * std::sin(dx / R);
                    }

                    // Apply vanishing point X scaling
                    float finalX = centerX + (curvedX - centerX) * vanishScale;

                    // Y offset from anchor, scaled by vanishScale to match width scaling
                    float relativeY = static_cast<float>((ty * m_TileHeight) - bottomPixelY) * vanishScale;
                    float finalY = projectedLeft.y + relativeY;

                    glm::vec2 pos(finalX, finalY);

                    // Calculate scaled tile width for this X position
                    float tileRightX = tileScreenX + tileWf;
                    float curvedRightX = tileRightX;
                    if (applyGlobe && R > 0.0f)
                    {
                        float dxRight = tileRightX - centerX;
                        curvedRightX = centerX + R * std::sin(dxRight / R);
                    }
                    float finalRightX = centerX + (curvedRightX - centerX) * vanishScale;
                    float tileDrawWidth = finalRightX - finalX;

                    int tsX = (tid % dataTilesPerRow) * m_TileWidth;
                    int tsY = (tid / dataTilesPerRow) * m_TileHeight;

                    renderer.DrawSpriteRegion(m_TilesetTexture, pos,
                                              glm::vec2(tileDrawWidth, tileHf * vanishScale),
                                              glm::vec2(static_cast<float>(tsX), static_cast<float>(tsY)),
                                              glm::vec2(tileWf, tileHf),
                                              layer.rotation[tIdx], white, flipY);
                }
            }

            renderer.SuspendPerspective(false);
        }
    }
}

void Tilemap::RenderForegroundLayersNoProjection(IRenderer &renderer, glm::vec2 renderCam, glm::vec2 renderSize,
                                                 glm::vec2 cullCam, glm::vec2 cullSize)
{
    // Single-pass NoProjection rendering for all foreground layers
    auto order = GetLayerRenderOrder();

    // Collect foreground layer indices in render order
    std::vector<size_t> fgLayers;
    fgLayers.reserve(m_Layers.size());
    for (size_t idx : order)
    {
        if (!m_Layers[idx].isBackground)
        {
            fgLayers.push_back(idx);
        }
    }
    if (fgLayers.empty())
        return;

    int x0, y0, x1, y1;
    ComputeTileRange(m_MapWidth, m_MapHeight, m_TileWidth, m_TileHeight, cullCam, cullSize, x0, y0, x1, y1);

    const int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
    const int mapWidth = m_MapWidth;
    const float tileWf = static_cast<float>(m_TileWidth);
    const float tileHf = static_cast<float>(m_TileHeight);
    const bool flipY = renderer.RequiresYFlip();
    const glm::vec3 white(1.0f);
    const bool perspectiveEnabled = renderer.GetPerspectiveState().enabled;

    if (!perspectiveEnabled)
    {
        // 2D mode: single-pass all foreground layers
        for (int y = y0; y <= y1; ++y)
        {
            const int rowOffset = y * mapWidth;
            const float tilePosY = y * tileHf - renderCam.y;

            for (int x = x0; x <= x1; ++x)
            {
                const size_t idx = static_cast<size_t>(rowOffset + x);
                const float tilePosX = x * tileWf - renderCam.x;

                for (size_t layerIdx : fgLayers)
                {
                    const TileLayer &layer = m_Layers[layerIdx];

                    if (!layer.noProjection[idx] || layer.ySorted[idx])
                        continue;

                    int tileID = layer.tiles[idx];
                    if (tileID < 0)
                        continue;

                    if (idx < layer.animationMap.size())
                    {
                        int animId = layer.animationMap[idx];
                        if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                        {
                            tileID = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                        }
                    }

                    if (IsTileTransparent(tileID))
                        continue;

                    int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
                    int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;

                    renderer.DrawSpriteRegion(m_TilesetTexture,
                                              glm::vec2(tilePosX, tilePosY),
                                              glm::vec2(tileWf, tileHf),
                                              glm::vec2(static_cast<float>(tilesetX), static_cast<float>(tilesetY)),
                                              glm::vec2(tileWf, tileHf),
                                              layer.rotation[idx], white, flipY);
                }
            }
        }
        return;
    }

    // 3D mode: structure-based rendering with shared processed array
    std::vector<bool> processed(static_cast<size_t>(m_MapWidth * m_MapHeight), false);

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            size_t idx = static_cast<size_t>(y * m_MapWidth + x);

            if (processed[idx])
                continue;

            // Check if ANY foreground layer has noProjection at this position
            bool hasNoProj = false;
            for (size_t layerIdx : fgLayers)
            {
                if (m_Layers[layerIdx].noProjection[idx] && !m_Layers[layerIdx].ySorted[idx])
                {
                    hasNoProj = true;
                    break;
                }
            }
            if (!hasNoProj)
                continue;

            // Flood-fill to find connected structure (across ALL layers)
            std::vector<std::pair<int, int>> structure;
            std::vector<std::pair<int, int>> stack;
            stack.push_back({x, y});

            int minX = x, maxX = x, minY = y, maxY = y;

            while (!stack.empty())
            {
                auto [cx, cy] = stack.back();
                stack.pop_back();

                if (cx < 0 || cx >= m_MapWidth || cy < 0 || cy >= m_MapHeight)
                    continue;

                size_t cIdx = static_cast<size_t>(cy * m_MapWidth + cx);
                if (processed[cIdx])
                    continue;

                // Check if this tile is no-projection in ANY layer
                bool isNoProj = false;
                for (size_t li = 0; li < m_Layers.size(); ++li)
                {
                    if (cIdx < m_Layers[li].noProjection.size() && m_Layers[li].noProjection[cIdx])
                    {
                        isNoProj = true;
                        break;
                    }
                }
                if (!isNoProj)
                    continue;

                processed[cIdx] = true;
                structure.push_back({cx, cy});

                minX = std::min(minX, cx);
                maxX = std::max(maxX, cx);
                minY = std::min(minY, cy);
                maxY = std::max(maxY, cy);

                stack.push_back({cx - 1, cy});
                stack.push_back({cx + 1, cy});
                stack.push_back({cx, cy - 1});
                stack.push_back({cx, cy + 1});
            }

            // Project bottom-left anchor for Y reference
            int leftPixelX = minX * m_TileWidth;
            int bottomPixelY = (maxY + 1) * m_TileHeight;

            // Get perspective state for globe curvature parameters
            auto perspState = renderer.GetPerspectiveState();
            float centerX = perspState.viewWidth * 0.5f;
            bool applyGlobe = (perspState.mode == IRenderer::ProjectionMode::Globe ||
                               perspState.mode == IRenderer::ProjectionMode::Fisheye);
            float R = perspState.sphereRadius;

            // Calculate vanishing point scale from bottom Y position
            float bottomScreenY = static_cast<float>(bottomPixelY) - renderCam.y;
            float t = (bottomScreenY - perspState.horizonY) / (perspState.viewHeight - perspState.horizonY);
            t = std::max(0.0f, std::min(1.0f, t));
            float vanishScale = perspState.horizonScale + (1.0f - perspState.horizonScale) * t;

            // Project the anchor for Y reference
            glm::vec2 projectedLeft = renderer.ProjectPoint(
                glm::vec2(static_cast<float>(leftPixelX) - renderCam.x,
                          static_cast<float>(bottomPixelY) - renderCam.y));

            // Apply exponential Y offset to fix lifted structure issue (vanishing point)
            float distanceFactor = 1.0f - vanishScale;
            float exponent = 2.0f;
            float multiplier = tileHf * 4.0f;
            float exponentialYOffset = std::pow(distanceFactor, exponent) * multiplier;
            projectedLeft.y += exponentialYOffset;

            // Compensate for globe Y curvature displacement
            if (applyGlobe && R > 0.0f)
            {
                float centerY = perspState.viewHeight * 0.5f;
                float dy = bottomScreenY - centerY;
                float globeY = centerY + R * std::sin(dy / R);
                float globeDisplacement = globeY - bottomScreenY;
                projectedLeft.y += globeDisplacement;
            }

            renderer.SuspendPerspective(true);

            // Render all foreground layers for this structure (in layer order)
            for (const auto &[tx, ty] : structure)
            {
                size_t tIdx = static_cast<size_t>(ty * m_MapWidth + tx);

                for (size_t layerIdx : fgLayers)
                {
                    const TileLayer &layer = m_Layers[layerIdx];

                    if (!layer.noProjection[tIdx] || layer.ySorted[tIdx])
                        continue;

                    int tid = layer.tiles[tIdx];
                    if (tid < 0)
                        continue;

                    if (tIdx < layer.animationMap.size())
                    {
                        int animId = layer.animationMap[tIdx];
                        if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()))
                        {
                            tid = m_AnimatedTiles[animId].GetFrameAtTime(m_AnimationTime);
                        }
                    }

                    if (IsTileTransparent(tid))
                        continue;

                    // Calculate tile's screen X position (left edge of tile)
                    float tileScreenX = static_cast<float>(tx * m_TileWidth) - renderCam.x;

                    // Apply globe X curvature if enabled
                    float curvedX = tileScreenX;
                    if (applyGlobe && R > 0.0f)
                    {
                        float dx = tileScreenX - centerX;
                        curvedX = centerX + R * std::sin(dx / R);
                    }

                    // Apply vanishing point X scaling
                    float finalX = centerX + (curvedX - centerX) * vanishScale;

                    // Y offset from anchor, scaled by vanishScale to match width scaling
                    float relativeY = static_cast<float>((ty * m_TileHeight) - bottomPixelY) * vanishScale;
                    float finalY = projectedLeft.y + relativeY;

                    glm::vec2 pos(finalX, finalY);

                    // Calculate scaled tile width for this X position
                    float tileRightX = tileScreenX + tileWf;
                    float curvedRightX = tileRightX;
                    if (applyGlobe && R > 0.0f)
                    {
                        float dxRight = tileRightX - centerX;
                        curvedRightX = centerX + R * std::sin(dxRight / R);
                    }
                    float finalRightX = centerX + (curvedRightX - centerX) * vanishScale;
                    float tileDrawWidth = finalRightX - finalX;

                    int tsX = (tid % dataTilesPerRow) * m_TileWidth;
                    int tsY = (tid / dataTilesPerRow) * m_TileHeight;

                    renderer.DrawSpriteRegion(m_TilesetTexture, pos,
                                              glm::vec2(tileDrawWidth, tileHf * vanishScale),
                                              glm::vec2(static_cast<float>(tsX), static_cast<float>(tsY)),
                                              glm::vec2(tileWf, tileHf),
                                              layer.rotation[tIdx], white, flipY);
                }
            }

            renderer.SuspendPerspective(false);
        }
    }
}

void Tilemap::GenerateDefaultMap()
{
    // Validate tileset is loaded
    if (!m_TilesetData || m_TilesetDataWidth == 0 || m_TilesetDataHeight == 0)
    {
        std::cerr << "ERROR: Cannot generate map - tileset data not loaded!" << std::endl;
        return;
    }

    // === Phase 1 & 2: Scan tileset for valid (non-transparent) tiles ===
    std::vector<int> validTileIDs;

    int totalTilesX = m_TilesetDataWidth / m_TileWidth;
    int totalTilesY = m_TilesetDataHeight / m_TileHeight;
    int totalTiles = totalTilesX * totalTilesY;

    std::cout << "Scanning tileset for non-transparent tiles..." << std::endl;
    std::cout << "  Tileset size: " << m_TilesetDataWidth << "x" << m_TilesetDataHeight << " pixels" << std::endl;
    std::cout << "  Tile size: " << m_TileWidth << "x" << m_TileHeight << " pixels" << std::endl;
    std::cout << "  Total tiles in tileset: " << totalTilesX << "x" << totalTilesY << " = " << totalTiles << " tiles" << std::endl;

    for (int tileID = 0; tileID < totalTiles; ++tileID)
    {
        // Verify tile alignment (should always be true for sequential IDs)
        int dataTilesPerRow = m_TilesetDataWidth / m_TileWidth;
        int tilesetX = (tileID % dataTilesPerRow) * m_TileWidth;
        int tilesetY = (tileID / dataTilesPerRow) * m_TileHeight;

        if (tilesetX % m_TileWidth != 0 || tilesetY % m_TileHeight != 0)
        {
            continue; // Skip misaligned tiles (shouldn't happen)
        }

        if (!IsTileTransparent(tileID))
        {
            validTileIDs.push_back(tileID);
        }
    }

    std::cout << "Found " << validTileIDs.size() << " non-transparent tiles out of " << totalTiles << " total tiles" << std::endl;

    if (validTileIDs.empty())
    {
        std::cerr << "ERROR: No valid non-transparent tiles found in tileset!" << std::endl;
        return;
    }

    // === Phase 3: Fill map with random valid tiles ===
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    std::cout << "Generating random map with " << (m_MapWidth * m_MapHeight) << " tiles..." << std::endl;

    for (int y = 0; y < m_MapHeight; ++y)
    {
        for (int x = 0; x < m_MapWidth; ++x)
        {
            // Uniform random selection from valid tiles
            int randomIndex = std::rand() % validTileIDs.size();
            int tileID = validTileIDs[randomIndex];
            SetTile(x, y, tileID);
        }
    }

    std::cout << "Generated random map with " << (m_MapWidth * m_MapHeight) << " tiles" << std::endl;
}

std::vector<int> Tilemap::GetValidTileIDs() const
{
    std::vector<int> validTileIDs;

    if (!m_TilesetData || m_TilesetDataWidth == 0 || m_TilesetDataHeight == 0)
    {
        return validTileIDs;
    }

    int totalTilesX = m_TilesetDataWidth / m_TileWidth;
    int totalTilesY = m_TilesetDataHeight / m_TileHeight;
    int totalTiles = totalTilesX * totalTilesY;

    for (int tileID = 0; tileID < totalTiles; ++tileID)
    {
        if (!IsTileTransparent(tileID))
        {
            validTileIDs.push_back(tileID);
        }
    }

    return validTileIDs;
}

static std::vector<DialogueCondition> ParseConditionString(const std::string &whenStr)
{
    std::vector<DialogueCondition> conditions;
    if (whenStr.empty())
        return conditions;

    // Split by " & " for AND conditions
    std::string remaining = whenStr;
    while (!remaining.empty())
    {
        size_t andPos = remaining.find(" & ");
        std::string part = (andPos != std::string::npos)
                               ? remaining.substr(0, andPos)
                               : remaining;
        remaining = (andPos != std::string::npos)
                        ? remaining.substr(andPos + 3)
                        : "";

        // Trim whitespace
        while (!part.empty() && part[0] == ' ')
            part.erase(0, 1);
        while (!part.empty() && part.back() == ' ')
            part.pop_back();
        if (part.empty())
            continue;

        DialogueCondition cond;

        // Check for negation
        bool negated = (part[0] == '!');
        if (negated)
            part.erase(0, 1);

        // Check for equals
        size_t eqPos = part.find('=');
        if (eqPos != std::string::npos)
        {
            cond.type = DialogueCondition::Type::FLAG_EQUALS;
            cond.key = part.substr(0, eqPos);
            cond.value = part.substr(eqPos + 1);
        }
        else
        {
            cond.type = negated ? DialogueCondition::Type::FLAG_NOT_SET
                                : DialogueCondition::Type::FLAG_SET;
            cond.key = part;
        }

        conditions.push_back(cond);
    }
    return conditions;
}

static std::vector<DialogueConsequence> ParseConsequenceArray(const nlohmann::json &doArr)
{
    std::vector<DialogueConsequence> consequences;
    if (!doArr.is_array())
        return consequences;

    for (const auto &item : doArr)
    {
        if (!item.is_string())
            continue;
        std::string str = item.get<std::string>();
        if (str.empty())
            continue;

        DialogueConsequence cons;

        // Check for clear flag prefix
        if (str[0] == '-')
        {
            cons.type = DialogueConsequence::Type::CLEAR_FLAG;
            cons.key = str.substr(1);
        }
        // Check for quest description (colon syntax for accepted_ flags)
        else if (str.find(':') != std::string::npos)
        {
            size_t colonPos = str.find(':');
            cons.type = DialogueConsequence::Type::SET_FLAG;
            cons.key = str.substr(0, colonPos);
            cons.value = str.substr(colonPos + 1); // Quest description
        }
        // Check for value assignment
        else if (str.find('=') != std::string::npos)
        {
            size_t eqPos = str.find('=');
            cons.type = DialogueConsequence::Type::SET_FLAG_VALUE;
            cons.key = str.substr(0, eqPos);
            cons.value = str.substr(eqPos + 1);
        }
        // Simple flag set
        else
        {
            cons.type = DialogueConsequence::Type::SET_FLAG;
            cons.key = str;
        }

        consequences.push_back(cons);
    }
    return consequences;
}

static std::string SerializeConditions(const std::vector<DialogueCondition> &conditions)
{
    if (conditions.empty())
        return "";

    std::string result;
    for (size_t i = 0; i < conditions.size(); ++i)
    {
        if (i > 0)
            result += " & ";
        const auto &c = conditions[i];

        if (c.type == DialogueCondition::Type::FLAG_NOT_SET)
            result += "!" + c.key;
        else if (c.type == DialogueCondition::Type::FLAG_EQUALS)
            result += c.key + "=" + c.value;
        else
            result += c.key;
    }
    return result;
}

static nlohmann::json SerializeConsequences(const std::vector<DialogueConsequence> &consequences)
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &c : consequences)
    {
        if (c.type == DialogueConsequence::Type::CLEAR_FLAG)
            arr.push_back("-" + c.key);
        else if (c.type == DialogueConsequence::Type::SET_FLAG_VALUE)
            arr.push_back(c.key + "=" + c.value);
        else if (c.type == DialogueConsequence::Type::SET_FLAG && !c.value.empty())
            arr.push_back(c.key + ":" + c.value); // Quest description
        else
            arr.push_back(c.key);
    }
    return arr;
}

bool Tilemap::SaveMapToJSON(const std::string &filename, const std::vector<NonPlayerCharacter> *npcs,
                            int playerTileX, int playerTileY, int characterType) const
{
    using json = nlohmann::json;

    json j;

    // Map dimensions
    j["width"] = m_MapWidth;
    j["height"] = m_MapHeight;
    j["tileWidth"] = m_TileWidth;
    j["tileHeight"] = m_TileHeight;

    // Collision (array of indices)
    j["collision"] = m_CollisionMap.GetCollisionIndices();

    // Navigation (array of indices)
    j["navigation"] = m_NavigationMap.GetNavigationIndices();

    // Elevation (sparse object)
    {
        json elevObj = json::object();
        for (int y = 0; y < m_MapHeight; ++y)
        {
            for (int x = 0; x < m_MapWidth; ++x)
            {
                int elev = GetElevation(x, y);
                if (elev != 0)
                {
                    int index = y * m_MapWidth + x;
                    elevObj[std::to_string(index)] = elev;
                }
            }
        }
        j["elevation"] = elevObj;
    }

    // Dynamic layers (all tile data stored here)
    json dynamicLayersArray = json::array();
    for (size_t layerIdx = 0; layerIdx < m_Layers.size(); ++layerIdx)
    {
        const TileLayer &layer = m_Layers[layerIdx];
        json layerJson = json::object();
        layerJson["name"] = layer.name;
        layerJson["renderOrder"] = layer.renderOrder;
        layerJson["isBackground"] = layer.isBackground;

        // Tiles (sparse)
        json tilesObj = json::object();
        for (size_t i = 0; i < layer.tiles.size(); ++i)
        {
            if (layer.tiles[i] != -1)
            {
                tilesObj[std::to_string(i)] = layer.tiles[i];
            }
        }
        layerJson["tiles"] = tilesObj;

        // Rotation (sparse)
        json rotObj = json::object();
        for (size_t i = 0; i < layer.rotation.size(); ++i)
        {
            if (layer.rotation[i] != 0.0f)
            {
                rotObj[std::to_string(i)] = layer.rotation[i];
            }
        }
        layerJson["rotation"] = rotObj;

        // NoProjection (array of indices)
        json noProjArr = json::array();
        for (size_t i = 0; i < layer.noProjection.size(); ++i)
        {
            if (layer.noProjection[i])
            {
                noProjArr.push_back(static_cast<int>(i));
            }
        }
        layerJson["noProjection"] = noProjArr;

        // YSorted (array of indices)
        json ySortedArr = json::array();
        for (size_t i = 0; i < layer.ySorted.size(); ++i)
        {
            if (layer.ySorted[i])
            {
                ySortedArr.push_back(static_cast<int>(i));
            }
        }
        layerJson["ySorted"] = ySortedArr;

        dynamicLayersArray.push_back(layerJson);
    }
    j["dynamicLayers"] = dynamicLayersArray;

    // Particle Zones
    json particleZonesArray = json::array();
    for (const auto &zone : m_ParticleZones)
    {
        json zoneJson;
        zoneJson["x"] = zone.position.x;
        zoneJson["y"] = zone.position.y;
        zoneJson["width"] = zone.size.x;
        zoneJson["height"] = zone.size.y;
        zoneJson["type"] = static_cast<int>(zone.type);
        zoneJson["enabled"] = zone.enabled;
        zoneJson["noProjection"] = zone.noProjection;
        particleZonesArray.push_back(zoneJson);
    }
    j["particleZones"] = particleZonesArray;

    // NPCs
    json npcsArray = json::array();
    if (npcs)
    {
        std::cout << "Saving " << npcs->size() << " NPCs to " << filename << std::endl;
        for (const auto &npc : *npcs)
        {
            json npcObj;
            npcObj["type"] = npc.GetType();
            npcObj["tileX"] = npc.GetTileX();
            npcObj["tileY"] = npc.GetTileY();
            if (!npc.GetName().empty())
            {
                npcObj["name"] = npc.GetName();
            }
            if (!npc.GetDialogue().empty())
            {
                npcObj["dialogue"] = npc.GetDialogue();
            }
            // Save dialogue tree (simplified format)
            if (npc.HasDialogueTree())
            {
                const DialogueTree &tree = npc.GetDialogueTree();
                json treeJson;
                if (tree.startNodeId != "start")
                    treeJson["start"] = tree.startNodeId;

                // Find default speaker (most common speaker in nodes)
                std::string defaultSpeaker = npc.GetName();
                if (!tree.nodes.empty())
                    defaultSpeaker = tree.nodes.begin()->second.speaker;
                if (!defaultSpeaker.empty())
                    treeJson["speaker"] = defaultSpeaker;

                json nodesObj = json::object();
                for (const auto &[nodeId, node] : tree.nodes)
                {
                    json nodeJson;
                    if (node.speaker != defaultSpeaker)
                        nodeJson["speaker"] = node.speaker;
                    nodeJson["text"] = node.text;

                    json choicesArr = json::array();
                    for (const auto &opt : node.options)
                    {
                        json choiceJson;
                        choiceJson["text"] = opt.text;
                        if (!opt.nextNodeId.empty())
                            choiceJson["goto"] = opt.nextNodeId;
                        std::string whenStr = SerializeConditions(opt.conditions);
                        if (!whenStr.empty())
                            choiceJson["when"] = whenStr;
                        if (!opt.consequences.empty())
                            choiceJson["do"] = SerializeConsequences(opt.consequences);
                        choicesArr.push_back(choiceJson);
                    }
                    nodeJson["choices"] = choicesArr;
                    nodesObj[nodeId] = nodeJson;
                }
                treeJson["nodes"] = nodesObj;
                npcObj["dialogueTree"] = treeJson;
            }
            npcsArray.push_back(npcObj);
            std::cout << "  Saved NPC: " << npc.GetType() << " at (" << npc.GetTileX() << ", " << npc.GetTileY() << ")" << std::endl;
        }
    }
    j["npcs"] = npcsArray;

    // Player position
    if (playerTileX >= 0 && playerTileY >= 0)
    {
        json playerObj;
        playerObj["tileX"] = playerTileX;
        playerObj["tileY"] = playerTileY;
        if (characterType >= 0)
        {
            playerObj["characterType"] = characterType;
        }
        j["player"] = playerObj;
    }
    else
    {
        j["player"] = nullptr;
    }

    // Animated Tiles - save animation definitions and placements
    json animatedTilesArray = json::array();
    for (const auto &anim : m_AnimatedTiles)
    {
        json animJson;
        animJson["frames"] = anim.frames;
        animJson["frameDuration"] = anim.frameDuration;
        animatedTilesArray.push_back(animJson);
    }
    j["animatedTiles"] = animatedTilesArray;

    // Animation Map - save per-layer animation maps (sparse format)
    json layerAnimMaps = json::array();
    for (size_t layerIdx = 0; layerIdx < m_Layers.size(); ++layerIdx)
    {
        json layerAnimObj = json::object();
        const auto &animMap = m_Layers[layerIdx].animationMap;
        for (size_t i = 0; i < animMap.size(); ++i)
        {
            if (animMap[i] >= 0)
            {
                layerAnimObj[std::to_string(i)] = animMap[i];
            }
        }
        layerAnimMaps.push_back(layerAnimObj);
    }
    j["layerAnimationMaps"] = layerAnimMaps;

    // Corner Cut Blocked - save as sparse array of indices with mask values
    {
        json cornerCutObj = json::object();
        for (size_t i = 0; i < m_CornerCutBlocked.size(); ++i)
        {
            if (m_CornerCutBlocked[i] != 0)
            {
                cornerCutObj[std::to_string(i)] = m_CornerCutBlocked[i];
            }
        }
        j["cornerCutBlocked"] = cornerCutObj;
    }

    // Write to file
    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "ERROR: Could not open file for writing: " << filename << std::endl;
        return false;
    }

    file << j.dump(2); // Pretty print with 2-space indent
    file.close();

    std::cout << "Map saved to " << filename << std::endl;
    return true;
}

bool Tilemap::LoadMapFromJSON(const std::string &filename, std::vector<NonPlayerCharacter> *npcs,
                              int *playerTileX, int *playerTileY, int *characterType)
{
    using json = nlohmann::json;

    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "ERROR: Could not open file for reading: " << filename << std::endl;
        return false;
    }

    json j;
    try
    {
        file >> j;
    }
    catch (const json::parse_error &e)
    {
        std::cerr << "ERROR: Failed to parse JSON: " << e.what() << std::endl;
        return false;
    }
    file.close();

    int width = j.value("width", 0);
    int height = j.value("height", 0);
    int tileWidth = j.value("tileWidth", 16);
    int tileHeight = j.value("tileHeight", 16);

    if (width <= 0 || height <= 0)
    {
        std::cerr << "ERROR: Invalid map dimensions in " << filename << std::endl;
        return false;
    }

    // Initialize tilemap
    m_TileWidth = tileWidth;
    m_TileHeight = tileHeight;
    SetTilemapSize(width, height, false);

    // Helper to load sparse tile layer {"index": value}
    auto loadTileLayer = [&](const std::string &name, std::function<void(int, int, int)> setTile)
    {
        if (!j.contains(name))
            return;
        const auto &layer = j[name];
        if (layer.is_object())
        {
            for (auto &[key, value] : layer.items())
            {
                try
                {
                    int index = std::stoi(key);
                    int tileID = value.get<int>();
                    int x = index % width;
                    int y = index / width;
                    if (x >= 0 && x < width && y >= 0 && y < height)
                        setTile(x, y, tileID);
                }
                catch (...)
                {
                }
            }
        }
    };

    // Helper to load sparse rotation layer
    auto loadRotationLayer = [&](const std::string &name, std::function<void(int, int, float)> setRot)
    {
        if (!j.contains(name))
            return;
        const auto &layer = j[name];
        if (layer.is_object())
        {
            for (auto &[key, value] : layer.items())
            {
                try
                {
                    int index = std::stoi(key);
                    float rot = value.get<float>();
                    int x = index % width;
                    int y = index / width;
                    if (x >= 0 && x < width && y >= 0 && y < height)
                        setRot(x, y, rot);
                }
                catch (...)
                {
                }
            }
        }
    };

    // Helper to load index array [idx1, idx2, ...]
    auto loadIndexArray = [&](const std::string &name, std::function<void(int, int, bool)> setFlag)
    {
        if (!j.contains(name))
            return;
        const auto &arr = j[name];
        if (arr.is_array())
        {
            for (const auto &idx : arr)
            {
                try
                {
                    int index = idx.get<int>();
                    int x = index % width;
                    int y = index / width;
                    if (x >= 0 && x < width && y >= 0 && y < height)
                        setFlag(x, y, true);
                }
                catch (...)
                {
                }
            }
        }
    };

    // Load collision and navigation
    loadIndexArray("collision", [this](int x, int y, bool v)
                   { SetTileCollision(x, y, v); });
    loadIndexArray("navigation", [this](int x, int y, bool v)
                   { SetNavigation(x, y, v); });
    loadIndexArray("navmesh", [this](int x, int y, bool v)
                   { SetNavigation(x, y, v); });

    // Load elevation
    loadTileLayer("elevation", [this](int x, int y, int v)
                  { SetElevation(x, y, v); });

    // Load dynamic layers (new format)
    bool sizeMismatch = false; // Track if layer data doesn't match new map size
    if (j.contains("dynamicLayers") && j["dynamicLayers"].is_array())
    {
        const auto &dynamicLayersArr = j["dynamicLayers"];
        m_Layers.clear();
        m_Layers.reserve(dynamicLayersArr.size());

        const size_t mapSize = static_cast<size_t>(width) * static_cast<size_t>(height);

        for (const auto &layerJson : dynamicLayersArr)
        {
            TileLayer layer;
            layer.name = layerJson.value("name", "");
            layer.renderOrder = layerJson.value("renderOrder", 0);
            layer.isBackground = layerJson.value("isBackground", true);
            layer.resize(mapSize);

            // Load tiles (sparse object)
            if (layerJson.contains("tiles") && layerJson["tiles"].is_object())
            {
                for (auto &[key, value] : layerJson["tiles"].items())
                {
                    try
                    {
                        size_t index = static_cast<size_t>(std::stoi(key));
                        if (index < mapSize)
                        {
                            layer.tiles[index] = value.get<int>();
                        }
                        else
                        {
                            sizeMismatch = true; // Index out of bounds for new size
                        }
                    }
                    catch (...)
                    {
                    }
                }
            }

            // Load rotation (sparse object)
            if (layerJson.contains("rotation") && layerJson["rotation"].is_object())
            {
                for (auto &[key, value] : layerJson["rotation"].items())
                {
                    try
                    {
                        size_t index = static_cast<size_t>(std::stoi(key));
                        if (index < mapSize)
                        {
                            layer.rotation[index] = value.get<float>();
                        }
                    }
                    catch (...)
                    {
                    }
                }
            }

            // Load noProjection (array of indices)
            if (layerJson.contains("noProjection") && layerJson["noProjection"].is_array())
            {
                for (const auto &idx : layerJson["noProjection"])
                {
                    try
                    {
                        size_t index = static_cast<size_t>(idx.get<int>());
                        if (index < mapSize)
                        {
                            layer.noProjection[index] = true;
                        }
                    }
                    catch (...)
                    {
                    }
                }
            }

            // Load ySorted (array of indices)
            if (layerJson.contains("ySorted") && layerJson["ySorted"].is_array())
            {
                for (const auto &idx : layerJson["ySorted"])
                {
                    try
                    {
                        size_t index = static_cast<size_t>(idx.get<int>());
                        if (index < mapSize)
                        {
                            layer.ySorted[index] = true;
                        }
                    }
                    catch (...)
                    {
                    }
                }
            }

            m_Layers.push_back(std::move(layer));
        }
        std::cout << "Loaded " << m_Layers.size() << " dynamic layers" << std::endl;

        // If layer data doesn't match new map size, regenerate the map
        if (sizeMismatch)
        {
            std::cout << "Map size changed - regenerating random map (" << width << "x" << height << ")" << std::endl;
            GenerateDefaultMap();
        }
    }

    // Load Particle Zones
    m_ParticleZones.clear();
    if (j.contains("particleZones") && j["particleZones"].is_array())
    {
        for (const auto &zoneJson : j["particleZones"])
        {
            ParticleZone zone;
            zone.position.x = zoneJson.value("x", 0.0f);
            zone.position.y = zoneJson.value("y", 0.0f);
            zone.size.x = zoneJson.value("width", 32.0f);
            zone.size.y = zoneJson.value("height", 32.0f);
            zone.type = static_cast<ParticleType>(zoneJson.value("type", 0));
            zone.enabled = zoneJson.value("enabled", true);
            zone.noProjection = zoneJson.value("noProjection", false);
            m_ParticleZones.push_back(zone);
        }
        std::cout << "Loaded " << m_ParticleZones.size() << " particle zones" << std::endl;
    }

    // Load NPCs
    if (npcs && j.contains("npcs") && j["npcs"].is_array())
    {
        npcs->clear();
        for (const auto &npcJson : j["npcs"])
        {
            std::string type = npcJson.value("type", "");
            int tileX = npcJson.value("tileX", 0);
            int tileY = npcJson.value("tileY", 0);
            std::string name = npcJson.value("name", "");
            std::string dialogue = npcJson.value("dialogue", "");

            if (!type.empty())
            {
                NonPlayerCharacter npc;
                if (npc.Load("assets/non-player/" + type + ".png"))
                {
                    npc.SetTilePosition(tileX, tileY, tileWidth);
                    if (!name.empty())
                        npc.SetName(name);
                    if (!dialogue.empty())
                        npc.SetDialogue(dialogue);

                    // Load dialogue tree (simplified format)
                    if (npcJson.contains("dialogueTree") && npcJson["dialogueTree"].is_object())
                    {
                        const auto &treeJson = npcJson["dialogueTree"];
                        DialogueTree tree;
                        tree.id = treeJson.value("id", npc.GetType());
                        tree.startNodeId = treeJson.value("start", "start");
                        std::string defaultSpeaker = treeJson.value("speaker", npc.GetName());

                        if (treeJson.contains("nodes") && treeJson["nodes"].is_object())
                        {
                            for (auto &[nodeId, nodeJson] : treeJson["nodes"].items())
                            {
                                DialogueNode node;
                                node.id = nodeId;
                                node.speaker = nodeJson.value("speaker", defaultSpeaker);
                                node.text = nodeJson.value("text", "");

                                if (nodeJson.contains("choices") && nodeJson["choices"].is_array())
                                {
                                    for (const auto &choiceJson : nodeJson["choices"])
                                    {
                                        DialogueOption opt;
                                        opt.text = choiceJson.value("text", "");
                                        opt.nextNodeId = choiceJson.value("goto", "");
                                        opt.conditions = ParseConditionString(choiceJson.value("when", ""));
                                        if (choiceJson.contains("do"))
                                            opt.consequences = ParseConsequenceArray(choiceJson["do"]);
                                        node.options.push_back(opt);
                                    }
                                }
                                tree.nodes[node.id] = node;
                            }
                        }
                        npc.SetDialogueTree(tree);
                    }

                    npcs->emplace_back(std::move(npc));
                }
            }
        }
        std::cout << "NPCs loaded: " << npcs->size() << std::endl;
    }

    // Load player position
    if (j.contains("player") && !j["player"].is_null())
    {
        const auto &player = j["player"];
        if (playerTileX)
            *playerTileX = player.value("tileX", -1);
        if (playerTileY)
            *playerTileY = player.value("tileY", -1);
        if (characterType)
            *characterType = player.value("characterType", -1);
    }

    // Load animated tile definitions
    if (j.contains("animatedTiles") && j["animatedTiles"].is_array())
    {
        m_AnimatedTiles.clear();
        for (const auto &animJson : j["animatedTiles"])
        {
            AnimatedTile anim;
            if (animJson.contains("frames") && animJson["frames"].is_array())
            {
                anim.frames = animJson["frames"].get<std::vector<int>>();
            }
            anim.frameDuration = animJson.value("frameDuration", 0.2f);
            m_AnimatedTiles.push_back(anim);
        }
        std::cout << "Loaded " << m_AnimatedTiles.size() << " animated tile definitions" << std::endl;
    }

    // Load per-layer animation maps (new format)
    size_t mapSize = static_cast<size_t>(m_MapWidth * m_MapHeight);
    if (j.contains("layerAnimationMaps") && j["layerAnimationMaps"].is_array())
    {
        const auto &layerAnimMaps = j["layerAnimationMaps"];
        for (size_t layerIdx = 0; layerIdx < layerAnimMaps.size() && layerIdx < m_Layers.size(); ++layerIdx)
        {
            if (layerAnimMaps[layerIdx].is_object())
            {
                auto &animMap = m_Layers[layerIdx].animationMap;
                if (animMap.size() != mapSize)
                {
                    animMap.assign(mapSize, -1);
                }
                for (auto &[key, value] : layerAnimMaps[layerIdx].items())
                {
                    size_t idx = static_cast<size_t>(std::stoi(key));
                    if (idx < animMap.size())
                    {
                        animMap[idx] = value.get<int>();
                    }
                }
            }
        }
        std::cout << "Loaded per-layer animation map placements" << std::endl;
    }
    // Backwards compatibility: load old "animationMap" format into layer 0
    else if (j.contains("animationMap") && j["animationMap"].is_object())
    {
        auto &animMap = m_Layers[0].animationMap;
        if (animMap.size() != mapSize)
        {
            animMap.assign(mapSize, -1);
        }
        for (auto &[key, value] : j["animationMap"].items())
        {
            size_t idx = static_cast<size_t>(std::stoi(key));
            if (idx < animMap.size())
            {
                animMap[idx] = value.get<int>();
            }
        }
        std::cout << "Loaded animation map placements (legacy format -> layer 0)" << std::endl;
    }

    // Load Corner Cut Blocked data
    if (j.contains("cornerCutBlocked") && j["cornerCutBlocked"].is_object())
    {
        // Ensure vector is sized correctly
        if (m_CornerCutBlocked.size() != mapSize)
        {
            m_CornerCutBlocked.assign(mapSize, 0);
        }
        for (auto &[key, value] : j["cornerCutBlocked"].items())
        {
            size_t idx = static_cast<size_t>(std::stoi(key));
            if (idx < m_CornerCutBlocked.size())
            {
                m_CornerCutBlocked[idx] = value.get<uint8_t>();
            }
        }
        std::cout << "Loaded corner cut blocked data" << std::endl;
    }

    // Debug: summarize animation state after load
    int animatedTileCount = 0;
    for (const auto &layer : m_Layers)
    {
        for (int a : layer.animationMap)
        {
            if (a >= 0)
                animatedTileCount++;
        }
    }
    std::cout << "[DEBUG] Animation state after load: "
              << m_AnimatedTiles.size() << " definitions, "
              << animatedTileCount << " placed tiles across all layers" << std::endl;
    for (size_t i = 0; i < m_AnimatedTiles.size(); ++i)
    {
        const auto &anim = m_AnimatedTiles[i];
        std::cout << "  Animation #" << i << ": " << anim.frames.size()
                  << " frames, " << anim.frameDuration << "s/frame" << std::endl;
    }

    std::cout << "Map loaded from " << filename << " (" << width << "x" << height << ")" << std::endl;
    return true;
}
