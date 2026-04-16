#ifndef CACAO_PIPELINEDEFS_H
#define CACAO_PIPELINEDEFS_H
#include "Core.h"
namespace Cacao
{
    enum class PolygonMode { Fill, Line, Point };
    enum class CullMode { None, Front, Back, FrontAndBack };
    enum class FrontFace { CounterClockwise, Clockwise };
    enum class CompareOp { Never, Less, Equal, LessOrEqual, Greater, NotEqual, GreaterOrEqual, Always };
    enum class StencilOp
    {
        Keep, Zero, Replace, IncrementAndClamp, DecrementAndClamp, Invert, IncrementWrap, DecrementWrap
    };
    enum class BlendFactor
    {
        Zero, One, SrcColor, OneMinusSrcColor, DstColor, OneMinusDstColor, SrcAlpha, OneMinusSrcAlpha, DstAlpha,
        OneMinusDstAlpha, ConstantColor, OneMinusConstantColor, SrcAlphaSaturate
    };
    enum class BlendOp { Add, Subtract, ReverseSubtract, Min, Max };
    enum class LogicOp
    {
        Clear, And, AndReverse, Copy, AndInverted, NoOp, Xor, Or, Nor, Equiv, Invert, OrReverse, CopyInverted,
        OrInverted, Nand, Set
    };
    enum class VertexInputRate { Vertex, Instance };
    struct VertexInputAttribute
    {
        uint32_t Location = 0;
        uint32_t Binding = 0;
        Format Format = Format::RGBA32_FLOAT;
        uint32_t Offset = 0;
        std::string SemanticName = "TEXCOORD";
        uint32_t SemanticIndex = UINT32_MAX;
    };
    struct VertexInputBinding
    {
        uint32_t Binding = 0;
        uint32_t Stride = 0;
        VertexInputRate InputRate = VertexInputRate::Vertex;
    };
    enum class PrimitiveTopology
    {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        TriangleFan,
        PatchList
    };
    struct InputAssemblyState
    {
        PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
        bool PrimitiveRestartEnable = false;
        uint32_t PatchControlPoints = 3;
    };
    struct RasterizationState
    {
        bool DepthClampEnable = false;
        bool RasterizerDiscardEnable = false;
        PolygonMode PolygonMode = PolygonMode::Fill;
        CullMode CullMode = CullMode::Back;
        FrontFace FrontFace = FrontFace::CounterClockwise;
        bool DepthBiasEnable = false;
        float DepthBiasConstantFactor = 0.0f;
        float DepthBiasClamp = 0.0f;
        float DepthBiasSlopeFactor = 0.0f;
        float LineWidth = 1.0f;
    };
    enum class ColorComponentFlags : uint32_t
    {
        R = 1 << 0,
        G = 1 << 1,
        B = 1 << 2,
        A = 1 << 3,
        All = R | G | B | A
    };
    inline ColorComponentFlags operator|(ColorComponentFlags a, ColorComponentFlags b)
    {
        return static_cast<ColorComponentFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    inline ColorComponentFlags& operator|=(ColorComponentFlags& a, ColorComponentFlags b)
    {
        a = a | b;
        return a;
    }
    inline bool operator&(ColorComponentFlags a, ColorComponentFlags b)
    {
        return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
    }
    struct StencilOpState
    {
        StencilOp FailOp = StencilOp::Keep;
        StencilOp PassOp = StencilOp::Keep;
        StencilOp DepthFailOp = StencilOp::Keep;
        CompareOp CompareOp = CompareOp::Always;
        uint32_t CompareMask = 0xFF;
        uint32_t WriteMask = 0xFF;
        uint32_t Reference = 0;
    };
    struct DepthStencilState
    {
        bool DepthTestEnable = true;
        bool DepthWriteEnable = true;
        CompareOp DepthCompareOp = CompareOp::Less;
        bool StencilTestEnable = false;
        StencilOpState Front;
        StencilOpState Back;
        uint32_t StencilReadMask = 0xFF;
        uint32_t StencilWriteMask = 0xFF;
    };
    struct ColorBlendAttachmentState
    {
        bool BlendEnable = false;
        BlendFactor SrcColorBlendFactor = BlendFactor::One;
        BlendFactor DstColorBlendFactor = BlendFactor::Zero;
        BlendOp ColorBlendOp = BlendOp::Add;
        BlendFactor SrcAlphaBlendFactor = BlendFactor::One;
        BlendFactor DstAlphaBlendFactor = BlendFactor::Zero;
        BlendOp AlphaBlendOp = BlendOp::Add;
        ColorComponentFlags ColorWriteMask = ColorComponentFlags::All;
    };
    struct ColorBlendState
    {
        bool LogicOpEnable = false;
        LogicOp LogicOp = LogicOp::Copy;
        std::vector<ColorBlendAttachmentState> Attachments;
        float BlendConstants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    };
    struct PushConstantRange
    {
        ShaderStage StageFlags;
        uint32_t Offset;
        uint32_t Size;
    };
}
#endif
