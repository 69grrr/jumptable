#include "saltus/buffer.hh"
#include <stdexcept>

namespace saltus
{
    BufferCreateInfo buffer_from_byte_array(
        BufferUsages usages,
        BufferAccessHint access_hint,
        const ByteArray &data
    ) {
        return {
            .usages = usages,
            .access_hint = access_hint,
            .size = data.size(),
            .data = data.data(),
        };
    }

    BufferUsages BufferUsages::with_uniform()
    {
        this->uniform = true;
        return *this;
    }
    BufferUsages BufferUsages::with_index()
    {
        this->index = true;
        return *this;
    }
    BufferUsages BufferUsages::with_vertex()
    {
        this->vertex = true;
        return *this;
    }

    Buffer::Buffer(BufferCreateInfo info):
        usages_(info.usages), access_hint_(info.access_hint), size_(info.size)
    {
        if (info.size == 0)
        {
            throw std::runtime_error("Cannot create a zero-sized buffer");
        }
    }

    Buffer::~Buffer()
    { }

    BufferUsages Buffer::usages() const
    {
        return usages_;
    }

    BufferAccessHint Buffer::access_hint() const
    {
        return access_hint_;
    }

    std::size_t Buffer::size() const
    {
        return size_;
    }
} // namespace saltus