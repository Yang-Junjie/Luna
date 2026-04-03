#pragma once
#include "Types.h"

#include <map>
#include <string>
#include <vector>

namespace luna {
struct ShaderMember {
    std::string name; // 变量名（例如 "albedoColor", "roughness"）

    // --- 最核心的两个空间属性 ---
    uint32_t offset; // 偏移量：该变量在 UBO Buffer 中的起始字节位置
    uint32_t size;   // 大小：该变量实际占用的字节数（包含对齐填充）

    // --- 进阶/辅助属性 ---
    ShaderDataType type;    // 数据类型：用于在向 GPU 传数据前做 assert 校验
    uint32_t arrayElements; // 数组长度：如果不是数组则为 1
};

struct ShaderReflectionData {
    std::string name;  // 变量名（用于 OpenGL 和 调试）
    ResourceType type; // 资源类型（UBO, SampledImage, StorageBuffer 等）
    uint32_t binding;  // 绑定点索引 (layout(binding = X))
    uint32_t count;    // 数组长度（如果是单体则为 1）
    uint32_t size;     // 如果是 Buffer，记录其字节大小
    // 后面会提到：用于处理 UBO 内部成员的元数据
    std::vector<ShaderMember> members;
};

class Shader {
public:
    virtual ~Shader() = default;
    using ReflectionMap = std::map<uint32_t, std::vector<ShaderReflectionData>>;
    virtual ShaderType getType() const = 0;

    const ReflectionMap& getReflectionMap() const
    {
        return m_reflectionMap;
    }

protected:
    ReflectionMap m_reflectionMap;
};
} // namespace luna
