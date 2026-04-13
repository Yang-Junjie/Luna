#version 450

layout(set = 0, binding = 0) uniform sampler2D u_gbuffer_albedo;
layout(set = 0, binding = 1) uniform sampler2D u_gbuffer_normal;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

void main()
{
    vec4 albedo = texture(u_gbuffer_albedo, frag_uv);
    if (albedo.a <= 0.001) {
        out_color = vec4(0.10, 0.10, 0.12, 1.0);
        return;
    }

    vec3 normal = normalize(texture(u_gbuffer_normal, frag_uv).xyz * 2.0 - 1.0);
    vec3 light_direction = normalize(vec3(0.45, 0.8, 0.35));
    float diffuse = max(dot(normal, light_direction), 0.15);
    out_color = vec4(albedo.rgb * diffuse, 1.0);
}
