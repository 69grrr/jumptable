#pragma once

#include <vulkan/vulkan_core.h>

#include "saltus/shader_pack.hh"
#include "saltus/vulkan/fwd.hh"

namespace saltus::vulkan
{
    class VulkanShaderPack: public ShaderPack
    {
    public:
        VulkanShaderPack(std::shared_ptr<VulkanDevice>, ShaderPackCreateInfo create_info);
        ~VulkanShaderPack();

        const std::shared_ptr<VulkanDevice> &device() const;
        const std::vector<std::shared_ptr<VulkanBindGroupLayout>> &bind_group_layouts() const;

        const std::shared_ptr<VulkanShader> &vertex_shader() const;
        const std::shared_ptr<VulkanShader> &fragment_shader() const;
    private:
        std::shared_ptr<VulkanDevice> device_;
        std::vector<std::shared_ptr<VulkanBindGroupLayout>> bind_group_layouts_;

        std::shared_ptr<VulkanShader> vertex_shader_;
        std::shared_ptr<VulkanShader> fragment_shader_;
    };
} // namespace saltus::vulkan


