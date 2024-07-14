#pragma once

#include <vulkan/vulkan_core.h>

#include "saltus/texture.hh"
#include "saltus/vulkan/fwd.hh"

namespace saltus::vulkan
{
    class VulkanTexture: public Texture
    {
    public:
        VulkanTexture(std::shared_ptr<VulkanDevice>, TextureCreateInfo);
        ~VulkanTexture();

        const std::shared_ptr<VulkanDevice> &device() const;

        const std::shared_ptr<VulkanImage> &image() const;
        const std::shared_ptr<VulkanSampler> &sampler() const;

        const std::shared_ptr<RawVulkanImageView> &image_view() const;

    private:
        std::shared_ptr<VulkanDevice> device_;

        std::shared_ptr<VulkanImage> image_;
        std::shared_ptr<VulkanSampler> sampler_;

        std::shared_ptr<RawVulkanImageView> image_view_;
    };
} // namespace saltus::vulkan
