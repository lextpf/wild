#include "Game.h"
#include "DialogueManager.h"
#include "DialogueSystem.h"

#include <functional>
#include <iostream>
#include <vector>

namespace
{
    /**
     * Word-wrap text to fit within a maximum width.
     *
     * NOTE: This is ASCII/space-delimited only; UTF-8 glyphs and very long
     * tokens are not split. The renderer must be able to measure each whole
     * line via measureWidth().
     *
     * @param text         The text to wrap.
     * @param maxWidth     Maximum line width in pixels.
     * @param measureWidth Function to measure the pixel width of a string.
     * @return Vector of wrapped lines.
     */
    std::vector<std::string> WrapText(
        const std::string &text,
        float maxWidth,
        const std::function<float(const std::string &)> &measureWidth)
    {
        std::vector<std::string> lines;
        std::string currentLine;
        std::string word;

        // Add current word to line, wrapping if it exceeds max width
        auto commitWord = [&]()
        {
            if (word.empty())
                return;
            const std::string testLine = currentLine.empty() ? word : (currentLine + " " + word);
            if (measureWidth(testLine) > maxWidth && !currentLine.empty())
            {
                lines.push_back(currentLine);
                currentLine = word;
            }
            else
            {
                currentLine = testLine;
            }
            word.clear();
        };

        // Finish current line and start a new one
        auto commitLine = [&]()
        {
            if (!currentLine.empty())
            {
                lines.push_back(currentLine);
                currentLine.clear();
            }
        };

        for (char c : text)
        {
            if (c == ' ')
                commitWord();
            else if (c == '\n')
            {
                commitWord();
                commitLine();
            }
            else
                word += c;
        }
        commitWord();
        commitLine();

        return lines;
    }
}

void Game::RenderNPCHeadText()
{
    if (!m_InDialogue || m_DialogueText.empty() || !m_DialogueNPC)
    {
        return;
    }

    // Get NPC position in screen space
    glm::vec2 npcWorldPos = m_DialogueNPC->GetPosition();
    glm::vec2 npcScreenPos = npcWorldPos - m_CameraPosition;

    // Position text above the NPC's head
    float textAreaWidth = 180.0f;
    const float NPC_SPRITE_HEIGHT = PlayerCharacter::RENDER_HEIGHT;
    float npcTopY = npcScreenPos.y - NPC_SPRITE_HEIGHT;
    float npcCenterX = npcScreenPos.x;

    glm::vec2 textAreaPos(
        npcCenterX - textAreaWidth * 0.5f,
        npcTopY - 10.0f);
    // TODO: Fixed height for now, should adjust based on zoom level
    // TODO: Clamp to the visible screen so head text cannot render off-screen.
    glm::vec2 textAreaSize(textAreaWidth, 50.0f);

    RenderDialogueText(textAreaPos, textAreaSize);
}

void Game::RenderDialogueText(glm::vec2 boxPos, glm::vec2 boxSize)
{
    if (m_DialogueText.empty())
    {
        return;
    }

    float scale = 0.2f;
    float lineHeight = 6.0f;
    float maxWidth = boxSize.x - 20.0f;

    auto lines = WrapText(m_DialogueText, maxWidth, [&](const std::string &s)
                          { return m_Renderer->GetTextWidth(s, scale); });

    // Render each line, centered horizontally
    float currentY = boxPos.y;
    glm::vec3 textColor(1.0f, 1.0f, 1.0f);

    for (const std::string &line : lines)
    {
        if (!line.empty())
        {
            float lineWidth = m_Renderer->GetTextWidth(line, scale);
            float lineStartX = boxPos.x + (boxSize.x - lineWidth) * 0.5f;
            m_Renderer->DrawText(line, glm::vec2(lineStartX, currentY), scale, textColor);
        }
        currentY += lineHeight;
    }
    // TODO: Clip or truncate lines when they exceed box height; currently they can spill past the box.
}

void Game::RenderDialogueTreeBox()
{
    if (!m_DialogueManager.IsActive())
    {
        return;
    }

    const DialogueNode *node = m_DialogueManager.GetCurrentNode();
    if (!node)
    {
        return;
    }

    // Get world dimensions for positioning, adjusted for zoom
    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_CameraZoom;
    float worldHeight = baseWorldHeight / m_CameraZoom;

    // Scale factor for UI elements, inverse of zoom so they appear constant size on screen
    float z = 1.0f / m_CameraZoom;

    // Dialogue box dimensions and position (fixed at bottom of visible screen)
    float boxWidth = baseWorldWidth * 0.9f * z;
    float boxHeight = 60.0f * z;
    float boxX = (worldWidth - boxWidth) * 0.5f;
    float boxY = worldHeight - boxHeight - (10.0f * z);

    // Main background - dark grey, semi-transparent
    glm::vec4 bgColor(0.18f, 0.16f, 0.14f, 0.85f);
    m_Renderer->DrawColoredRect(glm::vec2(boxX, boxY), glm::vec2(boxWidth, boxHeight), bgColor);

    // Outer border - off-white
    float borderWidth = 2.0f * z;
    glm::vec4 borderColorOuter(0.92f, 0.9f, 0.85f, 1.0f);
    m_Renderer->DrawColoredRect(glm::vec2(boxX, boxY), glm::vec2(boxWidth, borderWidth), borderColorOuter);                           // Top
    m_Renderer->DrawColoredRect(glm::vec2(boxX, boxY + boxHeight - borderWidth), glm::vec2(boxWidth, borderWidth), borderColorOuter); // Bottom
    m_Renderer->DrawColoredRect(glm::vec2(boxX, boxY), glm::vec2(borderWidth, boxHeight), borderColorOuter);                          // Left
    m_Renderer->DrawColoredRect(glm::vec2(boxX + boxWidth - borderWidth, boxY), glm::vec2(borderWidth, boxHeight), borderColorOuter); // Right

    // Inner border - off-white
    float innerBorderOffset = 3.0f * z;
    float innerBorderWidth = 1.0f * z;
    glm::vec4 borderColorInner(0.85f, 0.82f, 0.78f, 0.7f);
    m_Renderer->DrawColoredRect(glm::vec2(boxX + innerBorderOffset, boxY + innerBorderOffset),
                                glm::vec2(boxWidth - innerBorderOffset * 2, innerBorderWidth), borderColorInner); // Top
    m_Renderer->DrawColoredRect(glm::vec2(boxX + innerBorderOffset, boxY + boxHeight - innerBorderOffset - innerBorderWidth),
                                glm::vec2(boxWidth - innerBorderOffset * 2, innerBorderWidth), borderColorInner); // Bottom
    m_Renderer->DrawColoredRect(glm::vec2(boxX + innerBorderOffset, boxY + innerBorderOffset),
                                glm::vec2(innerBorderWidth, boxHeight - innerBorderOffset * 2), borderColorInner); // Left
    m_Renderer->DrawColoredRect(glm::vec2(boxX + boxWidth - innerBorderOffset - innerBorderWidth, boxY + innerBorderOffset),
                                glm::vec2(innerBorderWidth, boxHeight - innerBorderOffset * 2), borderColorInner); // Right

    // Corner decorations - off-white
    float cornerSize = 5.0f * z;
    glm::vec4 cornerColor(0.9f, 0.88f, 0.82f, 1.0f);
    m_Renderer->DrawColoredRect(glm::vec2(boxX - 1.0f * z, boxY - 1.0f * z), glm::vec2(cornerSize, cornerSize), cornerColor);                                                  // Top-left
    m_Renderer->DrawColoredRect(glm::vec2(boxX + boxWidth - cornerSize + 1.0f * z, boxY - 1.0f * z), glm::vec2(cornerSize, cornerSize), cornerColor);                          // Top-right
    m_Renderer->DrawColoredRect(glm::vec2(boxX - 1.0f * z, boxY + boxHeight - cornerSize + 1.0f * z), glm::vec2(cornerSize, cornerSize), cornerColor);                         // Bottom-left
    m_Renderer->DrawColoredRect(glm::vec2(boxX + boxWidth - cornerSize + 1.0f * z, boxY + boxHeight - cornerSize + 1.0f * z), glm::vec2(cornerSize, cornerSize), cornerColor); // Bottom-right

    float padding = 10.0f * z;
    float textScale = 0.18f * z;
    float lineHeight = 5.5f * z;
    float contentTopMargin = 4.0f * z; // Extra space at top for nameplate
    float contentStartY = boxY + padding + contentTopMargin;
    float currentY = contentStartY;

    // Get text ascent for proper alignment
    float textAscent = m_Renderer->GetTextAscent(textScale);
    float outlineSize = 2.0f; // Constant outline size
    // TODO: Scale outlineSize by z to keep stroke weight visually consistent across zoom levels.
    float textAlpha = 1.0f;   // Full opacity text

    // Calculate available content height
    float contentBottomY = boxY + boxHeight - padding;
    float availableHeight = contentBottomY - contentStartY;

    float speakerHeight = 0.0f;
    if (!node->speaker.empty())
    {
        // Speaker nameplate background
        float speakerScale = textScale * 1.2f;
        float speakerAscent = m_Renderer->GetTextAscent(speakerScale);
        float namePadding = 4.0f * z; // Padding on left and right inside nameplate
        float actualNameWidth = m_Renderer->GetTextWidth(node->speaker, speakerScale);
        float nameWidth = actualNameWidth + namePadding * 2;
        float nameHeight = speakerAscent + 4.0f * z;
        float nameX = boxX + padding - namePadding;
        float nameY = currentY - speakerAscent - 2.0f * z;

        // Nameplate background - muted gold
        glm::vec4 nameBg(0.72f, 0.58f, 0.22f, 1.0f);
        m_Renderer->DrawColoredRect(glm::vec2(nameX, nameY), glm::vec2(nameWidth, nameHeight), nameBg);

        // Nameplate border - off-white
        glm::vec4 nameBorder(0.85f, 0.82f, 0.78f, 0.7f);
        m_Renderer->DrawColoredRect(glm::vec2(nameX, nameY), glm::vec2(nameWidth, 1.0f * z), nameBorder);
        m_Renderer->DrawColoredRect(glm::vec2(nameX, nameY + nameHeight - 1.0f * z), glm::vec2(nameWidth, 1.0f * z), nameBorder);
        m_Renderer->DrawColoredRect(glm::vec2(nameX, nameY), glm::vec2(1.0f * z, nameHeight), nameBorder);
        m_Renderer->DrawColoredRect(glm::vec2(nameX + nameWidth - 1.0f * z, nameY), glm::vec2(1.0f * z, nameHeight), nameBorder);

        glm::vec3 speakerColor(1.0f, 0.9f, 0.5f);
        m_Renderer->DrawText(node->speaker, glm::vec2(boxX + padding, currentY - 1.0f * z), speakerScale, speakerColor, outlineSize, textAlpha);
        speakerHeight = lineHeight + 4.0f * z;
        currentY += speakerHeight;
    }

    const float maxTextWidth = boxWidth - padding * 2;
    auto allLines = WrapText(node->text, maxTextWidth, [&](const std::string &s)
                             { return m_Renderer->GetTextWidth(s, textScale); });

    const auto &visibleOptions = m_DialogueManager.GetVisibleOptions();
    int numOptions = static_cast<int>(visibleOptions.size());

    // Calculate how many lines fit in the available space
    float heightAfterSpeaker = availableHeight - speakerHeight;
    int totalLines = static_cast<int>(allLines.size());

    // Options are positioned at the bottom with minimal padding, giving more room for text
    float optionsBottomPadding = 7.0f * z; // Padding for options at bottom
    float effectiveOptionsSpace = static_cast<float>(numOptions) * lineHeight - (padding - optionsBottomPadding);
    if (effectiveOptionsSpace < 0)
        effectiveOptionsSpace = 0;
    float spaceForText = heightAfterSpeaker - effectiveOptionsSpace;
    int maxTextLines = static_cast<int>(spaceForText / lineHeight) + 1;
    if (maxTextLines < 1)
        maxTextLines = 1;

    // Check if text fits in the space above options
    bool everythingFits = (totalLines <= maxTextLines);
    int totalPages = 1;

    if (!everythingFits)
    {
        // Each page can show maxTextLines worth of text (except last page)
        int remainingLines = totalLines - maxTextLines;
        totalPages = 1 + (remainingLines + maxTextLines - 1) / maxTextLines;
    }
    m_DialogueTotalPages = totalPages;

    // Clamp current page
    if (m_DialoguePage >= totalPages)
        m_DialoguePage = totalPages - 1;
    if (m_DialoguePage < 0)
        m_DialoguePage = 0;

    bool isLastPage = (m_DialoguePage == totalPages - 1);

    // Calculate which lines to show on current page
    int startLine = 0;
    int linesToShow = 0;

    if (totalPages == 1)
    {
        // Everything fits
        startLine = 0;
        linesToShow = totalLines;
    }
    else if (isLastPage)
    {
        // Last page shows remaining lines that fit above options
        startLine = m_DialoguePage * maxTextLines;
        linesToShow = totalLines - startLine;
    }
    else
    {
        // Earlier pages show maxTextLines worth of text
        startLine = m_DialoguePage * maxTextLines;
        linesToShow = maxTextLines;
    }

    // Render dialogue text lines
    glm::vec3 textColor(0.95f, 0.93f, 0.88f);
    for (int i = 0; i < linesToShow && (startLine + i) < totalLines; ++i)
    {
        m_Renderer->DrawText(allLines[startLine + i], glm::vec2(boxX + padding, currentY), textScale, textColor, outlineSize, textAlpha);
        currentY += lineHeight;
    }
    currentY += 1.0f * z;

    // Position for bottom-right prompt
    float promptY = boxY + boxHeight - padding;
    float promptX = boxX + boxWidth - padding - 12.0f * z;

    if (!isLastPage)
    {
        // Show continue prompt at bottom right
        glm::vec3 promptColor(0.55f, 0.52f, 0.48f);
        m_Renderer->DrawText("Continue", glm::vec2(promptX, promptY), textScale * 0.85f, promptColor, outlineSize, 0.7f);

        float promptAscent = m_Renderer->GetTextAscent(textScale * 0.85f);
        float arrowCenterY = promptY - promptAscent * 0.5f;
        float arrowX = promptX - 6.0f * z;
        glm::vec4 arrowColor(0.65f, 0.52f, 0.2f, 0.85f);
        m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY - 2.0f * z), glm::vec2(1.0f * z, 1.0f * z), arrowColor);
        m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY - 1.0f * z), glm::vec2(2.0f * z, 1.0f * z), arrowColor);
        m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY), glm::vec2(3.0f * z, 1.0f * z), arrowColor);
        m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY + 1.0f * z), glm::vec2(2.0f * z, 1.0f * z), arrowColor);
        m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY + 2.0f * z), glm::vec2(1.0f * z, 1.0f * z), arrowColor);
    }
    else if (visibleOptions.empty())
    {
        // Last page with no options - show continue at bottom right
        glm::vec3 promptColor(0.55f, 0.52f, 0.48f);
        m_Renderer->DrawText("Continue", glm::vec2(promptX, promptY), textScale * 0.85f, promptColor, outlineSize, 0.7f);

        float promptAscent = m_Renderer->GetTextAscent(textScale * 0.85f);
        float arrowCenterY = promptY - promptAscent * 0.5f;
        float arrowX = promptX - 6.0f * z;
        glm::vec4 arrowColor(0.65f, 0.52f, 0.2f, 0.85f);
        m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY - 2.0f * z), glm::vec2(1.0f * z, 1.0f * z), arrowColor);
        m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY - 1.0f * z), glm::vec2(2.0f * z, 1.0f * z), arrowColor);
        m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY), glm::vec2(3.0f * z, 1.0f * z), arrowColor);
        m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY + 1.0f * z), glm::vec2(2.0f * z, 1.0f * z), arrowColor);
        m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY + 2.0f * z), glm::vec2(1.0f * z, 1.0f * z), arrowColor);
    }
    else
    {
        // Last page with options - show response options right under the text
        int selectedIndex = m_DialogueManager.GetSelectedOptionIndex();

        for (size_t i = 0; i < visibleOptions.size(); ++i)
        {
            const DialogueOption *opt = visibleOptions[i];
            bool isSelected = (static_cast<int>(i) == selectedIndex);

            if (isSelected)
            {
                float arrowCenterY = currentY - textAscent * 0.5f;
                float arrowX = boxX + padding;

                glm::vec4 arrowGold(1.0f, 0.88f, 0.4f, 1.0f);
                m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY - 2.0f * z), glm::vec2(1.0f * z, 1.0f * z), arrowGold);
                m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY - 1.0f * z), glm::vec2(2.0f * z, 1.0f * z), arrowGold);
                m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY), glm::vec2(3.0f * z, 1.0f * z), arrowGold);
                m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY + 1.0f * z), glm::vec2(2.0f * z, 1.0f * z), arrowGold);
                m_Renderer->DrawColoredRect(glm::vec2(arrowX, arrowCenterY + 2.0f * z), glm::vec2(1.0f * z, 1.0f * z), arrowGold);
            }

            std::string prefix = "   ";
            glm::vec3 optionColor = isSelected ? glm::vec3(1.0f, 0.9f, 0.5f) : glm::vec3(0.75f, 0.72f, 0.68f);

            // Check if this option gives a quest
            bool givesQuest = false;
            for (const auto &cons : opt->consequences)
            {
                if ((cons.type == DialogueConsequence::Type::SET_FLAG ||
                     cons.type == DialogueConsequence::Type::SET_FLAG_VALUE) &&
                    cons.key.find("accepted_") == 0 &&
                    cons.key.find("_quest") != std::string::npos)
                {
                    givesQuest = true;
                    break;
                }
            }

            std::string displayText = prefix + opt->text;
            // TODO: Wrap long option text to maxTextWidth; current rendering can overflow the box.
            m_Renderer->DrawText(displayText, glm::vec2(boxX + padding, currentY), textScale, optionColor, outlineSize, textAlpha);

            // Draw the exclamation mark in gold if this is a quest option
            if (givesQuest)
            {
                glm::vec3 questYellow(1.0f, 0.88f, 0.4f);
                float textWidth = m_Renderer->GetTextWidth(prefix + opt->text + " ", textScale);
                float exclamationX = boxX + padding + textWidth;
                m_Renderer->DrawText(">!<", glm::vec2(exclamationX, currentY), textScale, questYellow, outlineSize, 1.0f);
            }
            currentY += lineHeight;
        }
    }
}

bool Game::IsDialogueOnLastPage() const
{
    return m_DialoguePage >= m_DialogueTotalPages - 1;
}
