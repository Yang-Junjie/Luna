#version 450

layout(set = 0, binding = 0) uniform sampler2D u_albedo_texture;

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec3 frag_normal;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;

void main()
{
    out_albedo = texture(u_albedo_texture, frag_uv);
    out_normal = vec4(normalize(frag_normal) * 0.5 + 0.5, 1.0);
}
