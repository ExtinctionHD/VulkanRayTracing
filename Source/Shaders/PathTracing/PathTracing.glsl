#ifndef PATH_TRACING_GLSL
#define PATH_TRACING_GLSL

#ifndef SHADER_STAGE
#define SHADER_STAGE raygen
#pragma shader_stage(raygen)
#extension GL_NV_ray_tracing : require
void main() {}
#endif

vec2 GetUV()
{
    return vec2(gl_LaunchIDNV.xy) / vec2(gl_LaunchSizeNV.xy - 1);
}

vec2 Lerp(vec2 a, vec2 b, vec2 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec3 Lerp(vec3 a, vec3 b, vec3 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec4 Lerp(vec4 a, vec4 b, vec4 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

#endif