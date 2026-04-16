#include "Impls/OpenGL/GLPipelineCache.h"
#include <fstream>
#include <filesystem>

namespace Cacao
{
    GLPipelineCache::GLPipelineCache(const std::string& cacheDir)
        : m_cacheDir(cacheDir)
    {
        if (!m_cacheDir.empty())
            LoadFromDisk();
    }

    Ref<GLPipelineCache> GLPipelineCache::Create(const std::string& cacheDir)
    {
        return std::make_shared<GLPipelineCache>(cacheDir);
    }

    GLPipelineCache::~GLPipelineCache()
    {
        if (!m_cacheDir.empty())
            SaveToDisk();
    }

    std::vector<uint8_t> GLPipelineCache::GetData() const
    {
        return {};
    }

    void GLPipelineCache::Merge(std::span<const Ref<PipelineCache>>)
    {
    }

    bool GLPipelineCache::IsProgramBinarySupported()
    {
        GLint numFormats = 0;
        glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &numFormats);
        return numFormats > 0;
    }

    bool GLPipelineCache::LoadProgram(GLuint program, const std::string& key)
    {
        auto it = m_cache.find(key);
        if (it == m_cache.end()) return false;

        glProgramBinary(program, it->second.format,
                        it->second.data.data(),
                        static_cast<GLsizei>(it->second.data.size()));

        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        return linked == GL_TRUE;
    }

    bool GLPipelineCache::SaveProgram(GLuint program, const std::string& key)
    {
        GLint length = 0;
        glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &length);
        if (length <= 0) return false;

        GLProgramBinary binary;
        binary.data.resize(length);
        glGetProgramBinary(program, length, nullptr, &binary.format, binary.data.data());

        m_cache[key] = std::move(binary);
        return true;
    }

    void GLPipelineCache::SaveToDisk()
    {
        if (m_cacheDir.empty()) return;
        std::filesystem::create_directories(m_cacheDir);

        for (const auto& [key, binary] : m_cache)
        {
            std::string path = m_cacheDir + "/" + key + ".bin";
            std::ofstream ofs(path, std::ios::binary);
            if (!ofs) continue;

            ofs.write(reinterpret_cast<const char*>(&binary.format), sizeof(binary.format));
            uint32_t size = static_cast<uint32_t>(binary.data.size());
            ofs.write(reinterpret_cast<const char*>(&size), sizeof(size));
            ofs.write(reinterpret_cast<const char*>(binary.data.data()), size);
        }
    }

    void GLPipelineCache::LoadFromDisk()
    {
        if (m_cacheDir.empty() || !std::filesystem::exists(m_cacheDir)) return;

        for (const auto& entry : std::filesystem::directory_iterator(m_cacheDir))
        {
            if (entry.path().extension() != ".bin") continue;

            std::ifstream ifs(entry.path(), std::ios::binary);
            if (!ifs) continue;

            GLProgramBinary binary;
            ifs.read(reinterpret_cast<char*>(&binary.format), sizeof(binary.format));
            uint32_t size = 0;
            ifs.read(reinterpret_cast<char*>(&size), sizeof(size));
            binary.data.resize(size);
            ifs.read(reinterpret_cast<char*>(binary.data.data()), size);

            std::string key = entry.path().stem().string();
            m_cache[key] = std::move(binary);
        }
    }
}
