#ifndef CACAO_GLPIPELINECACHE_H
#define CACAO_GLPIPELINECACHE_H
#include "Pipeline.h"
#include "GLCommon.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace Cacao
{
    struct GLProgramBinary
    {
        GLenum format;
        std::vector<uint8_t> data;
    };

    class CACAO_API GLPipelineCache final : public PipelineCache
    {
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
}

#endif
