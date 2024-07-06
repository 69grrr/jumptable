#pragma once

#include <vulkan/vulkan_core.h>
#include "saltus/vulkan/vulkan_device.hh"

namespace saltus::vulkan
{
    class RawVulkanBuffer
    {
    public:
        RawVulkanBuffer(
            std::shared_ptr<VulkanDevice> device,
            VkDeviceSize size,
            VkBufferUsageFlags usage_flags
        );
        ~RawVulkanBuffer();

        operator VkBuffer() const;
        VkBuffer buffer_handle() const;

        void alloc(VkMemoryPropertyFlags memory_properties);
        void *map(VkDeviceSize offset, VkDeviceSize size);
        void unmap();

    private:
        std::shared_ptr<VulkanDevice> device_;

        VkBuffer buffer_ = VK_NULL_HANDLE;
        VkDeviceMemory memory_ = VK_NULL_HANDLE;

        uint32_t find_mem_type(uint32_t type_filter_, VkMemoryPropertyFlags properties);
    };
} // namespace saltus::vulkan