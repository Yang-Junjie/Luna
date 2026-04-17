#ifndef LUNA_RHI_GLPIPELINECACHE_H
#define LUNA_RHI_GLPIPELINECACHE_H
#include "GLCommon.h"
#include "Pipeline.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace luna::RHI {
struct GLProgramBinary {
    GLenum format;
    std::vector<uint8_t> data;
};

class LUNA_RHI_API GLPipelineCache final : public PipelineCache {
public:
    GLPipelineCache(const std::string& cacheDir = "");
    static Ref<GLPipelineCache> Create(const std::string& cacheDir = "");
    ~GLPipelineCache() override;

    std::vector<uint8_t> GetData() const override;
    void Merge(std::span<const Ref<PipelineCache>> srcCaches) override;

    bool LoadProgram(GLuint program, const std::string& key);
    bool SaveProgram(GLuint program, const std::string& key);

    void SaveToDisk();
    void LoadFromDisk();

    static bool IsProgramBinarySupported();

private:
    std::string m_cacheDir;
    std::unordered_map<std::string, GLProgramBinary> m_cache;
};
} // namespace luna::RHI

#endif
