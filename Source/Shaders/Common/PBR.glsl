#ifndef PBR_GLSL
#define PBR_GLSL

#ifndef SHADER_STAGE
#define SHADER_STAGE vertex
#pragma shader_stage(vertex)
void main() {}
#endif

#include "Common/Constants.glsl"
#include "Common/Common.glsl"
#include "Common/Random.glsl"
#include "Common/MonteCarlo.glsl"

#define DIELECTRIC_F0 vec3(0.04)

vec3 Diffuse_Lambert(vec3 baseColor)
{
    return baseColor * INVERSE_PI;
}

vec3 F_Schlick(vec3 F0, float VoH)
{
    const float Fc = pow(1 - VoH, 5.0);
    return F0 + (1 - F0) * Fc;
}

float D_GGX(float a2, float NoH)
{
    const float d = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * d * d);
}

float G_Schlick(float a, float NoV, float NoL)
{
    const float k = a * 0.5;

    const float ggxV = NoV * (1 - k) + k;
    const float ggxL = NoL * (1 - k) + k;

    return NoV / (ggxV * ggxL);
}

vec3 ImportanceSampleGGX(vec2 E, float a2)
{
    const float phi = 2.0 * PI * E.x;
    const float cosTheta = sqrt((1.0 - E.y) / (1.0 + (a2 - 1.0) * E.y));
    const float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;
    
    return H;
}

float ImportancePdfGGX(float cosTheta, float a2)
{
    return cosTheta * D_GGX(a2, cosTheta);
}

#endif