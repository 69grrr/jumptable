#pragma once

#include <memory>
#include <vector>

#include "saltus/fwd.hh"

namespace saltus
{
    struct InstanceGroupCreateInfo {
        std::shared_ptr<ShaderPack> shader_pack;
        std::shared_ptr<Mesh> mesh;
        std::vector<std::shared_ptr<BindGroup>> bind_groups;
    };

    class InstanceGroup
    {
    public:
        virtual ~InstanceGroup() = 0;
        InstanceGroup(const InstanceGroup &x) = delete;
        const InstanceGroup &operator =(const InstanceGroup &x) = delete;

        const std::shared_ptr<ShaderPack> &shader_pack() const;
        const std::shared_ptr<Mesh> &mesh() const;
        const std::vector<std::shared_ptr<BindGroup>> &bind_groups() const;
        
    protected:
        InstanceGroup(InstanceGroupCreateInfo info);

    private:
        std::shared_ptr<ShaderPack> shader_pack__;
        std::shared_ptr<Mesh> mesh_;
        std::vector<std::shared_ptr<BindGroup>> bind_groups_;
    };
} // namespace saltus
