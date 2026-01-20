#include "VulkanRenderer.h"
#include "Texture.h"

#include <iostream>
#include <cstring>
#include <stdexcept>

#define VK_CHECK(x)                                                                                          \
    do                                                                                                       \
    {                                                                                                        \
        VkResult result = x;                                                                                 \
        if (result != VK_SUCCESS)                                                                            \
        {                                                                                                    \
            std::cerr << "Vulkan error at " << __FILE__ << ":" << __LINE__ << " - " << result << std::endl;  \
            throw std::runtime_error("Vulkan operation failed");                                             \
        }                                                                                                    \
    } while (0)

void VulkanRenderer::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                 VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateImage(m_Device, &imageInfo, nullptr, &image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_Device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(m_Device, &allocInfo, nullptr, &imageMemory));
    VK_CHECK(vkBindImageMemory(m_Device, image, imageMemory, 0));
}

void VulkanRenderer::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = m_CommandBuffers[m_CurrentFrame];

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::runtime_error("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanRenderer::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = m_CommandBuffers[m_CurrentFrame];

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void VulkanRenderer::CreateTextureSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    VK_CHECK(vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_TextureSampler));
}

VulkanRenderer::TextureResources &VulkanRenderer::GetOrCreateTexture(const Texture &texture)
{
    // Use the Texture object's address as the unique cache key.
    // Each Texture object occupies a unique memory location, so this guarantees
    // no collisions between different textures (unlike a size-based hash).
    const Texture *textureKey = &texture;

    // Check if texture already exists in cache
    auto it = m_TextureCache.find(textureKey);
    if (it != m_TextureCache.end() && it->second.initialized)
    {
        return it->second;
    }

    // Create new texture resources
    TextureResources resources{};
    resources.initialized = false;

    int width = texture.GetWidth();
    int height = texture.GetHeight();

    if (width <= 0 || height <= 0)
    {
        // Return white texture if invalid
        resources.image = m_WhiteTextureImage;
        resources.imageView = m_WhiteTextureImageView;
        resources.memory = m_WhiteTextureImageMemory;
        resources.initialized = true;
        m_TextureCache[textureKey] = resources;
        return m_TextureCache[textureKey];
    }

// Try to use texture's Vulkan resources if they exist
#ifdef USE_VULKAN
    VkImageView texImageView = texture.GetVulkanImageView();
    if (texImageView != VK_NULL_HANDLE)
    {
        // Texture already has Vulkan resources - use them
        resources.imageView = texImageView;
        resources.initialized = true;
        m_TextureCache[textureKey] = resources;
        return m_TextureCache[textureKey];
    }

    // Texture doesn't have Vulkan resources yet - need to create them
    // But we can't call CreateVulkanTexture from here because we don't have access to the Texture's private methods
    // So we'll return white texture for now and log a warning
    std::cerr << "Warning: Texture " << static_cast<const void *>(textureKey) << " (size " << width << "x" << height
              << ") not uploaded to Vulkan yet. Using white texture fallback." << std::endl;
    std::cerr.flush();
#endif

    // Use white texture as fallback
    resources.image = m_WhiteTextureImage;
    resources.imageView = m_WhiteTextureImageView;
    resources.memory = m_WhiteTextureImageMemory;
    resources.initialized = true;
    m_TextureCache[textureKey] = resources;
    return m_TextureCache[textureKey];
}

void VulkanRenderer::UploadTextureData(VkImage image, unsigned char *data, int width, int height, int channels)
{
    if (!data || width <= 0 || height <= 0)
        return;

    VkDeviceSize imageSize = width * height * channels;

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    // Copy data to staging buffer
    void *mappedData;
    vkMapMemory(m_Device, stagingBufferMemory, 0, imageSize, 0, &mappedData);
    memcpy(mappedData, data, imageSize);
    vkUnmapMemory(m_Device, stagingBufferMemory);

    // Transition image layout for transfer
    TransitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy buffer to image
    CopyBufferToImage(stagingBuffer, image, width, height);

    // Transition image layout for shader reading
    TransitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Cleanup staging buffer
    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
}
