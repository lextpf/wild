#include "VulkanShader.h"

#include <iostream>
#include <fstream>

VkShaderModule VulkanShader::CreateShaderModule(VkDevice device, const std::vector<uint32_t> &code)
{
    if (code.empty())
    {
        throw std::runtime_error("Cannot create shader module from empty code!");
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}

// Load SPIR-V file
static std::vector<uint32_t> ReadSPIRVFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        std::cerr << "Failed to open SPIR-V file: " << filename << std::endl;
        return {};
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
    file.close();

    return buffer;
}

std::vector<uint32_t> VulkanShader::GetVertexShaderSPIRV()
{
    // Try to load compiled SPIR-V file
    std::vector<uint32_t> code = ReadSPIRVFile("shaders/sprite.vert.spv");
    if (code.empty())
    {
        std::cerr << "Warning: Could not load shaders/sprite.vert.spv" << std::endl;
        std::cerr << "Please compile shaders/sprite.vert to SPIR-V using:" << std::endl;
        std::cerr << "  glslangValidator -V shaders/sprite.vert -o shaders/sprite.vert.spv" << std::endl;
    }
    return code;
}

std::vector<uint32_t> VulkanShader::GetFragmentShaderSPIRV()
{
    // Try to load compiled SPIR-V file
    std::vector<uint32_t> code = ReadSPIRVFile("shaders/sprite.frag.spv");
    if (code.empty())
    {
        std::cerr << "Warning: Could not load shaders/sprite.frag.spv" << std::endl;
        std::cerr << "Please compile shaders/sprite.frag to SPIR-V using:" << std::endl;
        std::cerr << "  glslangValidator -V shaders/sprite.frag -o shaders/sprite.frag.spv" << std::endl;
    }
    return code;
}
