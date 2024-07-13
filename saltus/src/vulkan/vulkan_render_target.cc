#include "saltus/vulkan/vulkan_render_target.hh"

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>
#include <logger/level.hh>

#include "saltus/renderer.hh"
#include "saltus/vulkan/frame_ring.hh"
#include "saltus/vulkan/raw_vulkan_image.hh"
#include "saltus/vulkan/raw_vulkan_image_view.hh"

namespace saltus::vulkan
{
    VkPresentModeKHR renderer_present_mode_to_vulkan_present_mode(RendererPresentMode mode)
    {
        switch (mode)
        {
        case RendererPresentMode::Immediate:
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        case RendererPresentMode::Mailbox:
            return VK_PRESENT_MODE_MAILBOX_KHR;
        case RendererPresentMode::VSync:
            return VK_PRESENT_MODE_FIFO_KHR;
        }
        throw std::runtime_error("Unknown present mode");
    }

    RendererPresentMode vulkan_present_mode_to_renderer_present_mode(VkPresentModeKHR mode)
    {
        switch (mode)
        {
        case VK_PRESENT_MODE_IMMEDIATE_KHR:
            return RendererPresentMode::Immediate;
        case VK_PRESENT_MODE_MAILBOX_KHR:
            return RendererPresentMode::Immediate;
        case VK_PRESENT_MODE_FIFO_KHR:
            return RendererPresentMode::VSync;
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
            throw std::runtime_error("Unknown present mode");
        case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
            throw std::runtime_error("Unknown present mode");
        case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
            throw std::runtime_error("Unknown present mode");
        case VK_PRESENT_MODE_MAX_ENUM_KHR:
            throw std::runtime_error("Unknown present mode");
        }
        throw std::runtime_error("Unknown present mode");
    }

    VulkanRenderTarget::RenderBuffer::RenderBuffer(
        std::shared_ptr<VulkanRenderTarget> render_target,
        uint32_t,
        bool is_depth
    ): render_target_(render_target), is_depth_(is_depth) {
        VkFormat format = is_depth
            ? render_target->depth_format()
            : render_target->swapchain_image_format();

        auto extent = render_target->swapchain_extent();
        image_ = std::make_shared<RawVulkanImage>(
            RawVulkanImage::Builder(render_target->device())
                .with_format(format)
                .with_usage(is_depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                .with_size(extent.width, extent.height)
                .with_sample_count(static_cast<VkSampleCountFlagBits>(render_target->msaa_samples_))
        );

        image_view_ = std::make_shared<RawVulkanImageView>(
            image_,
            format,
            VK_IMAGE_VIEW_TYPE_2D,
            is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT
        );
    }

    VulkanRenderTarget::RenderBuffer::~RenderBuffer() = default;

    const std::shared_ptr<VulkanRenderTarget> &VulkanRenderTarget::RenderBuffer::render_target() const
    {
        return render_target_;
    }

    bool VulkanRenderTarget::RenderBuffer::is_depth() const
    {
        return is_depth_;
    }
   
    const std::shared_ptr<RawVulkanImage> &VulkanRenderTarget::RenderBuffer::image() const
    {
        return image_;
    }

    const std::shared_ptr<RawVulkanImageView> &VulkanRenderTarget::RenderBuffer::image_view() const
    {
        return image_view_;
    }

    VulkanRenderTarget::VulkanRenderTarget(
        std::shared_ptr<FrameRing> frame_ring,
        std::shared_ptr<VulkanDevice> device,
        RendererPresentMode target_present_mode,
        MsaaSamples msaa_samples
    ):
        frame_ring_(frame_ring),
        device_(device),
        target_present_mode_(target_present_mode),
        depth_resource_(frame_ring->allocate_resource<RenderBuffer>(
            [this](uint32_t fi){
                return std::make_unique<RenderBuffer>(this->shared_from_this(), fi, true);
            }
        ))
    {
        msaa_samples_ = static_cast<uint32_t>(msaa_samples);

        create();
    }

    VulkanRenderTarget::~VulkanRenderTarget()
    {
        destroy();
    }

    const std::shared_ptr<VulkanDevice> &VulkanRenderTarget::device() const
    {
        return device_;
    }

    const RendererPresentMode &VulkanRenderTarget::target_present_mode() const
    {
        return target_present_mode_;
    }

    void VulkanRenderTarget::target_present_mode(RendererPresentMode present_mode)
    {
        target_present_mode_ = present_mode;
        recreate();
    }

    VkSampleCountFlagBits VulkanRenderTarget::msaa_sample_bits() const
    {
        return static_cast<VkSampleCountFlagBits>(msaa_samples_);
    }

    const VkFormat &VulkanRenderTarget::swapchain_image_format() const
    {
        return swapchain_image_format_;
    }
    const VkExtent2D &VulkanRenderTarget::swapchain_extent() const
    {
        return swapchain_extent_;
    }
    const VkSwapchainKHR &VulkanRenderTarget::swapchain() const
    {
        return swapchain_;
    }
    const VkPresentModeKHR &VulkanRenderTarget::present_mode() const
    {
        return present_mode_;
    }
    const std::vector<VkImage> &VulkanRenderTarget::swapchain_images() const
    {
        return swapchain_images_;
    }
    const std::vector<VkImageView> &VulkanRenderTarget::swapchain_image_views() const
    {
        return swapchain_image_views_;
    }

    const VkFormat &VulkanRenderTarget::depth_format() const
    {
        return depth_format_;
    }
    const FrameResource<VulkanRenderTarget::RenderBuffer> &VulkanRenderTarget::depth_resource() const
    {
        return depth_resource_;
    }

    void VulkanRenderTarget::resize_if_changed()
    {
        SwapChainSupportDetails swap_chain_support =
            device_->get_physical_device_swap_chain_support_details();
        VkExtent2D new_extent =
            choose_swap_extent(swap_chain_support.capabilities);
        if (new_extent.width != swapchain_extent_.width || new_extent.height != swapchain_extent_.height)
        {
            recreate();
        }
    }
    
    void VulkanRenderTarget::recreate()
    {
        vkDeviceWaitIdle(*device_);
        destroy();
        create();
    }

    VkSurfaceFormatKHR VulkanRenderTarget::choose_swap_chain_format(
        const std::vector<VkSurfaceFormatKHR> &availableFormats
    ) {
        if (availableFormats.size() == 0)
            throw std::runtime_error("VulkanRenderTarget::choose_swap_chain_format was given an empty vector");
        for (const auto &format : availableFormats)
        {
            if (
                format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            ) {
                return format;
            }
        }
        return availableFormats[0];
    }

    VkPresentModeKHR VulkanRenderTarget::choose_swap_chain_present_mode(
        const std::vector<VkPresentModeKHR> &availablePresentModes
    ) {
        VkPresentModeKHR target = renderer_present_mode_to_vulkan_present_mode(target_present_mode_);
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == target) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VulkanRenderTarget::choose_swap_extent(
        const VkSurfaceCapabilitiesKHR &capabilities
    ) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }

        auto window_geometry = device_->window().request_geometry();

        uint32_t target_width = std::clamp(
            static_cast<uint32_t>(window_geometry.width),
            capabilities.minImageExtent.width, capabilities.maxImageExtent.width
        );
        uint32_t target_height = std::clamp(
            static_cast<uint32_t>(window_geometry.height),
            capabilities.minImageExtent.height, capabilities.maxImageExtent.height
        );

        return VkExtent2D {
            .width = target_width,
            .height = target_height,
        };
    }

    void VulkanRenderTarget::create()
    {
        create_swap_chain();
        create_images();
        create_image_views();

        uint32_t max_samples = device_->max_usable_sample_count();
        if (msaa_samples_ > max_samples)
        {
            logger::warn() << "Could not use target msaa samples: " << msaa_samples_ << " is above hardware maximum (" << max_samples << "), it has been clamped\n";
            msaa_samples_ = max_samples;
        }

        if (msaa_samples_ > 1)
        {
            backbuffer_resource_ = frame_ring_->allocate_resource<RenderBuffer>(
                [this](uint32_t fi){
                    return std::make_unique<RenderBuffer>(this->shared_from_this(), fi, false);
                }
            );
        }

        // TODO: Choose an otherone if not available
        depth_format_ = VK_FORMAT_D32_SFLOAT;
    }

    uint32_t VulkanRenderTarget::acquire_next_image(
        VkSemaphore semaphore,
        VkFence fence
    ) {
        uint32_t index = 0;
        VkResult result = vkAcquireNextImageKHR(
            *device_, swapchain_, UINT32_MAX, semaphore, fence, &index
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreate();
            // try again
            return acquire_next_image(semaphore, fence);
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            throw std::runtime_error("Could not acquire an image");
        return index;
    }

    VkImage VulkanRenderTarget::get_render_image(uint32_t acquired_image, uint32_t frame_index)
    {
        if (backbuffer_resource_.has_value())
            return backbuffer_resource_.value().get(frame_index).image()->handle();
        return get_present_image(acquired_image, frame_index);
    }

    VkImageView VulkanRenderTarget::get_render_image_view(uint32_t acquired_image, uint32_t frame_index)
    {
        if (backbuffer_resource_.has_value())
            return backbuffer_resource_.value().get(frame_index).image_view()->handle();
        return get_present_image_view(acquired_image, frame_index);
    }

    VkImage VulkanRenderTarget::get_present_image(uint32_t acquired_image, uint32_t)
    {
        return swapchain_images_[acquired_image];
    }

    VkImageView VulkanRenderTarget::get_present_image_view(uint32_t acquired_image, uint32_t)
    {
        return swapchain_image_views_[acquired_image];
    }

    void VulkanRenderTarget::create_swap_chain()
    {
        SwapChainSupportDetails swap_chain_support =
            device_->get_physical_device_swap_chain_support_details();

        VkSurfaceFormatKHR surface_format =
            choose_swap_chain_format(swap_chain_support.formats);
        swapchain_image_format_ = surface_format.format;
        VkExtent2D extent =
            choose_swap_extent(swap_chain_support.capabilities);
        swapchain_extent_ = extent;
        VkPresentModeKHR present_mode =
            choose_swap_chain_present_mode(swap_chain_support.present_modes);
        present_mode_ = present_mode;
        logger::trace() << "Using present mode '"
            << string_VkPresentModeKHR(present_mode) << "'\n";

        uint32_t max_image_count = swap_chain_support.capabilities.maxImageCount;
        uint32_t min_image_count = swap_chain_support.capabilities.minImageCount;
        uint32_t image_count = min_image_count + 1;
        if (max_image_count != 0 && image_count > max_image_count)
            image_count = max_image_count;
        logger::trace() << "Using " << image_count << " swapchain images (min: "
                    << min_image_count << ", max: " << max_image_count << ")\n";

        VkSwapchainCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = device_->surface();
        create_info.minImageCount = image_count;
        create_info.imageFormat = surface_format.format;
        create_info.imageColorSpace = surface_format.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = device_->get_physical_device_family_indices();

        std::array<uint32_t, 2> queue_family_indices = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value(),
        };

        if (queue_family_indices[0] != queue_family_indices[1]) {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = queue_family_indices.size();
            create_info.pQueueFamilyIndices = queue_family_indices.data();
        } else {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            create_info.queueFamilyIndexCount = 0; // Optional
            create_info.pQueueFamilyIndices = nullptr; // Optional
        }

        create_info.preTransform = swap_chain_support.capabilities.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode = present_mode;
        create_info.clipped = VK_TRUE;

        VkResult result =
            vkCreateSwapchainKHR(device_->device(), &create_info, nullptr, &swapchain_);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Could not create swap chain");
    }

    void VulkanRenderTarget::create_images()
    {
        uint32_t real_image_count = 0;
        vkGetSwapchainImagesKHR(
            *device_, swapchain_, &real_image_count, nullptr
        );
        swapchain_images_.resize(real_image_count);
        vkGetSwapchainImagesKHR(
            *device_, swapchain_, &real_image_count, swapchain_images_.data()
        );
    }

    void VulkanRenderTarget::create_image_views()
    {
        swapchain_image_views_.clear();

        for (const auto &image : swapchain_images_)
        {
            VkImageViewCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = image;
            create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format = swapchain_image_format_;
            create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel = 0;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount = 1;
            VkImageView image_view;
            VkResult result =
                vkCreateImageView(*device_, &create_info, nullptr, &image_view);
            if (result != VK_SUCCESS)
                throw std::runtime_error("Failed to create an image view");
            swapchain_image_views_.push_back(image_view);
        }
    }
    
    void VulkanRenderTarget::destroy()
    {
        destroy_image_views();
        swapchain_image_views_.clear();
        destroy_swap_chain();
        swapchain_images_.clear();
        swapchain_ = VK_NULL_HANDLE;

        backbuffer_resource_ = std::nullopt;

        depth_resource_.clear();
    }

    void VulkanRenderTarget::destroy_swap_chain()
    {
        vkDestroySwapchainKHR(*device_, swapchain_, nullptr);
    }

    void VulkanRenderTarget::destroy_image_views()
    {
        for (const auto &view : swapchain_image_views_)
            vkDestroyImageView(*device_, view, nullptr);
    }
} // namespace saltus::vulkan

