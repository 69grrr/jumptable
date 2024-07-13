#include "saltus/vulkan/raw_vulkan_buffer.hh"
#include <vulkan/vulkan_core.h>
#include "saltus/vulkan/raw_command_buffer.hh"

namespace saltus::vulkan
{
    RawVulkanBuffer::RawVulkanBuffer(
        std::shared_ptr<VulkanDevice> device,
        VkDeviceSize size,
        VkBufferUsageFlags usage_flags
    ): device_(device), size_(size) {
        VkBufferCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        create_info.size = size;
        create_info.usage = usage_flags;
        uint32_t families[] = {
            device->get_physical_device_family_indices().graphicsFamily.value(),
            device->get_physical_device_family_indices().transferFamily.value(),
        };
        if (families[0] == families[1])
        {
            create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            create_info.queueFamilyIndexCount = 1;
            create_info.pQueueFamilyIndices = families;
        }
        else {
            create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = families;
        }

        VkResult result =
            vkCreateBuffer(*device, &create_info, nullptr, &buffer_);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Could not create buffer");
    }

    RawVulkanBuffer::~RawVulkanBuffer()
    {
        vkDestroyBuffer(*device_, buffer_, nullptr);
        if (memory_ != nullptr)
            vkFreeMemory(*device_, memory_, nullptr);
    }

    VkDeviceSize RawVulkanBuffer::size() const
    {
        return size_;
    }

    RawVulkanBuffer::operator VkBuffer() const
    {
        return buffer_;
    }
    
    VkBuffer RawVulkanBuffer::buffer_handle() const
    {
        return buffer_;
    }

    void RawVulkanBuffer::alloc(VkMemoryPropertyFlags memory_properties)
    {
        if (memory_ != VK_NULL_HANDLE)
            throw std::runtime_error("Buffer already allocated");

        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(*device_, buffer_, &mem_reqs);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = device_->find_mem_type(
            mem_reqs.memoryTypeBits,
            memory_properties
        );

        VkResult result =
            vkAllocateMemory(*device_, &alloc_info, nullptr, &memory_);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Buffer allocation failed");

        vkBindBufferMemory(*device_, buffer_, memory_, 0);
    }

    void *RawVulkanBuffer::map(VkDeviceSize offset, VkDeviceSize size)
    {
        if (memory_ == VK_NULL_HANDLE)
            throw std::runtime_error("Cannot map non allocated buffer");

        void *data = nullptr;
        VkResult result = vkMapMemory(*device_, memory_, offset, size, 0, &data);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Buffer map error");

        return data;
    }

    void RawVulkanBuffer::unmap()
    {
        if (memory_ == VK_NULL_HANDLE)
            throw std::runtime_error("Cannot unmap non allocated buffer");

        vkUnmapMemory(*device_, memory_);
    }

    void RawVulkanBuffer::copy(
        RawVulkanBuffer &src_buffer,
        VkDeviceSize src_offset,
        VkDeviceSize dst_offset,
        VkDeviceSize size,
        bool wait
    ) {
        if (src_offset > src_buffer.size())
            throw std::range_error("src offset is too big");
        if (dst_offset > size_)
            throw std::range_error("dst offset is too big");
        if (size == VK_WHOLE_SIZE)
            size = std::min(src_buffer.size() - src_offset, size_ - dst_offset);

        RawCommandBuffer rcb(device_);
        rcb.begin();

        VkBufferCopy copy{};
        copy.srcOffset = src_offset;
        copy.dstOffset = dst_offset;
        copy.size = size;

        vkCmdCopyBuffer(
            rcb.handle(), src_buffer.buffer_handle(), buffer_,
            1, &copy
        );

        if (wait)
        {
            RawVulkanFence fence(device_);
            rcb.end_and_submit(device_->graphics_queue(), &fence);
            fence.wait();
        }
        else {
            rcb.end_and_submit(device_->graphics_queue());
        }
    }
} // namespace saltus::vulkan