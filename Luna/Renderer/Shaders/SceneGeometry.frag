#version 450

layout(set = 0, binding = 0) uniform sampler2D u_albedo_texture;

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec3 frag_normal;

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 albedo = texture(u_albedo_texture, frag_uv);
    vec3 normal = normalize(frag_normal);
    vec3 light_direction = normalize(vec3(0.45, 0.8, 0.35));
    float diffuse = max(dot(normal, light_direction), 0.15);
    out_color = vec4(albedo.rgb * diffuse, albedo.a);
}
